/*
  ==============================================================================

    PluginEditor.h
    Created: 11 Jan 2026
    Updated for JUCE 8.0.12

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
class WalrusLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
        juce::Slider& slider) override;

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

//==============================================================================
class WalrusDelay1AudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    WalrusDelay1AudioProcessorEditor(WalrusDelay1AudioProcessor&);
    ~WalrusDelay1AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    WalrusDelay1AudioProcessor& audioProcessor;

    // Knobs
    juce::Slider delayTimeKnob, feedbackKnob, wowRateKnob, wowDepthKnob,
        flutterRateKnob, flutterDepthKnob, dryWetKnob,
        reverbLevelKnob, filterFreqKnob, saturationKnob;

    // Buttons
    juce::ToggleButton tapeDelayOnOffButton, reverbOnOffButton, psychedelicModeButton;

    // Labels
    juce::Label delayTimeLabel, feedbackLabel, wowRateLabel, wowDepthLabel,
        flutterRateLabel, flutterDepthLabel, dryWetLabel, reverbLevelLabel,
        filterFreqLabel, saturationLabel, tapeDelayOnOffLabel,
        reverbOnOffLabel, psychedelicModeLabel;

    // Attachments
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> delayTimeAttachment, feedbackAttachment, wowRateAttachment,
        wowDepthAttachment, flutterRateAttachment, flutterDepthAttachment,
        dryWetAttachment, reverbLevelAttachment, filterFreqAttachment,
        saturationAttachment;

    std::unique_ptr<ButtonAttachment> tapeDelayOnOffAttachment, reverbOnOffAttachment, psychedelicModeAttachment;

    WalrusLookAndFeel walrusLookAndFeel;

    void setupKnob(juce::Slider& slider, const juce::String& name, juce::Colour colour = juce::Colours::white);
    void setupButton(juce::ToggleButton& button, const juce::String& name, juce::Colour colour = juce::Colours::white);
    void createLabel(juce::Label& label, const juce::String& text);

    void placeControl(juce::Component& control, juce::Label& label,
        int x, int y, int width, int height, int labelHeight);
    void placeControl(juce::ToggleButton& control, juce::Label& label,
        int x, int y, int width, int height, int labelHeight);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WalrusDelay1AudioProcessorEditor)
};