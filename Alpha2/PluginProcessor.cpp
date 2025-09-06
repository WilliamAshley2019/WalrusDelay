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
    reverbSizeParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("ReverbSize"));
    reverbDampParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("ReverbDamp"));
    reverbWidthParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("ReverbWidth"));
    reverbMixParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("ReverbMix"));
    reverbFreezeParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("ReverbFreeze"));
    filterFreqParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("FilterFreq"));

    jassert(delayTimeParam != nullptr); // Safety checks
    jassert(feedbackParam != nullptr);
    jassert(wowRateParam != nullptr);
    jassert(wowDepthParam != nullptr);
    jassert(flutterRateParam != nullptr);
    jassert(flutterDepthParam != nullptr);
    jassert(dryWetParam != nullptr);
    jassert(reverbSizeParam != nullptr);
    jassert(reverbDampParam != nullptr);
    jassert(reverbWidthParam != nullptr);
    jassert(reverbMixParam != nullptr);
    jassert(reverbFreezeParam != nullptr);
    jassert(filterFreqParam != nullptr);

    for (auto& sm : smoothedDelayTime)
        sm.reset(44100, 0.005f);
    smoothedFeedback.reset(44100, 0.05f);
    smoothedDryWet.reset(44100, 0.005f);
    smoothedReverbMix.reset(44100, 0.05f);
    for (auto& fade : feedbackFade)
        fade.reset(44100, 0.2f);
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

    delayOutput.setSize(2, samplesPerBlock);
    delayOutput.clear();

    juce::dsp::ProcessSpec spec{ sampleRate, static_cast<juce::uint32>(samplesPerBlock), 2 };
    wowOscillator.prepare(spec);
    flutterOscillator.prepare(spec);
    wowOscillator.setFrequency(wowRateParam->get());
    flutterOscillator.setFrequency(flutterRateParam->get());
    reverb.prepare(spec);

    for (auto& filter : feedbackFilters)
    {
        filter.prepare(sampleRate);
        filter.smoothedFreq.setCurrentAndTargetValue(filterFreqParam->get());
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
    smoothedReverbMix.reset(sampleRate, 0.05f);
    smoothedReverbMix.setCurrentAndTargetValue(reverbMixParam->get());
    for (auto& fade : feedbackFade)
    {
        fade.reset(sampleRate, 0.2f);
        fade.setCurrentAndTargetValue(1.0f);
    }
    for (auto& counter : silenceCounter)
        counter = 0;

    // Initialize reverb parameters
    reverbParams.roomSize = reverbSizeParam->get();
    reverbParams.damping = reverbDampParam->get();
    reverbParams.width = reverbWidthParam->get();
    reverbParams.wetLevel = reverbMixParam->get();
    reverbParams.dryLevel = 1.0f - reverbMixParam->get();
    reverbParams.freezeMode = reverbFreezeParam->get();
    reverb.setParameters(reverbParams);
}

