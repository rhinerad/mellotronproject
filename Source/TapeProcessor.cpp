#include "TapeProcessor.h"

#include <cmath>

namespace
{
    constexpr int toneUpdateInterval = 32;    // samples between tone coefficient updates while smoothing
    constexpr float hissBase = 0.0016f;       // ~ -56 dBFS with four tapes running
    constexpr float humBase  = 0.0005f;       // ~ -66 dBFS
    constexpr double humHz   = 50.0;
}

void TapeProcessor::prepare (double newSampleRate, int maxBlockSize, int newNumChannels)
{
    sampleRate = newSampleRate;
    numChannels = juce::jlimit (1, 2, newNumChannels);

    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, (juce::uint32) numChannels };

    // Centre the transport delay so the deepest wow + flutter excursion never reaches zero
    baseDelaySamples = (float) ((wowMaxMs + 1.5f * flutterMaxMs + 0.5f) * 0.001 * sampleRate);
    transport.prepare (spec);
    transport.setMaximumDelayInSamples ((int) std::ceil (baseDelaySamples * 2.0f) + 8);

    oversampling = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) numChannels, 1,
                                                                     juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                                                                     true, false);
    oversampling->initProcessing ((size_t) maxBlockSize);

    highPass.state = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, highPassHz, 0.7071f);
    lowPass.state  = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, juce::jmin (settings.toneHz, (float) (sampleRate * 0.45)), 0.7071f);
    highPass.prepare (spec);
    lowPass.prepare (spec);

    wowDepth.reset (sampleRate, 0.2);
    flutterDepth.reset (sampleRate, 0.2);
    drive.reset (sampleRate * 2.0, 0.05);
    toneHz.reset (sampleRate, 0.05);
    hissLevel.reset (sampleRate, 0.08);
    humLevel.reset (sampleRate, 0.08);

    wowRateCoef = onePoleCoef (0.7);
    flutterCoef = onePoleCoef (0.08);
    scrapeCoef  = onePoleCoef (1.0 / (juce::MathConstants<double>::twoPi * 15.0));

    latencySamples = juce::roundToInt (baseDelaySamples + oversampling->getLatencyInSamples());
    prepared = true;

    reset();
}

void TapeProcessor::reset()
{
    if (! prepared)
        return;

    transport.reset();
    oversampling->reset();
    highPass.reset();
    lowPass.reset();

    wowDepth.setCurrentAndTargetValue (settings.wow);
    flutterDepth.setCurrentAndTargetValue (settings.flutter);
    drive.setCurrentAndTargetValue (settings.drive);
    toneHz.setCurrentAndTargetValue (juce::jmin (settings.toneHz, (float) (sampleRate * 0.45)));
    hissLevel.setCurrentAndTargetValue (0.0f);
    humLevel.setCurrentAndTargetValue (0.0f);
    setToneCoefficients (toneHz.getCurrentValue());

    satGain = -1.0f;   // forces a recompute on the first sample

    wowPhase = flutterPhase = humPhase = 0.0;
    wowRate = wowRateTarget = wowRateHz;
    flutterRate = flutterRateTarget = 9.0f;
    flutterAmp = flutterAmpTarget = 0.8f;
    scrapeTarget = scrape1 = scrape2 = 0.0f;
    wowCountdown = flutterCountdown = scrapeCountdown = 0;
    std::fill (&pink[0][0], &pink[0][0] + 6, 0.0f);
}

void TapeProcessor::setSettings (const Settings& s) noexcept
{
    settings = s;
    wowDepth.setTargetValue (s.wow);
    flutterDepth.setTargetValue (s.flutter);
    drive.setTargetValue (s.drive);
    toneHz.setTargetValue (juce::jlimit (20.0f, (float) (sampleRate * 0.45), s.toneHz));
}

float TapeProcessor::onePoleCoef (double timeConstantSeconds) const noexcept
{
    return (float) (1.0 - std::exp (-1.0 / (timeConstantSeconds * sampleRate)));
}

//==============================================================================
float TapeProcessor::nextTransportOffsetMs() noexcept
{
    constexpr auto twoPi = juce::MathConstants<double>::twoPi;

    // Wow: capstan / pinch-roller eccentricity, a slow sine with a little rate drift
    if (--wowCountdown <= 0)
    {
        wowRateTarget = wowRateHz * (0.88 + 0.24 * random.nextDouble());
        wowCountdown = (int) (sampleRate * 1.5);
    }

    wowRate += (wowRateTarget - wowRate) * wowRateCoef;
    wowPhase += wowRate / sampleRate;
    if (wowPhase >= 1.0)
        wowPhase -= 1.0;

    // Flutter: a 5-15 Hz component that keeps changing rate and level...
    if (--flutterCountdown <= 0)
    {
        flutterRateTarget = 5.0f + 10.0f * random.nextFloat();
        flutterAmpTarget  = 0.5f + 0.5f * random.nextFloat();
        flutterCountdown  = (int) (sampleRate * (0.1 + 0.3 * random.nextDouble()));
    }

    flutterRate += (flutterRateTarget - flutterRate) * flutterCoef;
    flutterAmp  += (flutterAmpTarget  - flutterAmp)  * flutterCoef;
    flutterPhase += flutterRate / sampleRate;
    if (flutterPhase >= 1.0)
        flutterPhase -= 1.0;

    // ...plus random scrape: 30 Hz sample-and-hold noise through two 15 Hz one-poles
    if (--scrapeCountdown <= 0)
    {
        scrapeTarget = random.nextFloat() * 2.0f - 1.0f;
        scrapeCountdown = (int) (sampleRate / 30.0);
    }

    scrape1 += (scrapeTarget - scrape1) * scrapeCoef;
    scrape2 += (scrape1 - scrape2) * scrapeCoef;

    const auto wow     = (float) std::sin (twoPi * wowPhase);
    const auto flutter = 0.75f * flutterAmp * (float) std::sin (twoPi * flutterPhase) + 0.6f * scrape2;

    return wowDepth.getNextValue() * wowMaxMs * wow + flutterDepth.getNextValue() * flutterMaxMs * flutter;
}

