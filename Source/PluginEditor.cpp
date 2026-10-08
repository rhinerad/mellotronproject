#include "PluginEditor.h"
#include "Parameters.h"

//==============================================================================
const juce::Colour RetroLookAndFeel::panel      { 0xff1d1915 };
const juce::Colour RetroLookAndFeel::panelLight { 0xff2c2620 };
const juce::Colour RetroLookAndFeel::wood       { 0xff7a4524 };
const juce::Colour RetroLookAndFeel::woodDark   { 0xff4a2914 };
const juce::Colour RetroLookAndFeel::ivory      { 0xffeee4cc };
const juce::Colour RetroLookAndFeel::ivoryShade { 0xffc9bb9b };
const juce::Colour RetroLookAndFeel::amber      { 0xfff0a83a };
const juce::Colour RetroLookAndFeel::ink        { 0xff2a1d12 };
const juce::Colour RetroLookAndFeel::lcd        { 0xff120f0a };

RetroLookAndFeel::RetroLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, ivory.withAlpha (0.85f));
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, amber.withAlpha (0.4f));
    setColour (juce::Label::textColourId, ivory);
    setColour (juce::Label::textWhenEditingColourId, ivory);
    setColour (juce::TextEditor::textColourId, ivory);
    setColour (juce::TextEditor::backgroundColourId, panelLight);
    setColour (juce::TextEditor::focusedOutlineColourId, amber);
    setColour (juce::TextButton::textColourOffId, ink);
    setColour (juce::TextButton::textColourOnId, ink);
    setColour (juce::ToggleButton::textColourId, ivory);
    setColour (juce::TooltipWindow::backgroundColourId, ivory);
    setColour (juce::TooltipWindow::textColourId, ink);
    setColour (juce::TooltipWindow::outlineColourId, woodDark);
    setColour (juce::ResizableWindow::backgroundColourId, panel);
}

juce::Font RetroLookAndFeel::labelFont (float height)
{
    return juce::Font (juce::FontOptions (height, juce::Font::bold)).withExtraKerningFactor (0.12f);
}

void RetroLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                         float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;
    const auto centre = bounds.getCentre();
    const auto angle = startAngle + sliderPos * (endAngle - startAngle);
    const auto enabled = slider.isEnabled();

    auto pointAt = [centre] (float a, float r)
    {
        return centre + juce::Point<float> (std::sin (a) * r, -std::cos (a) * r);
    };

    // Scale ticks
    for (int i = 0; i <= 10; ++i)
    {
        const auto a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const auto major = (i % 5 == 0);
        g.setColour (ivory.withAlpha (major ? 0.75f : 0.4f));
        g.drawLine ({ pointAt (a, radius - (major ? 7.0f : 5.0f)), pointAt (a, radius) }, major ? 2.0f : 1.4f);
    }

    // Value arc
    const auto arcRadius = radius - 10.0f;
    juce::Path track, value;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
    const juce::PathStrokeType stroke (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.strokePath (track, stroke);
    g.setColour (enabled ? amber : amber.withSaturation (0.0f));
    g.strokePath (value, stroke);

    // Knob: drop shadow, knurled metal skirt, bakelite cap
    const auto knobRadius = arcRadius - 6.0f;
    const auto knob = juce::Rectangle<float> (knobRadius * 2.0f, knobRadius * 2.0f).withCentre (centre);

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (knob.translated (0.0f, 3.0f).expanded (1.0f));

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8f877c), knob.getX(), knob.getY(),
                                             juce::Colour (0xff2e2a26), knob.getRight(), knob.getBottom(), false));
    g.fillEllipse (knob);

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    for (int i = 0; i < 36; ++i)
    {
        const auto a = (float) i / 36.0f * juce::MathConstants<float>::twoPi + angle;
        g.drawLine ({ pointAt (a, knobRadius * 0.82f), pointAt (a, knobRadius - 0.5f) }, 1.0f);
    }

    const auto cap = knob.reduced (knobRadius * 0.2f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3d3632), cap.getCentreX(), cap.getY(),
                                             juce::Colour (0xff0b0a09), cap.getCentreX(), cap.getBottom(), false));
    g.fillEllipse (cap);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawEllipse (cap.reduced (1.0f), 1.0f);

    // Pointer
    g.setColour (ivory);
    g.drawLine ({ pointAt (angle, knobRadius * 0.18f), pointAt (angle, knobRadius * 0.78f) }, 3.0f);
}

void RetroLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                             bool highlighted, bool down)
{
    auto r = button.getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 4.0f);

    if (down)
        r.translate (0.0f, 1.5f);

    auto top = highlighted ? ivory.brighter (0.08f) : ivory;
    g.setGradientFill (juce::ColourGradient (down ? ivoryShade : top, r.getX(), r.getY(),
                                             down ? ivory.darker (0.15f) : ivoryShade, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (woodDark.withAlpha (0.8f));
    g.drawRoundedRectangle (r, 4.0f, 1.2f);
}

juce::Font RetroLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return labelFont (juce::jmin (15.0f, (float) buttonHeight * 0.4f));
}

void RetroLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    auto r = button.getLocalBounds().toFloat().reduced (1.0f);
    const auto on = button.getToggleState();

    g.setColour (panelLight);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (highlighted ? amber.withAlpha (0.6f) : ivory.withAlpha (0.25f));
    g.drawRoundedRectangle (r, 4.0f, 1.2f);

    const auto ledSize = juce::jmin (16.0f, r.getHeight() * 0.45f);
    const auto led = juce::Rectangle<float> (ledSize, ledSize).withCentre ({ r.getX() + 8.0f + ledSize * 0.5f, r.getCentreY() });

    if (on)
    {
        g.setColour (amber.withAlpha (0.25f));
        g.fillEllipse (led.expanded (5.0f));
    }

    g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xffffe2a8) : juce::Colour (0xff5a2a10),
                                             led.getCentreX(), led.getY() + 3.0f,
                                             on ? amber.darker (0.2f) : juce::Colour (0xff240f05),
                                             led.getCentreX(), led.getBottom(), true));
    g.fillEllipse (led);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (led, 1.0f);

    g.setColour (ivory.withAlpha (button.isEnabled() ? 1.0f : 0.5f));
    g.setFont (labelFont (12.5f));
    g.drawText (button.getButtonText(), r.withTrimmedLeft (ledSize + 16.0f), juce::Justification::centredLeft);
}

juce::Label* RetroLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (juce::FontOptions (12.5f));
    return label;
}

//==============================================================================
namespace
{
    constexpr int editorWidth = 880, editorHeight = 520;
    constexpr int cheekWidth = 26;

    struct KnobSpec
    {
        const char* paramId;
        const char* name;
        const char* tooltip;
    };

    const KnobSpec knobSpecs[] = {
        { ParamIDs::drive,        "DRIVE",   "Tape saturation: how hard the signal hits the tape" },
        { ParamIDs::wowDepth,     "WOW",     "Slow pitch drift from the capstan (about 0.8 Hz)" },
        { ParamIDs::flutterDepth, "FLUTTER", "Fast, irregular pitch wobble (5-15 Hz)" },
        { ParamIDs::tone,         "TONE",    "Low-pass cutoff: darker to brighter tape" },
        { ParamIDs::attack,       "ATTACK",  "How quickly the pressure pad engages the tape" },
        { ParamIDs::release,      "RELEASE", "How quickly the tape disengages after a key is released" },
        { ParamIDs::output,       "VOLUME",  "Output level" },
    };
}

TapeBankAudioProcessorEditor::TapeBankAudioProcessorEditor (TapeBankAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    groups = { { "TAPE", 0, 4, {} }, { "ENVELOPE", 4, 2, {} }, { "OUTPUT", 6, 1, {} } };

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        const auto& spec = knobSpecs[i];

        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
        k.slider.setTooltip (spec.tooltip);
        k.slider.setPopupDisplayEnabled (false, false, nullptr);
        addAndMakeVisible (k.slider);

        k.label.setText (spec.name, juce::dontSendNotification);
        k.label.setFont (RetroLookAndFeel::labelFont (12.5f));
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (k.label);

        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, spec.paramId, k.slider);
    }

    noiseButton.setTooltip ("Tape hiss (grows with the number of keys held) and motor hum");
    addAndMakeVisible (noiseButton);
    noiseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamIDs::noise, noiseButton);

    loadButton.setTooltip ("Choose a folder of .wav samples (or drag a folder onto the panel)");
    loadButton.onClick = [this] { chooseFolder(); };
    addAndMakeVisible (loadButton);

    keyboard.setAvailableRange (SamplerEngine::lowestNote, SamplerEngine::highestNote);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setWantsKeyboardFocus (false);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, RetroLookAndFeel::ivory);
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff141210));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff7d6f5c));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, RetroLookAndFeel::amber.withAlpha (0.65f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, RetroLookAndFeel::amber.withAlpha (0.2f));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha (0.5f));
    keyboard.setColour (juce::MidiKeyboardComponent::textLabelColourId, RetroLookAndFeel::ink.withAlpha (0.6f));
    addAndMakeVisible (keyboard);

    // Set after the children exist so the sliders rebuild their text boxes with this look
    setLookAndFeel (&lookAndFeel);
    setSize (editorWidth, editorHeight);
    timerCallback();
    startTimerHz (10);
}

