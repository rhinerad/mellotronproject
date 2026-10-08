#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <functional>
#include <vector>

/*  Polyphonic tape-replay sampler built on juce::Synthesiser + SamplerSound/SamplerVoice.

    Sample naming convention (see Samples/README.md):
      - The root note is read from the file name: a note name such as "C3", "F#2" or "Bb4"
        (scientific pitch, C4 = MIDI 60 = middle C), or a bare MIDI number such as "060".
        The last matching token wins, so "Strings MkII A3.wav" -> A3.
      - Each sample covers the keys nearest to its root, and the keyboard is limited to
        C2-C6 (MIDI 36-84). Sparse banks are filled by re-pitching the nearest sample.
      - If no file names contain notes, a single file is mapped across the whole range
        (root C4); several files are laid out chromatically from C2 in name order.
      - Like real Mellotron tapes, only the first 8 seconds of each sample are played.
*/
class SamplerEngine
{
public:
    static constexpr int lowestNote  = 36;   // C2
    static constexpr int highestNote = 84;   // C6
    static constexpr int numVoices   = 24;
    static constexpr double maxSampleSeconds = 8.0;

    /** A SamplerSound that remembers the rate it was recorded at, so the envelope can be
        corrected for SamplerVoice running its ADSR at the source rate. */
    class TapeSound : public juce::SamplerSound
    {
    public:
        TapeSound (const juce::String& soundName, juce::AudioFormatReader& source,
                   const juce::BigInteger& notes, int rootNote);

        const double sourceRate;
        const int root;
    };

    struct Bank
    {
        juce::String name;      // shown in the UI
        juce::String detail;    // sample count / range or an error
        juce::ReferenceCountedArray<TapeSound> sounds;
    };

    using AbortCheck = std::function<bool()>;

    SamplerEngine();

    void prepare (double sampleRate);
    void setEnvelope (float attackMs, float releaseMs) noexcept;
    void render (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi);
    int getNumActiveVoices() const noexcept;

    /** Swaps in a new set of sounds. Safe to call from any thread. */
    void installBank (Bank& bank);

    //==============================================================================
    static juce::String getWildcard()   { return "*.wav;*.aif;*.aiff;*.flac"; }
    static int parseRootNote (const juce::String& fileNameWithoutExtension);
    static juce::String noteName (int midiNote);

    static Bank loadFolder (const juce::File& folder, juce::AudioFormatManager&, const AbortCheck& shouldAbort);
    static Bank makeBuiltInBank (const AbortCheck& shouldAbort);

    /** Places the plug-in looks for a default bank, in priority order. */
    static juce::Array<juce::File> getDefaultSampleFolders();
    static bool folderHasSamples (const juce::File& folder);

private:
    /** Exposes the synth's internal lock so a bank swap is atomic with respect to rendering. */
    struct Synth final : juce::Synthesiser
    {
        const juce::CriticalSection& getLock() const noexcept { return lock; }
    };

    Synth synth;
    double hostSampleRate = 44100.0;
    float attackSeconds = 0.015f, releaseSeconds = 0.2f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplerEngine)
};