void TapeProcessor::saturate (juce::dsp::AudioBlock<float>& block) noexcept
{
    const auto numSamples = block.getNumSamples();
    const auto chans = block.getNumChannels();

    for (size_t i = 0; i < numSamples; ++i)
    {
        if (drive.isSmoothing() || satGain < 0.0f)
        {
            const auto d = drive.getNextValue();
            satGain   = juce::Decibels::decibelsToGain (d * 18.0f);   // up to +18 dB into the "tape"
            satBias   = 0.12f * d;                                    // asymmetry -> even harmonics
            satOffset = std::tanh (satBias);
            satMakeup = std::pow (satGain, -0.7f);
        }

        for (size_t ch = 0; ch < chans; ++ch)
        {
            auto* p = block.getChannelPointer (ch);
            p[i] = (std::tanh (satGain * p[i] + satBias) - satOffset) * satMakeup;
        }
    }
}

void TapeProcessor::addHissAndHum (juce::AudioBuffer<float>& buffer, int chans) noexcept
{
    const auto noiseOn = settings.noise;
    const auto tapes = (float) juce::jmin (activeTapes, 8);

    // Motor hum is always there while the machine runs; hiss grows with the number of tapes moving
    hissLevel.setTargetValue (noiseOn ? hissBase * (0.3f + 0.35f * std::sqrt (tapes)) : 0.0f);
    humLevel.setTargetValue (noiseOn ? humBase : 0.0f);

    if (! noiseOn && ! hissLevel.isSmoothing() && ! humLevel.isSmoothing())
        return;

    const auto humInc = humHz / sampleRate;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto hiss = hissLevel.getNextValue();
        const auto humPhaseRad = juce::MathConstants<double>::twoPi * humPhase;
        const auto hum = humLevel.getNextValue()
                       * (float) (std::sin (humPhaseRad) + 0.5 * std::sin (2.0 * humPhaseRad) + 0.25 * std::sin (3.0 * humPhaseRad));

        humPhase += humInc;
        if (humPhase >= 1.0)
            humPhase -= 1.0;

        for (int ch = 0; ch < chans; ++ch)
        {
            // Paul Kellet's economy pink filter
            auto& b = pink[ch];
            const auto white = random.nextFloat() * 2.0f - 1.0f;
            b[0] = 0.99765f * b[0] + white * 0.0990460f;
            b[1] = 0.96300f * b[1] + white * 0.2965164f;
            b[2] = 0.57000f * b[2] + white * 1.0526913f;
            const auto pinkSample = (b[0] + b[1] + b[2] + white * 0.1848f) * 0.25f;

            buffer.addSample (ch, i, pinkSample * hiss + hum);
        }
    }
}

void TapeProcessor::setToneCoefficients (float cutoffHz) noexcept
{
    // Assigning into the existing coefficient object avoids allocating on the audio thread
    *lowPass.state = juce::dsp::IIR::ArrayCoefficients<float>::makeLowPass (sampleRate, cutoffHz, 0.7071f);
}

//==============================================================================
void TapeProcessor::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! prepared)
        return;

    const int numSamples = buffer.getNumSamples();
    const int chans = juce::jmin (buffer.getNumChannels(), numChannels);

    if (numSamples == 0 || chans == 0)
        return;

    // 1. Transport: every channel reads the same moving tape
    for (int i = 0; i < numSamples; ++i)
    {
        const auto delay = baseDelaySamples + nextTransportOffsetMs() * 0.001f * (float) sampleRate;

        for (int ch = 0; ch < chans; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            transport.pushSample (ch, data[i]);
            data[i] = transport.popSample (ch, delay, true);
        }
    }

    // 2. Saturation at 2x to keep the tanh harmonics from aliasing
    auto block = juce::dsp::AudioBlock<float> (buffer).getSubsetChannelBlock (0, (size_t) chans);
    auto upsampled = oversampling->processSamplesUp (block);
    saturate (upsampled);
    oversampling->processSamplesDown (block);

    // 3. Hiss and hum go in before the filters so the tone control shapes them too
    addHissAndHum (buffer, chans);

    // 4. Fixed high-pass, smoothed tone low-pass
    for (int start = 0; start < numSamples; start += toneUpdateInterval)
    {
        const auto len = juce::jmin (toneUpdateInterval, numSamples - start);

        if (toneHz.isSmoothing())
            setToneCoefficients (toneHz.skip (len));

        auto sub = block.getSubBlock ((size_t) start, (size_t) len);
        juce::dsp::ProcessContextReplacing<float> context (sub);
        highPass.process (context);
        lowPass.process (context);
    }
}
