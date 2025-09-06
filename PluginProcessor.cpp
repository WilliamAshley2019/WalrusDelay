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
    tapeDelayOnOffParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("TapeDelayOnOff"));
    reverbOnOffParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("ReverbOnOff"));

    jassert(delayTimeParam != nullptr);
    jassert(feedbackParam != nullptr);
    jassert(wowRateParam != nullptr);
    jassert(wowDepthParam != nullptr);
    jassert(flutterRateParam != nullptr);
    jassert(flutterDepthParam != nullptr);
    jassert(dryWetParam != nullptr);
    jassert(reverbLevelParam != nullptr);
    jassert(filterFreqParam != nullptr);
    jassert(tapeDelayOnOffParam != nullptr);
    jassert(reverbOnOffParam != nullptr);

    for (auto& sm : smoothedDelayTime)
        sm.reset(44100, 0.005f);
    smoothedFeedback.reset(44100, 0.05f);
    smoothedDryWet.reset(44100, 0.005f);
    smoothedReverbLevel.reset(44100, 0.05f);
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
    for (auto& buffer : reverbBuffers)
    {
        buffer.createCircularBuffer(static_cast<int>(sampleRate * 0.05f)); // 50ms reverb buffer
        buffer.flushBuffer();
    }

    delayOutput.setSize(2, samplesPerBlock);
    delayOutput.clear();

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
        filter.smoothedFreq.setCurrentAndTargetValue(2000.0f);
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

void WalrusDelay1AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    delayOutput.setSize(totalNumInputChannels, buffer.getNumSamples());
    delayOutput.clear();

    // Process tape delay if enabled
    if (tapeDelayOnOffParam->get())
    {
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
                // Debug filter frequency
                if (sample == 0 && channel == 0)
                    juce::Logger::outputDebugString("Filter Freq: " + juce::String(filterFreqParam->get()) + " Hz");

                // Detect silence for feedback fade
                if (std::abs(inputData[sample]) < 0.00001f)
                {
                    if (counter > 1000)
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

                // Debug modulated delay time
                if (sample == 0 && channel == 0)
                    juce::Logger::outputDebugString("Modulated Delay Time (ms): " + juce::String(targetDelayMs));

                // Read delayed sample
                float delayedSample = delayBuffer.readBuffer(delaySamples);

                // Apply feedback with filtering
                float feedbackSample = feedbackFilter.process(delayedSample);
                float feedbackGain = (feedbackParam->get() == 0.0f) ? 0.0f : smoothedFeedback.getNextValue() * fade.getNextValue();
                // Debug feedback gain
                if (sample == 0 && channel == 0)
                {
                    juce::Logger::outputDebugString("Feedback Gain: " + juce::String(feedbackGain));
                    if (feedbackParam->get() == 0.0f && feedbackGain > 0.0f)
                        juce::Logger::outputDebugString("Warning: Non-zero feedback gain at 0%");
                }
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

            // Debug spikes
            if (counter > 1000 && std::abs(delayData[buffer.getNumSamples() - 1]) > 0.1f)
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
    }
    else
    {
        // Bypass tape delay: copy input to output
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            auto* outputData = buffer.getWritePointer(channel);
            const auto* inputData = buffer.getReadPointer(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                outputData[sample] = inputData[sample];
        }
    }

    // Process reverb if enabled
    if (reverbOnOffParam->get())
    {
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            auto& reverbBuffer = reverbBuffers[channel];
            auto& reverbFilter = reverbFilters[channel];
            auto* outputData = buffer.getWritePointer(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                reverbFilter.update(2000.0f);
                float reverbSample = reverbBuffer.readBuffer(reverbDelayMs * getSampleRate() / 1000.0f);
                reverbSample = reverbFilter.process(reverbSample);
                float reverbInput = outputData[sample];
                reverbBuffer.writeBuffer(reverbInput);
                float reverbMix = smoothedReverbLevel.getNextValue();
                outputData[sample] = outputData[sample] * (1.0f - reverbMix) + reverbSample * reverbMix;
                // Debug reverb mix
                if (sample == 0 && channel == 0)
                    juce::Logger::outputDebugString("Reverb Mix: " + juce::String(reverbMix));
            }
        }
    }
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
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f, 0.5f), 0.5f,
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

    layout.add(std::make_unique<juce::AudioParameterFloat>("ReverbLevel", "Reverb Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f,
        percentageAttributes));

    layout.add(std::make_unique<juce::AudioParameterFloat>("FilterFreq", "Filter Freq",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.25f), 2000.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float value, int) { return juce::String((int)value) + " Hz"; })));

    layout.add(std::make_unique<juce::AudioParameterBool>("TapeDelayOnOff", "Tape Delay On/Off", true));
    layout.add(std::make_unique<juce::AudioParameterBool>("ReverbOnOff", "Reverb On/Off", false));

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