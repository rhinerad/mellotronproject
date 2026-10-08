#include "SamplerEngine.h"

#include <algorithm>
#include <cmath>

namespace
{
    /** Exposes an in-memory buffer as an AudioFormatReader, so generated audio can be
        handed to SamplerSound exactly like a file. */
    class BufferReader final : public juce::AudioFormatReader
    {
    public:
        BufferReader (const juce::AudioBuffer<float>& source, double rate)
            : AudioFormatReader (nullptr, "Memory"), buffer (source)
        {
            sampleRate = rate;
            bitsPerSample = 32;
            lengthInSamples = buffer.getNumSamples();
            numChannels = (unsigned int) buffer.getNumChannels();
            usesFloatingPointData = true;
        }

        bool readSamples (int* const* destChannels, int numDestChannels, int startOffsetInDestBuffer,
                          juce::int64 startSampleInFile, int numSamples) override
        {
            clearSamplesBeyondAvailableLength (destChannels, numDestChannels, startOffsetInDestBuffer,
                                               startSampleInFile, numSamples, lengthInSamples);

            for (int ch = 0; ch < numDestChannels; ++ch)
            {
                if (destChannels[ch] == nullptr)
                    continue;

                auto* dest = reinterpret_cast<float*> (destChannels[ch]) + startOffsetInDestBuffer;

                if (ch < buffer.getNumChannels() && numSamples > 0)
                    juce::FloatVectorOperations::copy (dest, buffer.getReadPointer (ch, (int) startSampleInFile), numSamples);
                else if (numSamples > 0)
                    juce::FloatVectorOperations::clear (dest, numSamples);
            }

            return true;
        }

    private:
        const juce::AudioBuffer<float>& buffer;
    };

    /** Band-limited step correction for the sawtooth oscillators below. */
    double polyBlep (double t, double dt)
    {
        if (t < dt)        { t /= dt;               return t + t - t * t - 1.0; }
        if (t > 1.0 - dt)  { t = (t - 1.0) / dt;    return t * t + t + t + 1.0; }
        return 0.0;
    }

    /** Topology-preserving state-variable low-pass (Zavalishin / Simper). */
    struct LowPass
    {
        void setup (double cutoff, double sampleRate, double q)
        {
            auto g = std::tan (juce::MathConstants<double>::pi * cutoff / sampleRate);
            auto k = 1.0 / q;
            a1 = 1.0 / (1.0 + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        double process (double x)
        {
            auto v3 = x - ic2;
            auto v1 = a1 * ic1 + a2 * v3;
            auto v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0 * v1 - ic1;
            ic2 = 2.0 * v2 - ic2;
            return v2;
        }

        double a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
    };

    /** A slow-bowed, three-saw "tape strings" note, used when no sample bank is installed. */
    juce::AudioBuffer<float> renderStringNote (int midiNote, double sampleRate, juce::Random& rng)
    {
        const auto numSamples = (int) (SamplerEngine::maxSampleSeconds * sampleRate);
        juce::AudioBuffer<float> out (1, numSamples);
        auto* dest = out.getWritePointer (0);

        const auto freq = 440.0 * std::pow (2.0, (midiNote - 69) / 12.0);
        const double detuneCents[] = { -9.0, 0.0, 7.0 };
        double phase[3], inc[3];

        for (int v = 0; v < 3; ++v)
        {
            phase[v] = rng.nextDouble();
            inc[v] = freq * std::pow (2.0, detuneCents[v] / 1200.0) / sampleRate;
        }

        LowPass lp1, lp2;
        const auto cutoff = juce::jlimit (600.0, 4500.0, freq * 5.0);
        lp1.setup (cutoff, sampleRate, 0.6);
        lp2.setup (cutoff * 1.4, sampleRate, 0.7);

        const auto attack = 0.08 * sampleRate;
        const auto fade = 0.05 * sampleRate;
        float peak = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            double x = 0.0;

            for (int v = 0; v < 3; ++v)
            {
                x += 2.0 * phase[v] - 1.0 - polyBlep (phase[v], inc[v]);
                phase[v] += inc[v];
                if (phase[v] >= 1.0)
                    phase[v] -= 1.0;
            }

            const auto t = i / sampleRate;
            auto env = 0.8 + 0.2 * std::exp (-t / 1.5);   // bow pressure eases off

            if (i < attack)
                env *= 0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * i / attack);
            if (i > numSamples - fade)
                env *= (numSamples - i) / fade;

            const auto y = (float) (lp2.process (lp1.process (x)) * env);
            dest[i] = y;
            peak = std::max (peak, std::abs (y));
        }

