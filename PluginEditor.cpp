/*
  ==============================================================================

    PluginEditor.cpp
    Created: 5 Sep 2025

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
void WalrusLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, const float rotaryStartAngle, const float rotaryEndAngle, [[maybe_unused]] juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4);
    g.setColour(juce::Colours::darkgrey);
    g.fillEllipse(bounds);
    juce::Colour gradientStart = juce::Colours::darkviolet.darker(0.3f);
    juce::Colour gradientEnd = juce::Colours::cyan.darker(0.3f);
    juce::ColourGradient gradient(gradientStart, bounds.getBottomLeft(), gradientEnd, bounds.getTopRight(), false);
    g.setGradientFill(gradient);
    g.fillEllipse(bounds.reduced(4));
    auto center = bounds.getCentre();
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.4f;
    auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    juce::Path p;
    p.startNewSubPath(center.getPointOnCircumference(radius * 0.6f, angle));
    p.lineTo(center.getPointOnCircumference(radius * 0.95f, angle));
    g.setColour(juce::Colours::yellow);
    g.strokePath(p, juce::PathStrokeType(2.0f));
}

//==============================================================================
WalrusDelay1AudioProcessorEditor::WalrusDelay1AudioProcessorEditor(WalrusDelay1AudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setLookAndFeel(&walrusLookAndFeel);

    setupSlider(delayTimeKnob, "Delay Time");
    setupSlider(feedbackKnob, "Feedback");
    setupSlider(wowRateKnob, "Wow Rate");
    setupSlider(wowDepthKnob, "Wow Depth");
    setupSlider(flutterRateKnob, "Flutter Rate");
    setupSlider(flutterDepthKnob, "Flutter Depth");
    setupSlider(dryWetKnob, "Dry/Wet Mix");
    setupSlider(reverbLevelKnob, "Reverb Level");
    setupSlider(filterFreqKnob, "Filter Freq");
    setupButton(tapeDelayOnOffButton, "Tape Delay On/Off");
    setupButton(reverbOnOffButton, "Reverb On/Off");

    delayTimeLabel.setText("Delay Time", juce::dontSendNotification);
    delayTimeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(delayTimeLabel);

    feedbackLabel.setText("Feedback", juce::dontSendNotification);
    feedbackLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(feedbackLabel);

    wowRateLabel.setText("Wow Rate", juce::dontSendNotification);
    wowRateLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(wowRateLabel);

    wowDepthLabel.setText("Wow Depth", juce::dontSendNotification);
    wowDepthLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(wowDepthLabel);

    flutterRateLabel.setText("Flutter Rate", juce::dontSendNotification);
    flutterRateLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(flutterRateLabel);

    flutterDepthLabel.setText("Flutter Depth", juce::dontSendNotification);
    flutterDepthLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(flutterDepthLabel);

    dryWetLabel.setText("Dry/Wet Mix", juce::dontSendNotification);
    dryWetLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(dryWetLabel);

    reverbLevelLabel.setText("Reverb Level", juce::dontSendNotification);
    reverbLevelLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(reverbLevelLabel);

    filterFreqLabel.setText("Filter Freq", juce::dontSendNotification);
    filterFreqLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(filterFreqLabel);

    tapeDelayOnOffLabel.setText("Tape On/Off", juce::dontSendNotification);
    tapeDelayOnOffLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(tapeDelayOnOffLabel);

    reverbOnOffLabel.setText("Reverb On/Off", juce::dontSendNotification);
    reverbOnOffLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(reverbOnOffLabel);

    delayTimeAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "DelayTime", delayTimeKnob);
    feedbackAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "Feedback", feedbackKnob);
    wowRateAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "WowRate", wowRateKnob);
    wowDepthAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "WowDepth", wowDepthKnob);
    flutterRateAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "FlutterRate", flutterRateKnob);
    flutterDepthAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "FlutterDepth", flutterDepthKnob);
    dryWetAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "DryWet", dryWetKnob);
    reverbLevelAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "ReverbLevel", reverbLevelKnob);
    filterFreqAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "FilterFreq", filterFreqKnob);
    tapeDelayOnOffAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "TapeDelayOnOff", tapeDelayOnOffButton);
    reverbOnOffAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "ReverbOnOff", reverbOnOffButton);

    setSize(900, 350); // Increased width for toggles
}

WalrusDelay1AudioProcessorEditor::~WalrusDelay1AudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void WalrusDelay1AudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkslategrey.darker(0.3f));
    g.setColour(juce::Colours::purple.withAlpha(0.5f));
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(20), 15);
    g.setColour(juce::Colours::lightgrey);
    g.setFont(15.0f);
    g.drawFittedText("WalrusDelay1", getLocalBounds().reduced(10), juce::Justification::centredTop, 1);
}

void WalrusDelay1AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto titleArea = bounds.removeFromTop(50);
    auto knobArea = bounds.reduced(20);

    int knobWidth = (knobArea.getWidth() - (10 * 10)) / 11; // Space for 9 knobs + 2 toggles
    int knobHeight = knobArea.getHeight() - 30;
    int labelHeight = 20;
    int currentX = knobArea.getX();

    delayTimeKnob.setBounds(currentX, knobArea.getY(), knobWidth, knobHeight);
    delayTimeLabel.setBounds(currentX, knobArea.getY() + knobHeight, knobWidth, labelHeight);
    currentX += knobWidth + 10;

    feedbackKnob.setBounds(currentX, knobArea.getY(), knobWidth, knobHeight);
    feedbackLabel.setBounds(currentX, knobArea.getY() + knobHeight, knobWidth, labelHeight);
    currentX += knobWidth + 10;

    wowRateKnob.setBounds(currentX, knobArea.getY(), knobWidth, knobHeight);
    wowRateLabel.setBounds(currentX, knobArea.getY() + knobHeight, knobWidth, labelHeight);
    currentX += knobWidth + 10;

    wowDepthKnob.setBounds(currentX, knobArea.getY(), knobWidth, knobHeight);
    wowDepthLabel.setBounds(currentX, knobArea.getY() + knobHeight, knobWidth, labelHeight);
    currentX += knobWidth + 10;

    flutterRateKnob.setBounds(currentX, knobArea.getY(), knobWidth, knobHeight);
    flutterRateLabel.setBounds(currentX, knobArea.getY() + knobHeight, knobWidth, labelHeight);
    currentX += knobWidth + 10;

    flutterDepthKnob.setBounds(currentX, knobArea.getY(), knobWidth, knobHeight);
    flutterDepthLabel.setBounds(currentX, knobArea.getY() + knobHeight, knobWidth, labelHeight);
    currentX += knobWidth + 10;

    dryWetKnob.setBounds(currentX, knobArea.getY(), knobWidth, knobHeight);
    dryWetLabel.setBounds(currentX, knobArea.getY() + knobHeight, knobWidth, labelHeight);
    currentX += knobWidth + 10;

    reverbLevelKnob.setBounds(currentX, knobArea.getY(), knobWidth, knobHeight);
    reverbLevelLabel.setBounds(currentX, knobArea.getY() + knobHeight, knobWidth, labelHeight);
    currentX += knobWidth + 10;

    filterFreqKnob.setBounds(currentX, knobArea.getY(), knobWidth, knobHeight);
    filterFreqLabel.setBounds(currentX, knobArea.getY() + knobHeight, knobWidth, labelHeight);
    currentX += knobWidth + 10;

    tapeDelayOnOffButton.setBounds(currentX, knobArea.getY(), knobWidth, knobHeight);
    tapeDelayOnOffLabel.setBounds(currentX, knobArea.getY() + knobHeight, knobWidth, labelHeight);
    currentX += knobWidth + 10;

    reverbOnOffButton.setBounds(currentX, knobArea.getY(), knobWidth, knobHeight);
    reverbOnOffLabel.setBounds(currentX, knobArea.getY() + knobHeight, knobWidth, labelHeight);
}

void WalrusDelay1AudioProcessorEditor::setupSlider(juce::Slider& slider, [[maybe_unused]] const juce::String& name)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible(slider);
}

void WalrusDelay1AudioProcessorEditor::setupButton(juce::ToggleButton& button, [[maybe_unused]] const juce::String& name)
{
    button.setButtonText(name);
    addAndMakeVisible(button);
}