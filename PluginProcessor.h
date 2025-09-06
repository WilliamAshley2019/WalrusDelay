/*
  ==============================================================================

    PluginProcessor.h
    Created: 5 Sep 2025

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

/**
 * @class WalrusDelay1AudioProcessor
 * @brief Handles all audio processing and parameter management for the WalrusDelay1 plugin.
 *
 * This class implements a tape delay with wow and flutter modulation, a low-pass filter in the feedback loop,
 * and a simplified reverb effect, with on/off toggles for tape delay and reverb sections.
 */
class WalrusDelay1AudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    WalrusDelay1AudioProcessor();
    ~WalrusDelay1AudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

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
    // Circular Buffer for Delay Line
    template <typename T>
    class CircularBuffer
    {
    public:
        CircularBuffer() {}
        ~CircularBuffer() {}

        void flushBuffer() { if (buffer) memset(buffer.get(), 0, bufferLength * sizeof(T)); }

        void createCircularBuffer(unsigned int _bufferLength)
        {
            createCircularBufferPowerOfTwo((unsigned int)(pow(2, ceil(log(_bufferLength) / log(2)))));
        }

        void createCircularBufferPowerOfTwo(unsigned int _bufferLengthPowerOfTwo)
        {
            writeIndex = 0;
            bufferLength = _bufferLengthPowerOfTwo;
            wrapMask = bufferLength - 1;
            buffer = std::make_unique<T[]>(bufferLength);
            flushBuffer();
        }

        void writeBuffer(T input)
        {
            buffer[writeIndex++] = input;
            writeIndex &= wrapMask;
        }

        T readBuffer(float delayInSamples)
        {
            delayInSamples = juce::jlimit(0.0f, static_cast<float>(bufferLength - 1), delayInSamples);
            float readIndex = (float)writeIndex - delayInSamples;
            if (readIndex < 0) readIndex += bufferLength;
            int readIndexInt = (int)readIndex;
            float frac = readIndex - readIndexInt;
            int readIndexNext = (readIndexInt + 1) & wrapMask;
            return buffer[readIndexInt] * (1.0f - frac) + buffer[readIndexNext] * frac;
        }

        unsigned int getBufferLength() { return bufferLength; }

    private:
        std::unique_ptr<T[]> buffer = nullptr;
        unsigned int writeIndex = 0;
        unsigned int bufferLength = 1024;
        unsigned int wrapMask = bufferLength - 1;
    };

    // Low-Pass Filter
    struct LowPassFilter
    {
        void prepare(double newSampleRate)
        {
            sampleRate = static_cast<float>(newSampleRate);
            reset();
            smoothedFreq.reset(sampleRate, 0.05f);
        }

        void reset() { x1 = x2 = y1 = y2 = 0.0f; }

        void update(float frequency)
        {
            frequency = juce::jlimit(200.0f, 4000.0f, frequency);
            if (frequency != smoothedFreq.getTargetValue())
            {
                smoothedFreq.setTargetValue(frequency);
                float freq = smoothedFreq.getNextValue();
                float omega = 2.0f * juce::MathConstants<float>::pi * freq / sampleRate;
                alpha = sin(omega) / (2.0f * 0.5f);
                float cosOmega = cos(omega);
                b1 = -2.0f * cosOmega;
                b2 = 1.0f - alpha;
                a0 = (1.0f + cosOmega) * 0.5f;
                a1 = 1.0f - cosOmega;
                float gain = a0 + a1 + a0;
                a0 /= gain;
                a1 /= gain;
                b1 /= gain;
                b2 /= gain;
                // Debug coefficients
                if (!std::isfinite(a0) || !std::isfinite(a1) || !std::isfinite(b1) || !std::isfinite(b2))
                    juce::Logger::outputDebugString("Non-finite filter coefficients: a0=" + juce::String(a0) + ", a1=" + juce::String(a1) +
                        ", b1=" + juce::String(b1) + ", b2=" + juce::String(b2));
            }
        }

        float process(float input)
        {
            float output = a0 * input + a1 * x1 + a0 * x2 - b1 * y1 - b2 * y2;
            x2 = x1; x1 = input;
            y2 = y1; y1 = output;
            if (!std::isfinite(output))
            {
                juce::Logger::outputDebugString("Non-finite output in LowPassFilter");
                reset();
                output = 0.0f;
            }
            return juce::jlimit(-1.0f, 1.0f, output);
        }

        float sampleRate = 44100.0f;
        float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
        float a0 = 1.0f, a1 = 0.0f, b1 = 0.0f, b2 = 0.0f, alpha = 0.0f;
        juce::LinearSmoothedValue<float> smoothedFreq{ 2000.0f };
    };

    // Parameter Layout
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // DSP Members
    std::array<CircularBuffer<float>, 2> delayBuffers; // One per channel
    std::array<LowPassFilter, 2> feedbackFilters; // Low-pass filters for feedback
    int maxDelaySamples{ 0 };

    // LFOs for Wow & Flutter modulation
    juce::dsp::Oscillator<float> wowOscillator{ [](float x) { return std::sin(x); } };
    juce::dsp::Oscillator<float> flutterOscillator{ [](float x) { return std::sin(x); } };
    float wowPhaseLeft{ 0.0f }, wowPhaseRight{ 0.0f };
    float flutterPhaseLeft{ 0.0f }, flutterPhaseRight{ 0.0f };

    // Reverb (simplified single-tap reverb)
    std::array<CircularBuffer<float>, 2> reverbBuffers;
    std::array<LowPassFilter, 2> reverbFilters;
    float reverbDelayMs{ 50.0f }; // Fixed reverb delay time

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
    juce::AudioParameterBool* tapeDelayOnOffParam; // New toggle
    juce::AudioParameterBool* reverbOnOffParam; // New toggle

    // Smoothing
    std::array<juce::LinearSmoothedValue<float>, 2> smoothedDelayTime; // Per channel for stereo
    juce::LinearSmoothedValue<float> smoothedFeedback{ 0.5f };
    juce::LinearSmoothedValue<float> smoothedDryWet{ 0.5f };
    juce::LinearSmoothedValue<float> smoothedReverbLevel{ 0.0f };

    // Fade-out for feedback
    std::array<juce::LinearSmoothedValue<float>, 2> feedbackFade; // Per channel
    std::array<int, 2> silenceCounter; // Count silent samples to trigger fade

    // Temporary buffer for delay output
    juce::AudioBuffer<float> delayOutput;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WalrusDelay1AudioProcessor)
};