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
    setupSlider(filterFreqKnob, "Filter Freq");
    setupSlider(reverbSizeKnob, "Reverb Size");
    setupSlider(reverbDampKnob, "Reverb Damp");
    setupSlider(reverbWidthKnob, "Reverb Width");
    setupSlider(reverbMixKnob, "Reverb Mix");
    setupButton(reverbFreezeButton, "Reverb Freeze");

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

    filterFreqLabel.setText("Filter Freq", juce::dontSendNotification);
    filterFreqLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(filterFreqLabel);

    reverbSizeLabel.setText("Size", juce::dontSendNotification);
    reverbSizeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(reverbSizeLabel);

    reverbDampLabel.setText("Damp", juce::dontSendNotification);
    reverbDampLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(reverbDampLabel);

    reverbWidthLabel.setText("Width", juce::dontSendNotification);
    reverbWidthLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(reverbWidthLabel);

    reverbMixLabel.setText("Mix", juce::dontSendNotification);
    reverbMixLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(reverbMixLabel);

    reverbFreezeLabel.setText("Freeze", juce::dontSendNotification);
    reverbFreezeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(reverbFreezeLabel);

    delayTimeAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "DelayTime", delayTimeKnob);
    feedbackAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "Feedback", feedbackKnob);
    wowRateAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "WowRate", wowRateKnob);
    wowDepthAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "WowDepth", wowDepthKnob);
    flutterRateAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "FlutterRate", flutterRateKnob);
    flutterDepthAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "FlutterDepth", flutterDepthKnob);
    dryWetAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "DryWet", dryWetKnob);
    filterFreqAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "FilterFreq", filterFreqKnob);
    reverbSizeAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "ReverbSize", reverbSizeKnob);
    reverbDampAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "ReverbDamp", reverbDampKnob);
    reverbWidthAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "ReverbWidth", reverbWidthKnob);
    reverbMixAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "ReverbMix", reverbMixKnob);
    reverbFreezeAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "ReverbFreeze", reverbFreezeButton);

    setSize(1000, 350); // Increased width for 13 controls
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

    int knobWidth = (knobArea.getWidth() - (12 * 10)) / 13; // Space for 13 controls
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

    filterFreqKnob.setBounds(currentX, knobArea.getY(), knobWidth, knobHeight);
    filterFreqLabel.setBounds(currentX, knobArea.getY() + knobHeight, knobWidth, labelHeight);
    currentX += knobWidth + 10;

    reverbSizeKnob.setBounds(currentX, knobArea.getY(), knobWidth, knobHeight);
    reverbSizeLabel.setBounds(currentX, knobArea.getY() + knobHeight, knobWidth, labelHeight);
    currentX += knobWidth + 10;

    reverbDampKnob.setBounds(currentX, knobArea.getY(), knobWidth, knobHeight);
    reverbDampLabel.setBounds(currentX, knobArea.getY() + knobHeight, knobWidth, labelHeight);
    currentX += knobWidth + 10;

    reverbWidthKnob.setBounds(currentX, knobArea.getY(), knobWidth, knobHeight);
    reverbWidthLabel.setBounds(currentX, knobArea.getY() + knobHeight, knobWidth, labelHeight);
    currentX += knobWidth + 10;

    reverbMixKnob.setBounds(currentX, knobArea.getY(), knobWidth, knobHeight);
    reverbMixLabel.setBounds(currentX, knobArea.getY() + knobHeight, knobWidth, labelHeight);
    currentX += knobWidth + 10;

    reverbFreezeButton.setBounds(currentX, knobArea.getY(), knobWidth, knobHeight);
    reverbFreezeLabel.setBounds(currentX, knobArea.getY() + knobHeight, knobWidth, labelHeight);
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