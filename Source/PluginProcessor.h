#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "SamplerEngine.h"
#include "TapeProcessor.h"

class TapeBankAudioProcessor final : public juce::AudioProcessor
{
public:
    TapeBankAudioProcessor();
    ~TapeBankAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                         { return true; }

    const juce::String getName() const override             { return JucePlugin_Name; }
    bool acceptsMidi() const override                       { return true; }
    bool producesMidi() const override                      { return false; }
    bool isMidiEffect() const override                      { return false; }
    double getTailLengthSeconds() const override            { return 2.0; }

    int getNumPrograms() override                           { return 1; }
    int getCurrentProgram() override                        { return 0; }
    void setCurrentProgram (int) override                   {}
    const juce::String getProgramName (int) override        { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    /** Loads a folder of samples on a background thread. An empty File means the default bank. */
    void loadSampleFolder (const juce::File& folder);

    struct BankStatus
    {
        juce::String name, detail;
        bool loading = false;
    };

    BankStatus getBankStatus() const;
    /** The user-chosen folder, or an empty File when the default bank is in use. */
    juce::File getActiveFolder() const;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

private:
    void startLoad (const juce::File& folder, bool restoringState);

    SamplerEngine sampler;
    TapeProcessor tape;
    juce::AudioFormatManager formatManager;

    std::atomic<float>* drive = nullptr;
    std::atomic<float>* wowDepth = nullptr;
    std::atomic<float>* flutterDepth = nullptr;
    std::atomic<float>* tone = nullptr;
    std::atomic<float>* attack = nullptr;
    std::atomic<float>* release = nullptr;
    std::atomic<float>* noise = nullptr;
    std::atomic<float>* output = nullptr;

    juce::SmoothedValue<float> outputGain;

    mutable juce::CriticalSection statusLock;
    BankStatus status;
    juce::File activeFolder;
    std::atomic<int> latestLoadRequest { 0 };

    // Declared last so its thread stops before anything a job touches is destroyed
    juce::ThreadPool loader { juce::ThreadPoolOptions{}.withThreadName ("TapeBank loader").withNumberOfThreads (1) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeBankAudioProcessor)
};