        if (peak > 0.0f)
            out.applyGain (0.25f / peak);

        return out;
    }

    int noteFromToken (const juce::String& token)
    {
        static constexpr int pitchClasses[] = { 9, 11, 0, 2, 4, 5, 7 };   // A B C D E F G

        auto s = token.toLowerCase();
        if (s.length() < 2 || s.length() > 3)
            return -1;

        auto letter = s[0];
        if (letter < 'a' || letter > 'g')
            return -1;

        int pc = pitchClasses[letter - 'a'];
        int pos = 1;

        if (s[pos] == '#')       { ++pc; ++pos; }
        else if (s[pos] == 'b' && s.length() == 3) { --pc; ++pos; }

        if (pos != s.length() - 1 || ! juce::CharacterFunctions::isDigit (s[pos]))
            return -1;

        const int octave = s[pos] - '0';
        const int note = (octave + 1) * 12 + pc;
        return juce::isPositiveAndBelow (note, 128) ? note : -1;
    }
}

//==============================================================================
SamplerEngine::TapeSound::TapeSound (const juce::String& soundName, juce::AudioFormatReader& source,
                                     const juce::BigInteger& notes, int rootNote)
    : SamplerSound (soundName, source, notes, rootNote, 0.015, 0.2, maxSampleSeconds),
      sourceRate (source.sampleRate),
      root (rootNote)
{
}

//==============================================================================
SamplerEngine::SamplerEngine()
{
    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new juce::SamplerVoice());

    synth.setNoteStealingEnabled (true);
}

void SamplerEngine::prepare (double sampleRate)
{
    hostSampleRate = sampleRate;
    synth.setCurrentPlaybackSampleRate (sampleRate);
}

void SamplerEngine::setEnvelope (float attackMs, float releaseMs) noexcept
{
    attackSeconds  = attackMs  * 0.001f;
    releaseSeconds = releaseMs * 0.001f;
}

void SamplerEngine::render (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi)
{
    const juce::ScopedLock sl (synth.getLock());

    // SamplerVoice clocks its ADSR at the sample's own rate, so scale the times to match the host
    for (int i = 0; i < synth.getNumSounds(); ++i)
    {
        auto* sound = static_cast<TapeSound*> (synth.getSound (i).get());
        const auto scale = (float) (hostSampleRate / sound->sourceRate);
        sound->setEnvelopeParameters ({ attackSeconds * scale, 0.0f, 1.0f, releaseSeconds * scale });
    }

    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());
}

int SamplerEngine::getNumActiveVoices() const noexcept
{
    int active = 0;

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (synth.getVoice (i)->isVoiceActive())
            ++active;

    return active;
}

void SamplerEngine::installBank (Bank& bank)
{
    juce::ReferenceCountedArray<juce::SynthesiserSound> previous;

    {
        const juce::ScopedLock sl (synth.getLock());

        for (int i = 0; i < synth.getNumSounds(); ++i)
            previous.add (synth.getSound (i));

        synth.allNotesOff (0, false);
        synth.clearSounds();

        for (auto* sound : bank.sounds)
            synth.addSound (sound);
    }

    // `previous` releases the old sample data here, on the calling thread rather than the audio thread
}

//==============================================================================
int SamplerEngine::parseRootNote (const juce::String& fileNameWithoutExtension)
{
    juce::StringArray tokens;
    tokens.addTokens (fileNameWithoutExtension, " _-.,()[]{}", "");
    tokens.removeEmptyStrings();

    for (int i = tokens.size(); --i >= 0;)
        if (auto note = noteFromToken (tokens[i]); note >= 0)
            return note;

    for (int i = tokens.size(); --i >= 0;)
        if (tokens[i].length() <= 3 && tokens[i].containsOnly ("0123456789"))
            if (auto note = tokens[i].getIntValue(); juce::isPositiveAndBelow (note, 128))
                return note;

    return -1;
}

juce::String SamplerEngine::noteName (int midiNote)
{
    // octaveForMiddleC = 4 -> scientific pitch, matching the file naming convention
    return juce::MidiMessage::getMidiNoteName (midiNote, true, true, 4);
}

bool SamplerEngine::folderHasSamples (const juce::File& folder)
{
    return folder.isDirectory()
        && ! folder.findChildFiles (juce::File::findFiles, false, getWildcard(), juce::File::FollowSymlinks::no).isEmpty();
}

