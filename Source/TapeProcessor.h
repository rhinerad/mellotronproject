#pragma once

#include <juce_dsp/juce_dsp.h>

/*  Tape-machine colouring applied to the summed synth output:

      transport (wow + flutter modulated delay)
        -> 2x oversampled tanh saturation
        -> hiss / motor hum (optional)
        -> 80 Hz high-pass -> tone low-pass
*/
class TapeProcessor
{
public:
    struct Settings
    {
        float drive   = 0.3f;     // 0..1
        float wow     = 0.3f;     // 0..1
        float flutter = 0.25f;    // 0..1
        float toneHz  = 7000.0f;  // low-pass cutoff
        bool noise    = false;    // hiss + motor hum
    };

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    void setSettings (const Settings& newSettings) noexcept;
    /** Hiss follows the number of tapes running, like the real thing. */
    void setActiveTapes (int numActiveVoices) noexcept   { activeTapes = numActiveVoices; }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

    /** Fixed delay introduced by the transport line and the oversampling filters. */
    int getLatencySamples() const noexcept               { return latencySamples; }

    static constexpr float wowMaxMs     = 2.0f;    // ~20 cents peak at 0.8 Hz
    static constexpr float flutterMaxMs = 0.15f;   // ~10 cents peak at 10 Hz
    static constexpr float wowRateHz    = 0.8f;
    static constexpr float highPassHz   = 80.0f;

private:
    float nextTransportOffsetMs() noexcept;
    void saturate (juce::dsp::AudioBlock<float>& block) noexcept;
    void addHissAndHum (juce::AudioBuffer<float>& buffer, int numChannelsToUse) noexcept;
    void setToneCoefficients (float cutoffHz) noexcept;
    float onePoleCoef (double timeConstantSeconds) const noexcept;

    using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;

    Settings settings;
    double sampleRate = 44100.0;
    int numChannels = 2;
    int latencySamples = 0;
    float baseDelaySamples = 0.0f;
    bool prepared = false;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> transport;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    Filter highPass, lowPass;

    juce::SmoothedValue<float> wowDepth, flutterDepth, drive, hissLevel, humLevel;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> toneHz;

    // Saturator coefficients, recomputed only while drive is moving
    float satGain = 1.0f, satBias = 0.0f, satOffset = 0.0f, satMakeup = 1.0f;

    // Wow: a sine whose rate drifts a little around wowRateHz
    double wowPhase = 0.0, wowRate = wowRateHz, wowRateTarget = wowRateHz;
    int wowCountdown = 0;

    // Flutter: a 5-15 Hz sine wandering in rate and level, plus smoothed random scrape
    double flutterPhase = 0.0;
    float flutterRate = 9.0f, flutterRateTarget = 9.0f, flutterAmp = 0.8f, flutterAmpTarget = 0.8f;
    float scrapeTarget = 0.0f, scrape1 = 0.0f, scrape2 = 0.0f;
    int flutterCountdown = 0, scrapeCountdown = 0;
    float wowRateCoef = 0.0f, flutterCoef = 0.0f, scrapeCoef = 0.0f;

    // Noise
    juce::Random random;
    float pink[2][3] {};
    double humPhase = 0.0;
    int activeTapes = 0;
};