TapeBankAudioProcessorEditor::~TapeBankAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void TapeBankAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (cheekWidth, 0).reduced (18, 0);

    area.removeFromTop (74);   // logo strip, painted

    auto bankRow = area.removeFromTop (54);
    loadButton.setBounds (bankRow.removeFromRight (170));
    bankRow.removeFromRight (12);
    noiseButton.setBounds (bankRow.removeFromRight (140));
    bankRow.removeFromRight (12);
    displayBounds = bankRow;

    area.removeFromTop (16);
    auto knobRow = area.removeFromTop (178);

    constexpr int groupGap = 14;
    const int totalKnobs = (int) knobs.size();
    const int knobWidth = (knobRow.getWidth() - groupGap * ((int) groups.size() - 1)) / totalKnobs;

    for (auto& group : groups)
    {
        group.bounds = knobRow.removeFromLeft (knobWidth * group.numKnobs);
        knobRow.removeFromLeft (groupGap);

        auto inner = group.bounds.reduced (0, 4).withTrimmedTop (18);

        for (int i = 0; i < group.numKnobs; ++i)
        {
            auto cell = inner.removeFromLeft (knobWidth);
            auto& k = knobs[(size_t) (group.firstKnob + i)];
            k.label.setBounds (cell.removeFromTop (20));
            k.slider.setBounds (cell.reduced (6, 0));
        }
    }

    area.removeFromTop (14);
    keyboardFrame = area.withTrimmedBottom (16);
    auto keys = keyboardFrame.reduced (6).withTrimmedTop (8);
    keyboard.setBounds (keys);
    keyboard.setKeyWidth ((float) keys.getWidth() / 29.0f);   // 29 white keys from C2 to C6
}

//==============================================================================
void TapeBankAudioProcessorEditor::paintWood (juce::Graphics& g, juce::Rectangle<float> r) const
{
    g.setGradientFill (juce::ColourGradient (RetroLookAndFeel::wood, r.getX(), 0.0f,
                                             RetroLookAndFeel::woodDark, r.getRight(), 0.0f, false));
    g.fillRect (r);

    juce::Random grain (0x5eed + (int) r.getX());
    for (int i = 0; i < 14; ++i)
    {
        const auto x = r.getX() + grain.nextFloat() * r.getWidth();
        const auto wobble = 1.0f + grain.nextFloat() * 3.0f;
        juce::Path line;
        line.startNewSubPath (x, r.getY());
        for (float yy = r.getY(); yy <= r.getBottom(); yy += 20.0f)
            line.lineTo (x + std::sin (yy * 0.013f + (float) i) * wobble, yy);

        g.setColour (RetroLookAndFeel::woodDark.withAlpha (0.25f + 0.3f * grain.nextFloat()));
        g.strokePath (line, juce::PathStrokeType (0.6f + grain.nextFloat()));
    }

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRect (r.getX() < 1.0f ? r.withX (r.getRight() - 2.0f).withWidth (2.0f) : r.withWidth (2.0f));
}

void TapeBankAudioProcessorEditor::paintDisplay (juce::Graphics& g) const
{
    const auto r = displayBounds.toFloat();

    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.expanded (2.0f), 6.0f);
    g.setColour (RetroLookAndFeel::lcd);
    g.fillRoundedRectangle (r, 5.0f);
    g.setGradientFill (juce::ColourGradient (RetroLookAndFeel::amber.withAlpha (0.08f), r.getCentreX(), r.getY(),
                                             juce::Colours::transparentBlack, r.getCentreX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 5.0f);

    auto text = r.reduced (12.0f, 6.0f);
    const auto name = shownStatus.name.isNotEmpty() ? shownStatus.name.toUpperCase() : juce::String ("NO BANK");

    g.setColour (RetroLookAndFeel::amber);
    g.setFont (RetroLookAndFeel::labelFont (17.0f));
    g.drawFittedText (name, text.removeFromTop (text.getHeight() * 0.55f).toNearestInt(), juce::Justification::bottomLeft, 1);

    g.setColour (RetroLookAndFeel::amber.withAlpha (shownStatus.loading ? 1.0f : 0.7f));
    g.setFont (juce::FontOptions (12.5f));
    g.drawFittedText (shownStatus.detail, text.toNearestInt(), juce::Justification::topLeft, 1);
}

void TapeBankAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Panel
    g.setGradientFill (juce::ColourGradient (RetroLookAndFeel::panelLight, 0.0f, 0.0f,
                                             RetroLookAndFeel::panel, 0.0f, bounds.getHeight(), false));
    g.fillAll();

    paintWood (g, bounds.withWidth ((float) cheekWidth));
    paintWood (g, bounds.withLeft (bounds.getRight() - (float) cheekWidth));

    const auto inner = bounds.reduced ((float) cheekWidth, 0.0f).reduced (18.0f, 0.0f);

    // Logo strip
    g.setColour (RetroLookAndFeel::ivory);
    g.setFont (RetroLookAndFeel::labelFont (34.0f).withExtraKerningFactor (0.3f));
    g.drawText ("TAPEBANK", inner.withHeight (60.0f).withTrimmedTop (8.0f), juce::Justification::centredLeft);

    g.setColour (RetroLookAndFeel::amber);
    g.setFont (RetroLookAndFeel::labelFont (11.5f).withExtraKerningFactor (0.25f));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("TAPE REPLAY KEYBOARD  \xc2\xb7  MODEL 400")), inner.withHeight (60.0f).withTrimmedTop (8.0f),
                juce::Justification::centredRight);

    g.setColour (RetroLookAndFeel::ivory.withAlpha (0.2f));
    g.fillRect (inner.getX(), 64.0f, inner.getWidth(), 1.0f);

    paintDisplay (g);

    // Knob groups: outlined panels with their title set into the top edge
    for (auto& group : groups)
    {
        const auto r = group.bounds.toFloat().withTrimmedTop (8.0f);
        const auto titleFont = RetroLookAndFeel::labelFont (11.0f).withExtraKerningFactor (0.3f);
        const auto titleWidth = juce::GlyphArrangement::getStringWidth (titleFont, group.title) + 14.0f;

        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.fillRoundedRectangle (r, 6.0f);

        juce::Path frame;
        frame.addRoundedRectangle (r, 6.0f);
        g.saveState();
        g.excludeClipRegion (juce::Rectangle<float> (titleWidth, 16.0f).withCentre ({ r.getCentreX(), r.getY() }).toNearestInt());
        g.setColour (RetroLookAndFeel::ivory.withAlpha (0.3f));
        g.strokePath (frame, juce::PathStrokeType (1.0f));
        g.restoreState();

        g.setColour (RetroLookAndFeel::amber);
        g.setFont (titleFont);
        g.drawText (group.title, juce::Rectangle<float> (titleWidth, 16.0f).withCentre ({ r.getCentreX(), r.getY() }),
                    juce::Justification::centred);
    }

    // Keyboard well with a strip of felt along the back
    const auto well = keyboardFrame.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (well, 5.0f);
    g.setColour (juce::Colour (0xff5b1a17));
    g.fillRect (well.reduced (6.0f, 0.0f).withTrimmedTop (5.0f).withHeight (7.0f));

    if (dragHover)
    {
        g.setColour (RetroLookAndFeel::amber.withAlpha (0.12f));
        g.fillRect (bounds);
        g.setColour (RetroLookAndFeel::amber);
        g.drawRect (bounds, 3.0f);
    }
}

//==============================================================================
void TapeBankAudioProcessorEditor::timerCallback()
{
    auto status = processorRef.getBankStatus();
    const auto folder = processorRef.getActiveFolder().getFullPathName();

    if (status.name != shownStatus.name || status.detail != shownStatus.detail
        || status.loading != shownStatus.loading || folder != shownFolder)
    {
        shownStatus = std::move (status);
        shownFolder = folder;
        loadButton.setEnabled (! shownStatus.loading);
        repaint (displayBounds.expanded (4));
    }
}

void TapeBankAudioProcessorEditor::chooseFolder()
{
    auto start = processorRef.getActiveFolder();
    if (! start.isDirectory())
        start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    chooser = std::make_unique<juce::FileChooser> ("Choose a folder of samples", start);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [safeThis = juce::Component::SafePointer (this)] (const juce::FileChooser& fc)
                          {
                              const auto result = fc.getResult();
                              if (safeThis != nullptr && result.isDirectory())
                                  safeThis->processorRef.loadSampleFolder (result);
                          });
}

bool TapeBankAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& path : files)
    {
        const juce::File f (path);
        if (f.isDirectory() || f.hasFileExtension ("wav;aif;aiff;flac"))
            return true;
    }

    return false;
}

void TapeBankAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int)
{
    setDragHover (false);

    if (files.isEmpty())
        return;

    const juce::File f (files[0]);
    processorRef.loadSampleFolder (f.isDirectory() ? f : f.getParentDirectory());
}

void TapeBankAudioProcessorEditor::setDragHover (bool shouldHover)
{
    if (dragHover != shouldHover)
    {
        dragHover = shouldHover;
        repaint();
    }
}
