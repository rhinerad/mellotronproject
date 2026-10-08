#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"

namespace
{
    const juce::Identifier samplesFolderProperty { "samplesFolder" };
}

TapeBankAudioProcessor::TapeBankAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TapeBankState", Params::createLayout())
{
    formatManager.registerBasicFormats();

    drive        = apvts.getRawParameterValue (ParamIDs::drive);
    wowDepth     = apvts.getRawParameterValue (ParamIDs::wowDepth);
    flutterDepth = apvts.getRawParameterValue (ParamIDs::flutterDepth);
    tone         = apvts.getRawParameterValue (ParamIDs::tone);
    attack       = apvts.getRawParameterValue (ParamIDs::attack);
    release      = apvts.getRawParameterValue (ParamIDs::release);
    noise        = apvts.getRawParameterValue (ParamIDs::noise);
    output       = apvts.getRawParameterValue (ParamIDs::output);

    // Start on the default bank right away; a restored project's folder supersedes it
    startLoad ({}, false);
}

TapeBankAudioProcessor::~TapeBankAudioProcessor()
{
    ++latestLoadRequest;   // makes any running load bail out early
    loader.removeAllJobs (true, 10000);
}

//==============================================================================
void TapeBankAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampler.prepare (sampleRate);

    tape.setSettings ({ drive->load(), wowDepth->load(), flutterDepth->load(), tone->load(), noise->load() >= 0.5f });
    tape.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    setLatencySamples (tape.getLatencySamples());

    outputGain.reset (sampleRate, 0.05);
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (output->load()));
}

void TapeBankAudioProcessor::releaseResources()
{
    tape.reset();
}

bool TapeBankAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return layouts.getMainInputChannelSet().isDisabled()
        && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void TapeBankAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numSamples = buffer.getNumSamples();

    buffer.clear();
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    sampler.setEnvelope (attack->load(), release->load());
    sampler.render (buffer, midi);

    tape.setActiveTapes (sampler.getNumActiveVoices());
    tape.setSettings ({ drive->load(), wowDepth->load(), flutterDepth->load(), tone->load(), noise->load() >= 0.5f });
    tape.process (buffer);

    outputGain.setTargetValue (juce::Decibels::decibelsToGain (output->load()));
    outputGain.applyGain (buffer, numSamples);
}

//==============================================================================
juce::AudioProcessorEditor* TapeBankAudioProcessor::createEditor()
{
    return new TapeBankAudioProcessorEditor (*this);
}

void TapeBankAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    {
        const juce::ScopedLock sl (statusLock);
        state.setProperty (samplesFolderProperty, activeFolder.getFullPathName(), nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TapeBankAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);
    const auto path = tree.getProperty (samplesFolderProperty).toString();
    tree.removeProperty (samplesFolderProperty, nullptr);
    apvts.replaceState (tree);

    const auto folder = juce::File::isAbsolutePath (path) ? juce::File (path) : juce::File();

    {
        // Hosts re-send state often (undo, preset browsing); don't reload a bank that's already in
        const juce::ScopedLock sl (statusLock);
        if (folder == activeFolder && ! status.loading)
            return;
    }

    startLoad (folder, true);
}

//==============================================================================
void TapeBankAudioProcessor::loadSampleFolder (const juce::File& folder)
{
    startLoad (folder, false);
}

TapeBankAudioProcessor::BankStatus TapeBankAudioProcessor::getBankStatus() const
{
    const juce::ScopedLock sl (statusLock);
    return status;
}

juce::File TapeBankAudioProcessor::getActiveFolder() const
{
    const juce::ScopedLock sl (statusLock);
    return activeFolder;
}

void TapeBankAudioProcessor::startLoad (const juce::File& folder, bool restoringState)
{
    const int requestId = ++latestLoadRequest;

    {
        const juce::ScopedLock sl (statusLock);
        status.loading = true;
        status.detail = "Loading " + (folder == juce::File() ? juce::String ("default bank") : folder.getFileName()) + "...";
    }

    loader.addJob ([this, requestId, folder, restoringState]
    {
        const auto shouldAbort = [this, requestId]
        {
            auto* job = juce::ThreadPoolJob::getCurrentThreadPoolJob();
            return latestLoadRequest.load() != requestId || (job != nullptr && job->shouldExit());
        };

        SamplerEngine::Bank bank;
        juce::String problem;

        if (folder != juce::File())
        {
            if (folder.isDirectory())
                bank = SamplerEngine::loadFolder (folder, formatManager, shouldAbort);
            else
                bank.detail = "Folder not found";

            if (shouldAbort())
                return;

            if (bank.sounds.isEmpty())
            {
                problem = "Couldn't load \"" + folder.getFileName() + "\": " + bank.detail;

                if (! restoringState)
                {
                    // A bad pick from the UI keeps whatever was already playing
                    const juce::ScopedLock sl (statusLock);
                    status.loading = false;
                    status.detail = problem;
                    return;
                }
            }
        }

        if (bank.sounds.isEmpty())
        {
            for (auto& candidate : SamplerEngine::getDefaultSampleFolders())
            {
                if (SamplerEngine::folderHasSamples (candidate))
                    bank = SamplerEngine::loadFolder (candidate, formatManager, shouldAbort);

                if (shouldAbort())
                    return;

                if (! bank.sounds.isEmpty())
                    break;
            }

            if (bank.sounds.isEmpty())
                bank = SamplerEngine::makeBuiltInBank (shouldAbort);
        }

        if (shouldAbort())
            return;

        sampler.installBank (bank);

        const juce::ScopedLock sl (statusLock);
        // A missing folder restored from a project is remembered, so re-saving doesn't lose it
        activeFolder = folder;
        status.loading = false;
        status.name = bank.name;
        status.detail = problem.isNotEmpty() ? problem + "  |  using " + bank.name : bank.detail;
    });
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TapeBankAudioProcessor();
}
