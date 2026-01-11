/*
  ==============================================================================

    PluginEditor.cpp
    Created: 11 Jan 2026
    Updated for JUCE 8.0.12

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
void WalrusLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4);

    juce::ColourGradient gradient(
        juce::Colours::darkviolet.darker(0.2f),
        bounds.getBottomLeft(),
        juce::Colours::cyan.darker(0.3f),
        bounds.getTopRight(),
        true
    );

    gradient.addColour(0.3f, juce::Colours::purple.withBrightness(0.6f));
    gradient.addColour(0.7f, juce::Colours::blue.withBrightness(0.5f));

    g.setGradientFill(gradient);
    g.fillEllipse(bounds);

    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawEllipse(bounds.reduced(1), 1.0f);

    auto center = bounds.getCentre();
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.4f;
    auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    juce::Path needle;
    needle.startNewSubPath(center.getPointOnCircumference(radius * 0.6f, angle));
    needle.lineTo(center.getPointOnCircumference(radius * 0.95f, angle));

    float hue = sliderPos * 0.7f;
    g.setColour(juce::Colour::fromHSV(hue, 0.8f, 0.9f, 1.0f));
    g.strokePath(needle, juce::PathStrokeType(3.0f));

    g.setColour(juce::Colours::yellow.withAlpha(0.7f));
    g.fillEllipse(juce::Rectangle<float>(8, 8).withCentre(center));
}

void WalrusLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
    bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(2);

    if (button.getToggleState())
    {
        juce::ColourGradient gradient(
            juce::Colours::limegreen.darker(0.2f),
            bounds.getTopLeft(),
            juce::Colours::yellow.withBrightness(0.7f),
            bounds.getBottomRight(),
            true
        );
        g.setGradientFill(gradient);
    }
    else
    {
        g.setColour(juce::Colours::darkgrey.withAlpha(0.7f));
    }

    g.fillRoundedRectangle(bounds, 6.0f);

    g.setColour(juce::Colours::white.withAlpha(0.3f));
    g.drawRoundedRectangle(bounds, 6.0f, 1.5f);

    g.setColour(juce::Colours::white);
     
    g.setFont(juce::Font(14.0f));
    g.drawText(button.getButtonText(), bounds, juce::Justification::centred);
}

 
WalrusDelay1AudioProcessorEditor::WalrusDelay1AudioProcessorEditor(WalrusDelay1AudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setLookAndFeel(&walrusLookAndFeel);

    setupKnob(delayTimeKnob, "Delay Time", juce::Colours::cyan);
    setupKnob(feedbackKnob, "Feedback", juce::Colours::magenta);
    setupKnob(wowRateKnob, "Wow Rate", juce::Colours::yellow);
    setupKnob(wowDepthKnob, "Wow Depth", juce::Colours::orange);
    setupKnob(flutterRateKnob, "Flutter Rate", juce::Colours::green);
    setupKnob(flutterDepthKnob, "Flutter Depth", juce::Colours::lime);
    setupKnob(dryWetKnob, "Dry/Wet", juce::Colours::blue);
    setupKnob(reverbLevelKnob, "Reverb Level", juce::Colours::purple);
    setupKnob(filterFreqKnob, "Filter Freq", juce::Colours::red);
    setupKnob(saturationKnob, "Tape Warmth", juce::Colours::brown);

    setupButton(tapeDelayOnOffButton, "Tape Delay", juce::Colours::cyan.withAlpha(0.7f));
    setupButton(reverbOnOffButton, "Reverb", juce::Colours::purple.withAlpha(0.7f));
    setupButton(psychedelicModeButton, "Psychedelic", juce::Colours::orange.withAlpha(0.7f));

    createLabel(delayTimeLabel, "DELAY TIME");
    createLabel(feedbackLabel, "FEEDBACK");
    createLabel(wowRateLabel, "WOW RATE");
    createLabel(wowDepthLabel, "WOW DEPTH");
    createLabel(flutterRateLabel, "FLUTTER RATE");
    createLabel(flutterDepthLabel, "FLUTTER DEPTH");
    createLabel(dryWetLabel, "DRY/WET");
    createLabel(reverbLevelLabel, "REVERB");
    createLabel(filterFreqLabel, "FILTER");
    createLabel(saturationLabel, "WARMTH");
    createLabel(tapeDelayOnOffLabel, "TAPE");
    createLabel(reverbOnOffLabel, "REVERB");
    createLabel(psychedelicModeLabel, "PSYCHEDELIC");

    delayTimeAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "DelayTime", delayTimeKnob);
    feedbackAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "Feedback", feedbackKnob);
    wowRateAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "WowRate", wowRateKnob);
    wowDepthAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "WowDepth", wowDepthKnob);
    flutterRateAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "FlutterRate", flutterRateKnob);
    flutterDepthAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "FlutterDepth", flutterDepthKnob);
    dryWetAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "DryWet", dryWetKnob);
    reverbLevelAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "ReverbLevel", reverbLevelKnob);
    filterFreqAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "FilterFreq", filterFreqKnob);
    saturationAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "Saturation", saturationKnob);

    tapeDelayOnOffAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "TapeDelayOnOff", tapeDelayOnOffButton);
    reverbOnOffAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "ReverbOnOff", reverbOnOffButton);
    psychedelicModeAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "PsychedelicMode", psychedelicModeButton);

    psychedelicModeButton.onClick = [this]() {
        repaint();
        };

    setSize(1000, 400);
    setResizable(true, true);
    setResizeLimits(800, 350, 1200, 500);
}

WalrusDelay1AudioProcessorEditor::~WalrusDelay1AudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void WalrusDelay1AudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient bgGradient(
        juce::Colours::darkblue.withBrightness(0.1f),
        getLocalBounds().getTopLeft().toFloat(),
        juce::Colours::black,
        getLocalBounds().getBottomRight().toFloat(),
        true
    );

    bgGradient.addColour(0.3f, juce::Colours::darkviolet.withBrightness(0.15f));
    bgGradient.addColour(0.6f, juce::Colours::darkgreen.withBrightness(0.1f));

    g.setGradientFill(bgGradient);
    g.fillAll();

     
    g.setColour(juce::Colours::white.withAlpha(0.9f));
 
    g.setFont(juce::Font(36.0f, juce::Font::bold));

    auto titleBounds = getLocalBounds().removeFromTop(70).reduced(20, 10);
    juce::String title = "WALRUS DELAY 2";

    // Draw title with gradient effect
    juce::ColourGradient titleGradient(
        juce::Colours::cyan,
        titleBounds.getTopLeft().toFloat(),
        juce::Colours::magenta,
        titleBounds.getBottomRight().toFloat(),
        false
    );
    titleGradient.addColour(0.5f, juce::Colours::yellow);

    g.setGradientFill(titleGradient);
    g.drawText(title, titleBounds, juce::Justification::centred);

    // Subtle glow lines in psychedelic mode
    if (psychedelicModeButton.getToggleState())
    {
        g.setColour(juce::Colours::cyan.withAlpha(0.1f));
        for (int i = 0; i < 5; ++i)
        {
            float y = static_cast<float>(getHeight()) * (i + 1) / 6.0f;
            g.drawLine(0.0f, y, static_cast<float>(getWidth()), y, 2.0f);
        }
    }

    // Version text
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.setFont(juce::Font(12.0f));
    g.drawText("JUCE 8.0.12 | Walrus Delay Version 2",
        getLocalBounds().removeFromBottom(20),
        juce::Justification::centred);
}

void WalrusDelay1AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto titleArea = bounds.removeFromTop(70);
    auto controlArea = bounds.reduced(20, 10);

    const int numRows = 2;
    const int knobsPerRow = 6;
    const int knobWidth = controlArea.getWidth() / knobsPerRow;
    const int knobHeight = static_cast<int>(controlArea.getHeight() * 0.7f / numRows);
    const int labelHeight = 25;

    // Row 1
    int currentX = controlArea.getX();
    int currentY = controlArea.getY();

    placeControl(delayTimeKnob, delayTimeLabel, currentX, currentY, knobWidth, knobHeight, labelHeight); currentX += knobWidth;
    placeControl(feedbackKnob, feedbackLabel, currentX, currentY, knobWidth, knobHeight, labelHeight); currentX += knobWidth;
    placeControl(wowRateKnob, wowRateLabel, currentX, currentY, knobWidth, knobHeight, labelHeight); currentX += knobWidth;
    placeControl(wowDepthKnob, wowDepthLabel, currentX, currentY, knobWidth, knobHeight, labelHeight); currentX += knobWidth;
    placeControl(flutterRateKnob, flutterRateLabel, currentX, currentY, knobWidth, knobHeight, labelHeight); currentX += knobWidth;
    placeControl(flutterDepthKnob, flutterDepthLabel, currentX, currentY, knobWidth, knobHeight, labelHeight);

    // Row 2
    currentX = controlArea.getX();
    currentY += knobHeight + labelHeight + 10;

    placeControl(dryWetKnob, dryWetLabel, currentX, currentY, knobWidth, knobHeight, labelHeight); currentX += knobWidth;
    placeControl(reverbLevelKnob, reverbLevelLabel, currentX, currentY, knobWidth, knobHeight, labelHeight); currentX += knobWidth;
    placeControl(filterFreqKnob, filterFreqLabel, currentX, currentY, knobWidth, knobHeight, labelHeight); currentX += knobWidth;
    placeControl(saturationKnob, saturationLabel, currentX, currentY, knobWidth, knobHeight, labelHeight); currentX += knobWidth;

    // Buttons area
    int buttonWidth = knobWidth / 2;
    int buttonHeight = static_cast<int>(knobHeight * 0.8f);

    placeControl(tapeDelayOnOffButton, tapeDelayOnOffLabel, currentX, currentY, buttonWidth, buttonHeight, labelHeight); currentX += buttonWidth + 20;
    placeControl(reverbOnOffButton, reverbOnOffLabel, currentX, currentY, buttonWidth, buttonHeight, labelHeight); currentX += buttonWidth + 20;
    placeControl(psychedelicModeButton, psychedelicModeLabel, currentX, currentY, buttonWidth, buttonHeight, labelHeight);
}

void WalrusDelay1AudioProcessorEditor::setupKnob(juce::Slider& slider, const juce::String& name, juce::Colour colour)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    slider.setColour(juce::Slider::rotarySliderFillColourId, colour.withAlpha(0.7f));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::white.withAlpha(0.3f));
    addAndMakeVisible(slider);
}

void WalrusDelay1AudioProcessorEditor::setupButton(juce::ToggleButton& button, const juce::String& name, juce::Colour colour)
{
    button.setButtonText(name);
    button.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    button.setColour(juce::ToggleButton::tickColourId, colour);
    button.setColour(juce::ToggleButton::tickDisabledColourId, colour.withAlpha(0.5f));
    addAndMakeVisible(button);
}

void WalrusDelay1AudioProcessorEditor::createLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
 
    label.setFont(juce::Font(13.0f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.8f));
    addAndMakeVisible(label);
}

void WalrusDelay1AudioProcessorEditor::placeControl(juce::Component& control, juce::Label& label,
    int x, int y, int width, int height, int labelHeight)
{
    control.setBounds(x, y, width, height);
    label.setBounds(x, y + height, width, labelHeight);
}

void WalrusDelay1AudioProcessorEditor::placeControl(juce::ToggleButton& control, juce::Label& label,
    int x, int y, int width, int height, int labelHeight)
{
    control.setBounds(x, y + (height - 30) / 2, width, 30);
    label.setBounds(x, y + height, width, labelHeight);
}