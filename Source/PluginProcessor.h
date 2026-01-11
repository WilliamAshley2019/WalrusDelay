/*
  ==============================================================================

    PluginProcessor.h
    Created: 11 Jan 2026
    Updated for JUCE 8.0.12

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>


class WalrusDelay1AudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    WalrusDelay1AudioProcessor();
    ~WalrusDelay1AudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;

private:
    //==============================================================================
    class TapeDelayLine
    {
    public:
        TapeDelayLine() = default;
        ~TapeDelayLine() = default;

        void prepare(double sampleRate, int maximumDelaySamples)
        {
            delayLine.prepare({ sampleRate, static_cast<juce::uint32>(512), 2 });
            delayLine.setMaximumDelayInSamples(maximumDelaySamples);
            delayLine.reset();
        }

        void setDelay(float delayInSamples)
        {
            currentDelay = juce::jlimit(1.0f, static_cast<float>(delayLine.getMaximumDelayInSamples()), delayInSamples);
        }

        float process(float input, float feedback, const std::function<float(float)>& saturationFunc)
        {
            float delayed = delayLine.popSample(0, currentDelay, true);
            delayed = saturationFunc(delayed);
            float feedbackSample = delayed * feedback;
            delayLine.pushSample(0, input + feedbackSample);
            return delayed;
        }

        void reset()
        {
            delayLine.reset();
        }

    private:
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine;
        float currentDelay = 1000.0f;
    };

    // Tape saturation
    struct TapeSaturation
    {
        static float softClip(float x)
        {
            if (x > 0.0f)
                return 1.0f - std::exp(-x);
            else
                return -1.0f + std::exp(x);
        }

        static float tubeWarmth(float x, float drive = 0.7f)
        {
            float sign = x < 0.0f ? -1.0f : 1.0f;
            return sign * (1.0f - std::exp(-std::abs(x) * (1.0f + drive)));
        }
    };

    // Simple 1-pole low-pass filter for feedback path
    class SimpleLowPassFilter
    {
    public:
        void prepare(double sampleRate)
        {
            sr = static_cast<float>(sampleRate);
            reset();
        }

        void reset()
        {
            z1 = 0.0f;
        }

        void setCutoff(float freq)
        {
            freq = juce::jlimit(20.0f, 20000.0f, freq);
            float omega = 2.0f * juce::MathConstants<float>::pi * freq / sr;
            // Simple 1-pole filter coefficient
            a = std::exp(-omega);
            b = 1.0f - a;
        }

        float process(float input)
        {
            float output = b * input + a * z1;
            z1 = output;
            return output;
        }

    private:
        float sr = 44100.0f;
        float a = 0.0f; // Feedback coefficient
        float b = 1.0f; // Feedforward coefficient
        float z1 = 0.0f; // Delay element
    };

    // Parameter Layout
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // DSP Members
    std::array<TapeDelayLine, 2> tapeDelays;
    std::array<SimpleLowPassFilter, 2> feedbackFilters;

    // LFOs for modulation
    juce::dsp::Oscillator<float> wowLFO{ [](float x) { return std::sin(x); } };
    juce::dsp::Oscillator<float> flutterLFO{ [](float x) { return std::sin(x * 2.0f); } };

    // Smoothing for parameters
    juce::LinearSmoothedValue<float> smoothedDelayTime{ 100.0f };
    juce::LinearSmoothedValue<float> smoothedFeedback{ 0.5f };
    juce::LinearSmoothedValue<float> smoothedDryWet{ 0.5f };
    juce::LinearSmoothedValue<float> smoothedReverbLevel{ 0.0f };
    juce::LinearSmoothedValue<float> smoothedFilterFreq{ 4000.0f };

    // Parameter pointers
    juce::AudioParameterFloat* delayTimeParam;
    juce::AudioParameterFloat* feedbackParam;
    juce::AudioParameterFloat* wowRateParam;
    juce::AudioParameterFloat* wowDepthParam;
    juce::AudioParameterFloat* flutterRateParam;
    juce::AudioParameterFloat* flutterDepthParam;
    juce::AudioParameterFloat* dryWetParam;
    juce::AudioParameterFloat* reverbLevelParam;
    juce::AudioParameterFloat* filterFreqParam;
    juce::AudioParameterFloat* saturationParam;
    juce::AudioParameterBool* tapeDelayOnOffParam;
    juce::AudioParameterBool* reverbOnOffParam;
    juce::AudioParameterBool* psychedelicModeParam;

    // Output buffers
    juce::AudioBuffer<float> delayBuffer;
    juce::AudioBuffer<float> wetBuffer;

    // Sample rate
    double currentSampleRate = 44100.0;
    int currentSamplesPerBlock = 512;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WalrusDelay1AudioProcessor)
};