/*
  ==============================================================================

    PluginProcessor.cpp
    Created: 5 Sep 2025


  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
WalrusDelay1AudioProcessor::WalrusDelay1AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    ),
    apvts(*this, nullptr, "Parameters", createParameterLayout())
#endif
{
    delayTimeParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("DelayTime"));
    feedbackParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("Feedback"));
    wowRateParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("WowRate"));
    wowDepthParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("WowDepth"));
    flutterRateParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("FlutterRate"));
    flutterDepthParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("FlutterDepth"));
    dryWetParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("DryWet"));
    reverbLevelParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("ReverbLevel"));
    filterFreqParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("FilterFreq"));

    for (auto& sm : smoothedDelayTime)
        sm.reset(44100, 0.005f);
    smoothedFeedback.reset(44100, 0.05f);
    smoothedDryWet.reset(44100, 0.005f);
    smoothedReverbLevel.reset(44100, 0.05f); // Increased for smoother reverb
    for (auto& fade : feedbackFade)
        fade.reset(44100, 0.2f); // 200ms fade-out
    for (auto& counter : silenceCounter)
        counter = 0;
}

WalrusDelay1AudioProcessor::~WalrusDelay1AudioProcessor()
{
}

//==============================================================================
const juce::String WalrusDelay1AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool WalrusDelay1AudioProcessor::acceptsMidi() const { return false; }
bool WalrusDelay1AudioProcessor::producesMidi() const { return false; }
bool WalrusDelay1AudioProcessor::isMidiEffect() const { return false; }
double WalrusDelay1AudioProcessor::getTailLengthSeconds() const { return 0.0; }
int WalrusDelay1AudioProcessor::getNumPrograms() { return 1; }
int WalrusDelay1AudioProcessor::getCurrentProgram() { return 0; }
void WalrusDelay1AudioProcessor::setCurrentProgram(int) {}
const juce::String WalrusDelay1AudioProcessor::getProgramName(int) { return {}; }
void WalrusDelay1AudioProcessor::changeProgramName(int, const juce::String&) {}

//==============================================================================
void WalrusDelay1AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    maxDelaySamples = static_cast<int>(sampleRate * 2.0); // 2 seconds max
    for (auto& buffer : delayBuffers)
    {
        buffer.createCircularBuffer(maxDelaySamples);
        buffer.flushBuffer();
    }
    for (auto& buffer : reverbBuffers)
    {
        buffer.createCircularBuffer(static_cast<int>(sampleRate * 0.1f)); // 100ms reverb buffer
        buffer.flushBuffer();
    }

    juce::dsp::ProcessSpec spec{ sampleRate, static_cast<juce::uint32>(samplesPerBlock), 2 };
    wowOscillator.prepare(spec);
    flutterOscillator.prepare(spec);
    wowOscillator.setFrequency(wowRateParam->get());
    flutterOscillator.setFrequency(flutterRateParam->get());

    for (auto& filter : feedbackFilters)
    {
        filter.prepare(sampleRate);
        filter.smoothedFreq.setCurrentAndTargetValue(filterFreqParam->get());
    }
    for (auto& filter : reverbFilters)
    {
        filter.prepare(sampleRate);
        filter.smoothedFreq.setCurrentAndTargetValue(4000.0f);
    }

    for (auto& sm : smoothedDelayTime)
    {
        sm.reset(sampleRate, 0.005f);
        sm.setCurrentAndTargetValue(delayTimeParam->get());
    }
    smoothedFeedback.reset(sampleRate, 0.05f);
    smoothedFeedback.setCurrentAndTargetValue(feedbackParam->get());
    smoothedDryWet.reset(sampleRate, 0.005f);
    smoothedDryWet.setCurrentAndTargetValue(dryWetParam->get());
    smoothedReverbLevel.reset(sampleRate, 0.05f);
    smoothedReverbLevel.setCurrentAndTargetValue(reverbLevelParam->get());
    for (auto& fade : feedbackFade)
    {
        fade.reset(sampleRate, 0.2f);
        fade.setCurrentAndTargetValue(1.0f);
    }
    for (auto& counter : silenceCounter)
        counter = 0;
}

void WalrusDelay1AudioProcessor::releaseResources()
{
    for (auto& buffer : delayBuffers)
        buffer.flushBuffer();
    for (auto& buffer : reverbBuffers)
        buffer.flushBuffer();
    for (auto& fade : feedbackFade)
        fade.setTargetValue(0.0f);
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool WalrusDelay1AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo() ||
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}
#endif

void WalrusDelay1AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    wowOscillator.setFrequency(wowRateParam->get());
    flutterOscillator.setFrequency(flutterRateParam->get());

    smoothedFeedback.setTargetValue(juce::jlimit(0.0f, 0.7f, feedbackParam->get()));
    smoothedDryWet.setTargetValue(juce::jlimit(0.0f, 1.0f, dryWetParam->get()));
    smoothedReverbLevel.setTargetValue(juce::jlimit(0.0f, 0.5f, reverbLevelParam->get()));

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        const auto* inputData = buffer.getReadPointer(channel);
        auto* outputData = buffer.getWritePointer(channel);
        auto& delayBuffer = delayBuffers[channel];
        auto& feedbackFilter = feedbackFilters[channel];
        auto& reverbBuffer = reverbBuffers[channel];
        auto& reverbFilter = reverbFilters[channel];
        auto& delaySmoother = smoothedDelayTime[channel];
        auto& fade = feedbackFade[channel];
        auto& counter = silenceCounter[channel];
        float& wowPhase = (channel == 0) ? wowPhaseLeft : wowPhaseRight;
        float& flutterPhase = (channel == 0) ? flutterPhaseLeft : flutterPhaseRight;

        delaySmoother.setTargetValue(delayTimeParam->get());
        feedbackFilter.update(filterFreqParam->get());

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            // Detect silence
            bool isSilent = std::abs(inputData[sample]) < 1e-6f;
            counter = isSilent ? counter + 1 : 0;
            fade.setTargetValue(counter > 100 ? 0.0f : 1.0f); // Fade after ~2ms silence

            // LFO Modulation
            float wowMod = std::sin(wowPhase) * wowDepthParam->get();
            wowPhase += static_cast<float>(2.0f * juce::MathConstants<float>::pi * wowRateParam->get() / getSampleRate());
            if (wowPhase > 2.0f * juce::MathConstants<float>::pi)
                wowPhase -= 2.0f * juce::MathConstants<float>::pi;

            float flutterMod = std::sin(flutterPhase) * flutterDepthParam->get();
            flutterPhase += static_cast<float>(2.0f * juce::MathConstants<float>::pi * flutterRateParam->get() / getSampleRate());
            if (flutterPhase > 2.0f * juce::MathConstants<float>::pi)
                flutterPhase -= 2.0f * juce::MathConstants<float>::pi;

            float targetDelayMs = delaySmoother.getNextValue() * (1.0f + wowMod + flutterMod);
            targetDelayMs = juce::jlimit(1.0f, 2000.0f, targetDelayMs);
            float delaySamples = static_cast<float>((targetDelayMs / 1000.0f) * getSampleRate());

            // Read delayed sample
            float delayedSample = delayBuffer.readBuffer(delaySamples);

            // Apply feedback with filtering and fade
            float feedbackSample = feedbackFilter.process(delayedSample);
            float feedbackGain = smoothedFeedback.getNextValue() * fade.getNextValue() * 0.9f;
            float inputForDelay = inputData[sample] + feedbackSample * feedbackGain;
            inputForDelay = juce::jlimit(-1.0f, 1.0f, inputForDelay);
            delayBuffer.writeBuffer(inputForDelay);

            // Reverb (single-tap)
            float reverbSample = reverbBuffer.readBuffer(reverbDelayMs * static_cast<float>(getSampleRate()) / 1000.0f);
            reverbSample = reverbFilter.process(reverbSample);
            reverbBuffer.writeBuffer(inputForDelay * 0.2f + reverbSample * 0.6f); // Reduced decay and input

            // Mix output
            float dryMix = 1.0f - smoothedDryWet.getNextValue();
            float wetMix = smoothedDryWet.getNextValue();
            float outputSample = inputData[sample] * dryMix + delayedSample * wetMix;
            outputSample += reverbSample * smoothedReverbLevel.getNextValue();
            outputSample = juce::jlimit(-1.0f, 1.0f, outputSample);

            outputData[sample] = outputSample;
        }

        // Log spikes only if detected
        if (counter > 100 && std::abs(outputData[buffer.getNumSamples() - 1]) > 0.1f)
            juce::Logger::outputDebugString("Spike detected on channel " + juce::String(channel) + ": " + juce::String(outputData[buffer.getNumSamples() - 1]));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout WalrusDelay1AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>("DelayTime", "Delay Time",
        juce::NormalisableRange<float>(50.0f, 500.0f, 1.0f), 100.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float value, int) { return juce::String((int)value) + " ms"; })));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Feedback", "Feedback",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float value, int) { return juce::String(value * 100.0f, 1) + " %"; })));

    layout.add(std::make_unique<juce::AudioParameterFloat>("WowRate", "Wow Rate",
        juce::NormalisableRange<float>(0.1f, 5.0f, 0.01f), 1.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float value, int) { return juce::String(value, 2) + " Hz"; })));

    layout.add(std::make_unique<juce::AudioParameterFloat>("WowDepth", "Wow Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.2f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float value, int) { return juce::String(value * 100.0f, 1) + " %"; })));

    layout.add(std::make_unique<juce::AudioParameterFloat>("FlutterRate", "Flutter Rate",
        juce::NormalisableRange<float>(0.1f, 20.0f, 0.1f), 10.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float value, int) { return juce::String(value, 2) + " Hz"; })));

    layout.add(std::make_unique<juce::AudioParameterFloat>("FlutterDepth", "Flutter Depth",
        juce::NormalisableRange<float>(0.0f, 0.4f, 0.01f), 0.1f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float value, int) { return juce::String(value * 100.0f, 1) + " %"; })));

    layout.add(std::make_unique<juce::AudioParameterFloat>("DryWet", "Dry/Wet Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float value, int) { return juce::String(value * 100.0f, 1) + " %"; })));

    layout.add(std::make_unique<juce::AudioParameterFloat>("ReverbLevel", "Reverb Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float value, int) { return juce::String(value * 100.0f, 1) + " %"; })));

    layout.add(std::make_unique<juce::AudioParameterFloat>("FilterFreq", "Filter Freq",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.25f), 2000.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float value, int) { return juce::String((int)value) + " Hz"; })));

    return layout;
}

juce::AudioProcessorEditor* WalrusDelay1AudioProcessor::createEditor()
{
    return new WalrusDelay1AudioProcessorEditor(*this);
}

bool WalrusDelay1AudioProcessor::hasEditor() const
{
    return true;
}

void WalrusDelay1AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void WalrusDelay1AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if (tree.isValid())
    {
        apvts.replaceState(tree);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WalrusDelay1AudioProcessor();
}