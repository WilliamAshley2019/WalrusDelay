/*
  ==============================================================================

    PluginProcessor.cpp
    Created: 11 Jan 2026
    Updated for JUCE 8.0.12

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
    saturationParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("Saturation"));
    tapeDelayOnOffParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("TapeDelayOnOff"));
    reverbOnOffParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("ReverbOnOff"));
    psychedelicModeParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("PsychedelicMode"));

    jassert(delayTimeParam != nullptr);
    jassert(feedbackParam != nullptr);
    jassert(wowRateParam != nullptr);
    jassert(wowDepthParam != nullptr);
    jassert(flutterRateParam != nullptr);
    jassert(flutterDepthParam != nullptr);
    jassert(dryWetParam != nullptr);
    jassert(reverbLevelParam != nullptr);
    jassert(filterFreqParam != nullptr);
    jassert(saturationParam != nullptr);
    jassert(tapeDelayOnOffParam != nullptr);
    jassert(reverbOnOffParam != nullptr);
    jassert(psychedelicModeParam != nullptr);

    smoothedDelayTime.reset(44100, 0.005);
    smoothedFeedback.reset(44100, 0.05);
    smoothedDryWet.reset(44100, 0.005);
    smoothedReverbLevel.reset(44100, 0.05);
    smoothedFilterFreq.reset(44100, 0.05);
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
double WalrusDelay1AudioProcessor::getTailLengthSeconds() const { return 2.0; }
int WalrusDelay1AudioProcessor::getNumPrograms() { return 1; }
int WalrusDelay1AudioProcessor::getCurrentProgram() { return 0; }
void WalrusDelay1AudioProcessor::setCurrentProgram(int) {}
const juce::String WalrusDelay1AudioProcessor::getProgramName(int) { return {}; }
void WalrusDelay1AudioProcessor::changeProgramName(int, const juce::String&) {}

//==============================================================================
void WalrusDelay1AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentSamplesPerBlock = samplesPerBlock;

    const int maxDelaySamples = static_cast<int>(sampleRate * 3.0);

    // Prepare tape delays
    for (auto& delay : tapeDelays)
    {
        delay.prepare(sampleRate, maxDelaySamples);
    }

    // Prepare LFOs
    wowLFO.prepare({ sampleRate, static_cast<juce::uint32>(samplesPerBlock), 2 });
    flutterLFO.prepare({ sampleRate, static_cast<juce::uint32>(samplesPerBlock), 2 });
    wowLFO.setFrequency(wowRateParam->get());
    flutterLFO.setFrequency(flutterRateParam->get());

    // Prepare filters
    for (auto& filter : feedbackFilters)
    {
        filter.prepare(sampleRate);
        filter.setCutoff(filterFreqParam->get());
    }

    // Reset smoothing
    smoothedDelayTime.reset(sampleRate, 0.005);
    smoothedDelayTime.setCurrentAndTargetValue(delayTimeParam->get());
    smoothedFeedback.reset(sampleRate, 0.05);
    smoothedFeedback.setCurrentAndTargetValue(feedbackParam->get());
    smoothedDryWet.reset(sampleRate, 0.005);
    smoothedDryWet.setCurrentAndTargetValue(dryWetParam->get());
    smoothedReverbLevel.reset(sampleRate, 0.05);
    smoothedReverbLevel.setCurrentAndTargetValue(reverbLevelParam->get());
    smoothedFilterFreq.reset(sampleRate, 0.05);
    smoothedFilterFreq.setCurrentAndTargetValue(filterFreqParam->get());

    // Prepare buffers
    delayBuffer.setSize(2, samplesPerBlock);
    wetBuffer.setSize(2, samplesPerBlock);
}

void WalrusDelay1AudioProcessor::releaseResources()
{
    for (auto& delay : tapeDelays)
    {
        delay.reset();
    }
}

bool WalrusDelay1AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo() ||
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void WalrusDelay1AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    auto numSamples = buffer.getNumSamples();

    // Clear unused channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    // Update LFO frequencies
    wowLFO.setFrequency(wowRateParam->get());
    flutterLFO.setFrequency(flutterRateParam->get());

    // Update filter cutoff
    float filterCutoff = smoothedFilterFreq.getNextValue();
    if (psychedelicModeParam->get())
    {
        filterCutoff *= 1.5f;
    }
    for (auto& filter : feedbackFilters)
    {
        filter.setCutoff(filterCutoff);
    }

    // Clear buffers
    delayBuffer.clear();
    wetBuffer.clear();

    // Process tape delay if enabled
    if (tapeDelayOnOffParam->get())
    {
        // Generate LFO modulation buffers
        juce::AudioBuffer<float> wowBuffer(2, numSamples);
        juce::AudioBuffer<float> flutterBuffer(2, numSamples);

        // Fill LFO buffers
        for (int channel = 0; channel < 2; ++channel)
        {
            auto* wowData = wowBuffer.getWritePointer(channel);
            auto* flutterData = flutterBuffer.getWritePointer(channel);

            // Reset LFO phase for consistent stereo
            wowLFO.reset();
            flutterLFO.reset();

            for (int sample = 0; sample < numSamples; ++sample)
            {
                wowData[sample] = wowLFO.processSample(0.0f);
                flutterData[sample] = flutterLFO.processSample(0.0f);
            }
        }

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            auto* inputData = buffer.getReadPointer(channel);
            auto* delayData = delayBuffer.getWritePointer(channel);
            auto* wowData = wowBuffer.getReadPointer(channel);
            auto* flutterData = flutterBuffer.getReadPointer(channel);

            // Process each sample
            for (int sample = 0; sample < numSamples; ++sample)
            {
                // Calculate modulated delay time
                float baseDelayMs = smoothedDelayTime.getNextValue();
                float wowMod = wowData[sample] * wowDepthParam->get() * 0.1f;
                float flutterMod = flutterData[sample] * flutterDepthParam->get() * 0.05f;
                float modulatedDelayMs = baseDelayMs * (1.0f + wowMod + flutterMod);
                float modulatedDelaySamples = (modulatedDelayMs / 1000.0f) * currentSampleRate;

                // Set delay time
                tapeDelays[channel].setDelay(modulatedDelaySamples);

                // Create saturation function
                float saturationAmount = saturationParam->get();
                auto saturationFunc = [saturationAmount, this](float x) -> float {
                    if (psychedelicModeParam->get())
                    {
                        return TapeSaturation::tubeWarmth(x, saturationAmount * 1.5f);
                    }
                    else
                    {
                        return TapeSaturation::softClip(x * (1.0f + saturationAmount * 0.5f));
                    }
                    };

                float input = inputData[sample];
                float delayed = tapeDelays[channel].process(
                    input,
                    smoothedFeedback.getNextValue(),
                    saturationFunc
                );

                delayed = feedbackFilters[channel].process(delayed);
                delayData[sample] = delayed;

                float dryMix = 1.0f - smoothedDryWet.getNextValue();
                float wetMix = smoothedDryWet.getNextValue();
                wetBuffer.setSample(channel, sample,
                    input * dryMix + delayed * wetMix);
            }
        }

        // Copy wet buffer to main buffer
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            buffer.copyFrom(channel, 0, wetBuffer, channel, 0, numSamples);
        }
    }

    // Process reverb if enabled (simplified reverb)
    if (reverbOnOffParam->get())
    {
        float reverbMix = smoothedReverbLevel.getNextValue();
        if (psychedelicModeParam->get())
        {
            reverbMix *= 1.2f;
        }

        // Simple feedback delay for reverb
        static std::array<juce::dsp::DelayLine<float>, 2> reverbDelays;
        static bool reverbInitialized = false;

        if (!reverbInitialized)
        {
            for (auto& delay : reverbDelays)
            {
                delay.prepare({ currentSampleRate, static_cast<juce::uint32>(currentSamplesPerBlock), 2 });
                delay.setMaximumDelayInSamples(static_cast<int>(currentSampleRate * 0.1f)); // 100ms
                delay.reset();
            }
            reverbInitialized = true;
        }

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            auto* data = buffer.getWritePointer(channel);
            auto& reverbDelay = reverbDelays[channel];

            for (int sample = 0; sample < numSamples; ++sample)
            {
                float input = data[sample];
                float reverbOut = reverbDelay.popSample(channel, 80.0f, true); // 80 sample delay
                reverbDelay.pushSample(channel, input + reverbOut * 0.6f);
                data[sample] = input * (1.0f - reverbMix) + reverbOut * reverbMix;
            }
        }
    }

    // Apply psychedelic mode effects
    if (psychedelicModeParam->get())
    {
        auto& random = juce::Random::getSystemRandom();
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            auto* data = buffer.getWritePointer(channel);
            for (int sample = 0; sample < numSamples; ++sample)
            {
                // Subtle tape noise
                float noise = (random.nextFloat() - 0.5f) * 0.0002f;
                data[sample] += noise;

                // Gentle tape compression
                data[sample] = std::tanh(data[sample] * 0.8f) / 0.8f;

                // Very subtle pitch wobble
                static float phase = 0.0f;
                float wobble = std::sin(phase) * 0.002f;
                data[sample] *= (1.0f + wobble);
                phase += 0.5f * 2.0f * juce::MathConstants<float>::pi / currentSampleRate;
                if (phase > 2.0f * juce::MathConstants<float>::pi)
                    phase -= 2.0f * juce::MathConstants<float>::pi;
            }
        }
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout WalrusDelay1AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const auto percentageAttributes = juce::AudioParameterFloatAttributes().withStringFromValueFunction(
        [](float value, int) { return juce::String(value * 100.0f, 1) + " %"; });

    // Delay parameters
    layout.add(std::make_unique<juce::AudioParameterFloat>("DelayTime", "Delay Time",
        juce::NormalisableRange<float>(50.0f, 3000.0f, 1.0f, 0.3f), 500.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float value, int) { return juce::String((int)value) + " ms"; })));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Feedback", "Feedback",
        juce::NormalisableRange<float>(0.0f, 0.95f, 0.01f, 0.5f), 0.5f,
        percentageAttributes));

    // Wow and Flutter
    layout.add(std::make_unique<juce::AudioParameterFloat>("WowRate", "Wow Rate",
        juce::NormalisableRange<float>(0.1f, 2.0f, 0.01f, 0.5f), 0.5f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float value, int) { return juce::String(value, 2) + " Hz"; })));

    layout.add(std::make_unique<juce::AudioParameterFloat>("WowDepth", "Wow Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.3f,
        percentageAttributes));

    layout.add(std::make_unique<juce::AudioParameterFloat>("FlutterRate", "Flutter Rate",
        juce::NormalisableRange<float>(5.0f, 50.0f, 0.1f, 0.5f), 15.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float value, int) { return juce::String(value, 2) + " Hz"; })));

    layout.add(std::make_unique<juce::AudioParameterFloat>("FlutterDepth", "Flutter Depth",
        juce::NormalisableRange<float>(0.0f, 0.5f, 0.01f), 0.15f,
        percentageAttributes));

    // Mix parameters
    layout.add(std::make_unique<juce::AudioParameterFloat>("DryWet", "Dry/Wet Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f,
        percentageAttributes));

    layout.add(std::make_unique<juce::AudioParameterFloat>("ReverbLevel", "Reverb Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.3f,
        percentageAttributes));

    // Tone shaping
    layout.add(std::make_unique<juce::AudioParameterFloat>("FilterFreq", "Filter Freq",
        juce::NormalisableRange<float>(100.0f, 16000.0f, 1.0f, 0.25f), 4000.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float value, int) { return juce::String((int)value) + " Hz"; })));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Saturation", "Tape Warmth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.4f,
        percentageAttributes));

    // Toggles
    layout.add(std::make_unique<juce::AudioParameterBool>("TapeDelayOnOff", "Tape Delay", true));
    layout.add(std::make_unique<juce::AudioParameterBool>("ReverbOnOff", "Reverb", false));
    layout.add(std::make_unique<juce::AudioParameterBool>("PsychedelicMode", "Psychedelic Mode", false));

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
        prepareToPlay(currentSampleRate, currentSamplesPerBlock);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WalrusDelay1AudioProcessor();
}