void WalrusDelay1AudioProcessor::releaseResources()
{
    for (auto& buffer : delayBuffers)
        buffer.flushBuffer();
    for (auto& fade : feedbackFade)
        fade.setTargetValue(0.0f);
    reverb.reset();
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

    delayOutput.setSize(totalNumInputChannels, buffer.getNumSamples());
    delayOutput.clear();

    // Update reverb parameters
    reverbParams.roomSize = reverbSizeParam->get();
    reverbParams.damping = reverbDampParam->get();
    reverbParams.width = reverbWidthParam->get();
    reverbParams.wetLevel = reverbMixParam->get();
    reverbParams.dryLevel = 1.0f - reverbMixParam->get();
    reverbParams.freezeMode = reverbFreezeParam->get();
    reverb.setParameters(reverbParams);

    // Process delay for each channel
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto& delayBuffer = delayBuffers[channel];
        auto& feedbackFilter = feedbackFilters[channel];
        auto& delaySmoother = smoothedDelayTime[channel];
        auto& fade = feedbackFade[channel];
        auto& counter = silenceCounter[channel];
        auto* inputData = buffer.getReadPointer(channel);
        auto* delayData = delayOutput.getWritePointer(channel);

        float wowPhase = (channel == 0) ? wowPhaseLeft : wowPhaseRight;
        float flutterPhase = (channel == 0) ? flutterPhaseLeft : flutterPhaseRight;

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            // Update filter
            feedbackFilter.update(filterFreqParam->get());

            // Detect silence for feedback fade
            if (std::abs(inputData[sample]) < 0.00001f)
            {
                if (counter > 100)
                    fade.setTargetValue(0.0f);
                counter++;
            }
            else
            {
                counter = 0;
                fade.setTargetValue(1.0f);
            }

            // Wow and flutter modulation
            float wowMod = wowOscillator.processSample(0.0f) * wowDepthParam->get() * 0.01f;
            wowPhase += 2.0f * juce::MathConstants<float>::pi * wowRateParam->get() / static_cast<float>(getSampleRate());
            if (wowPhase > 2.0f * juce::MathConstants<float>::pi)
                wowPhase -= 2.0f * juce::MathConstants<float>::pi;

            float flutterMod = flutterOscillator.processSample(0.0f) * flutterDepthParam->get() * 0.01f;
            flutterPhase += 2.0f * juce::MathConstants<float>::pi * flutterRateParam->get() / static_cast<float>(getSampleRate());
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

            // Store delay output
            delayData[sample] = delayedSample;

            // Update phases
            if (channel == 0)
            {
                wowPhaseLeft = wowPhase;
                flutterPhaseLeft = flutterPhase;
            }
            else
            {
                wowPhaseRight = wowPhase;
                flutterPhaseRight = flutterPhase;
            }
        }

        // Log spikes only if detected
        if (counter > 100 && std::abs(delayData[buffer.getNumSamples() - 1]) > 0.1f)
            juce::Logger::outputDebugString("Spike detected on channel " + juce::String(channel) + ": " + juce::String(delayData[buffer.getNumSamples() - 1]));
    }

    // Mix delay output
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        const auto* inputData = buffer.getReadPointer(channel);
        const auto* delayData = delayOutput.getReadPointer(channel);
        auto* outputData = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float dryMix = 1.0f - smoothedDryWet.getNextValue();
            float wetMix = smoothedDryWet.getNextValue();
            outputData[sample] = inputData[sample] * dryMix + delayData[sample] * wetMix;
        }
    }

    // Apply reverb
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    reverb.process(ctx);
}

juce::AudioProcessorValueTreeState::ParameterLayout WalrusDelay1AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const auto percentageAttributes = juce::AudioParameterFloatAttributes().withStringFromValueFunction(
        [](float value, int) { return juce::String(value * 100.0f, 1) + " %"; });

    layout.add(std::make_unique<juce::AudioParameterFloat>("DelayTime", "Delay Time",
        juce::NormalisableRange<float>(50.0f, 500.0f, 1.0f), 100.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float value, int) { return juce::String((int)value) + " ms"; })));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Feedback", "Feedback",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f,
        percentageAttributes));

    layout.add(std::make_unique<juce::AudioParameterFloat>("WowRate", "Wow Rate",
        juce::NormalisableRange<float>(0.1f, 5.0f, 0.01f), 1.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float value, int) { return juce::String(value, 2) + " Hz"; })));

    layout.add(std::make_unique<juce::AudioParameterFloat>("WowDepth", "Wow Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.2f,
        percentageAttributes));

    layout.add(std::make_unique<juce::AudioParameterFloat>("FlutterRate", "Flutter Rate",
        juce::NormalisableRange<float>(0.1f, 20.0f, 0.1f), 10.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float value, int) { return juce::String(value, 2) + " Hz"; })));

    layout.add(std::make_unique<juce::AudioParameterFloat>("FlutterDepth", "Flutter Depth",
        juce::NormalisableRange<float>(0.0f, 0.4f, 0.01f), 0.1f,
        percentageAttributes));

    layout.add(std::make_unique<juce::AudioParameterFloat>("DryWet", "Dry/Wet Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f,
        percentageAttributes));

    layout.add(std::make_unique<juce::AudioParameterFloat>("ReverbSize", "Reverb Size",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f,
        percentageAttributes));

    layout.add(std::make_unique<juce::AudioParameterFloat>("ReverbDamp", "Reverb Damp",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f,
        percentageAttributes));

    layout.add(std::make_unique<juce::AudioParameterFloat>("ReverbWidth", "Reverb Width",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f,
        percentageAttributes));

    layout.add(std::make_unique<juce::AudioParameterFloat>("ReverbMix", "Reverb Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f,
        percentageAttributes));

    layout.add(std::make_unique<juce::AudioParameterBool>("ReverbFreeze", "Reverb Freeze", false));

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