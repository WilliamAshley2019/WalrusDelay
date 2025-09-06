/*
  ==============================================================================

    PluginEditor.h
    Created: 5 Sep 2025
 
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
        float sliderPos, const float rotaryStartAngle, const float rotaryEndAngle,
        [[maybe_unused]] juce::Slider& slider) override;
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

    juce::Slider delayTimeKnob, feedbackKnob, wowRateKnob, wowDepthKnob, flutterRateKnob, flutterDepthKnob, dryWetKnob, reverbLevelKnob, filterFreqKnob;
    juce::Label delayTimeLabel, feedbackLabel, wowRateLabel, wowDepthLabel, flutterRateLabel, flutterDepthLabel, dryWetLabel, reverbLevelLabel, filterFreqLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> delayTimeAttachment, feedbackAttachment, wowRateAttachment, wowDepthAttachment,
        flutterRateAttachment, flutterDepthAttachment, dryWetAttachment, reverbLevelAttachment, filterFreqAttachment;

    WalrusLookAndFeel walrusLookAndFeel;

    void setupSlider(juce::Slider& slider, [[maybe_unused]] const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WalrusDelay1AudioProcessorEditor)
};