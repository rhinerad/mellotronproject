#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"

/** Bakelite knobs, ivory keys and an amber bank display. */
class RetroLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    RetroLookAndFeel();

    static const juce::Colour panel, panelLight, wood, woodDark, ivory, ivoryShade, amber, ink, lcd;

    static juce::Font labelFont (float height);

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& background,
                               bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    juce::Label* createSliderTextBox (juce::Slider&) override;
};

//==============================================================================
class TapeBankAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           public juce::FileDragAndDropTarget,
                                           private juce::Timer
{
public:
    explicit TapeBankAudioProcessorEditor (TapeBankAudioProcessor&);
    ~TapeBankAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override  { setDragHover (true); }
    void fileDragExit (const juce::StringArray&) override             { setDragHover (false); }
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Group
    {
        juce::String title;
        int firstKnob, numKnobs;
        juce::Rectangle<int> bounds;
    };

    void timerCallback() override;
    void chooseFolder();
    void setDragHover (bool);
    void paintWood (juce::Graphics&, juce::Rectangle<float>) const;
    void paintDisplay (juce::Graphics&) const;

    TapeBankAudioProcessor& processorRef;
    RetroLookAndFeel lookAndFeel;

    std::array<Knob, 7> knobs;
    std::vector<Group> groups;

    juce::ToggleButton noiseButton { "HISS & HUM" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> noiseAttachment;
    juce::TextButton loadButton { "LOAD SAMPLES" };
    juce::MidiKeyboardComponent keyboard;
    juce::TooltipWindow tooltips { this, 700 };
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Rectangle<int> displayBounds, keyboardFrame;
    TapeBankAudioProcessor::BankStatus shownStatus;
    juce::String shownFolder;
    bool dragHover = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeBankAudioProcessorEditor)
};
