#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ParamIDs
{
    inline constexpr const char* drive        = "drive";
    inline constexpr const char* wowDepth     = "wow_depth";
    inline constexpr const char* flutterDepth = "flutter_depth";
    inline constexpr const char* tone         = "tone";
    inline constexpr const char* attack       = "attack";
    inline constexpr const char* release      = "release";
    inline constexpr const char* noise        = "noise";
    inline constexpr const char* output       = "output";
}

namespace Params
{
    inline juce::String percentText (float v, int)  { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }

    inline juce::String hzText (float v, int)
    {
        return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz"
                            : juce::String (juce::roundToInt (v)) + " Hz";
    }

    inline juce::String msText (float v, int)
    {
        return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s"
                            : juce::String (juce::roundToInt (v)) + " ms";
    }

    inline juce::String dbText (float v, int)       { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; }

    inline juce::NormalisableRange<float> skewedRange (float min, float max, float centre)
    {
        juce::NormalisableRange<float> r (min, max);
        r.setSkewForCentre (centre);
        return r;
    }

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using juce::AudioParameterFloat;
        using juce::AudioParameterFloatAttributes;
        using juce::ParameterID;

        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        auto unit = juce::NormalisableRange<float> (0.0f, 1.0f);
        auto pct  = AudioParameterFloatAttributes().withStringFromValueFunction (percentText);

        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::drive, 1 },        "Drive",   unit, 0.3f,  pct));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::wowDepth, 1 },     "Wow",     unit, 0.3f,  pct));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::flutterDepth, 1 }, "Flutter", unit, 0.25f, pct));

        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::tone, 1 }, "Tone",
                                                           skewedRange (500.0f, 15000.0f, 3000.0f), 7000.0f,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction (hzText)
                                                                                          .withLabel ("Hz")));

        auto ms = AudioParameterFloatAttributes().withStringFromValueFunction (msText).withLabel ("ms");

        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::attack, 1 },  "Attack",
                                                           skewedRange (1.0f, 1000.0f, 100.0f), 15.0f, ms));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::release, 1 }, "Release",
                                                           skewedRange (10.0f, 2000.0f, 300.0f), 200.0f, ms));

        layout.add (std::make_unique<juce::AudioParameterBool> (ParameterID { ParamIDs::noise, 1 }, "Hiss & Hum", false));

        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::output, 1 }, "Output",
                                                           juce::NormalisableRange<float> (-24.0f, 6.0f, 0.1f), 0.0f,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction (dbText)
                                                                                          .withLabel ("dB")));
        return layout;
    }
}