juce::Array<juce::File> SamplerEngine::getDefaultSampleFolders()
{
    const auto binaryDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();

    return {
        binaryDir.getSiblingFile ("Resources").getChildFile ("Samples"),   // VST3 bundle, macOS app
        binaryDir.getChildFile ("Samples"),                                 // Windows/Linux standalone
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("TapeBank").getChildFile ("Samples")
    };
}

SamplerEngine::Bank SamplerEngine::loadFolder (const juce::File& folder, juce::AudioFormatManager& formats,
                                               const AbortCheck& shouldAbort)
{
    Bank bank;
    bank.name = folder.getFileName();

    auto files = folder.findChildFiles (juce::File::findFiles, false, getWildcard(), juce::File::FollowSymlinks::no);
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileName().compareNatural (b.getFileName()) < 0; });

    if (files.isEmpty())
    {
        bank.detail = "No .wav/.aif/.flac files in this folder";
        return bank;
    }

    struct Entry
    {
        juce::File file;
        int root;
        std::unique_ptr<juce::AudioFormatReader> reader;
    };

    std::vector<Entry> entries;
    int skipped = 0;

    for (auto& f : files)
    {
        if (auto root = parseRootNote (f.getFileNameWithoutExtension()); root >= 0)
            entries.push_back ({ f, root, nullptr });
        else
            ++skipped;
    }

    if (entries.empty())
    {
        skipped = 0;

        if (files.size() == 1)
            entries.push_back ({ files[0], 60, nullptr });
        else
            for (int i = 0; i < files.size() && lowestNote + i <= highestNote; ++i)
                entries.push_back ({ files[i], lowestNote + i, nullptr });
    }

    std::stable_sort (entries.begin(), entries.end(), [] (const Entry& a, const Entry& b) { return a.root < b.root; });

    // One sample per root; drop unreadable files before working out the key ranges
    std::vector<Entry> usable;

    for (auto& e : entries)
    {
        if (! usable.empty() && usable.back().root == e.root)
        {
            ++skipped;
            continue;
        }

        e.reader.reset (formats.createReaderFor (e.file));

        if (e.reader == nullptr || e.reader->lengthInSamples <= 0 || e.reader->sampleRate <= 0.0)
        {
            ++skipped;
            continue;
        }

        usable.push_back (std::move (e));
    }

    for (size_t i = 0; i < usable.size(); ++i)
    {
        if (shouldAbort())
            return {};

        auto& e = usable[i];
        const int lo = std::max (lowestNote,  i == 0 ? 0 : (usable[i - 1].root + e.root) / 2 + 1);
        const int hi = std::min (highestNote, i + 1 == usable.size() ? 127 : (e.root + usable[i + 1].root) / 2);

        if (lo > hi)
            continue;

        juce::BigInteger notes;
        notes.setRange (lo, hi - lo + 1, true);
        bank.sounds.add (new TapeSound (e.file.getFileNameWithoutExtension(), *e.reader, notes, e.root));
    }

    if (bank.sounds.isEmpty())
    {
        bank.detail = "No samples fall within C2-C6";
        return bank;
    }

    bank.detail = juce::String (bank.sounds.size()) + (bank.sounds.size() == 1 ? " sample" : " samples")
                + "  |  roots " + noteName (bank.sounds.getFirst()->root) + "-" + noteName (bank.sounds.getLast()->root);

    if (skipped > 0)
        bank.detail << "  |  " << skipped << " skipped";

    return bank;
}

SamplerEngine::Bank SamplerEngine::makeBuiltInBank (const AbortCheck& shouldAbort)
{
    constexpr double rate = 44100.0;
    constexpr int spacing = 3;   // one generated "tape" every minor third, each covering root +/- 1

    Bank bank;
    bank.name = "Built-in Tape Strings";
    bank.detail = "Synthetic fallback  |  load a sample folder for real tapes";

    juce::Random rng (0x7a9eba7c);

    for (int root = lowestNote; root <= highestNote; root += spacing)
    {
        if (shouldAbort())
            return {};

        const auto audio = renderStringNote (root, rate, rng);
        BufferReader reader (audio, rate);

        juce::BigInteger notes;
        const int lo = std::max (lowestNote,  root - 1);
        const int hi = std::min (highestNote, root + 1);
        notes.setRange (lo, hi - lo + 1, true);

        bank.sounds.add (new TapeSound ("Strings " + noteName (root), reader, notes, root));
    }

    return bank;
}
