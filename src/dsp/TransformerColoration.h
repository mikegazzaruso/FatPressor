#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>

/**
 * @brief Transformer Coloration - Output Stage Character
 *
 * Post-compression saturation that emulates output transformer characteristics:
 * - Low-frequency "thump" (subtle bass saturation)
 * - High-frequency "silk" (gentle HF rolloff with harmonics)
 * - Subtle odd-harmonic content (transformer core saturation)
 *
 * Unlike the tube stage (even harmonics), transformers add subtle odd
 * harmonics which give the "iron" character.
 *
 * Controlled by the FAT parameter (0-100%).
 */
class TransformerColoration
{
public:
    TransformerColoration() = default;

    void prepare(double newSampleRate, int samplesPerBlock)
    {
        sampleRate = newSampleRate;
        juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32>(samplesPerBlock), 2 };

        // Low shelf for bass enhancement (~100Hz)
        lowShelf.prepare(spec);
        *lowShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(
            sampleRate, 100.0f, 0.7f, 1.0f);

        // High shelf for subtle HF rolloff (~8kHz)
        highShelf.prepare(spec);
        *highShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(
            sampleRate, 8000.0f, 0.7f, 1.0f);
    }

    void reset()
    {
        lowShelf.reset();
        highShelf.reset();
    }

    /**
     * @brief Set coloration amount (0.0 to 1.0)
     * Maps from FAT parameter percentage
     */
    void setAmount(float amount)
    {
        colorAmount = juce::jlimit(0.0f, 1.0f, amount);

        // === TRANSFORMER MAGIC ===
        // Low shelf: moderate bass boost (up to +3dB) - subtle weight
        float lowGainDb = colorAmount * 3.0f;
        float lowGain = juce::Decibels::decibelsToGain(lowGainDb);
        *lowShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(
            sampleRate, 120.0f, 0.5f, lowGain);  // Wide Q for smooth bass

        // High shelf: very subtle rolloff (up to -1.5dB) - just a touch of silk
        float highGainDb = -colorAmount * 1.5f;
        float highGain = juce::Decibels::decibelsToGain(highGainDb);
        *highShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(
            sampleRate, 8000.0f, 0.5f, highGain);  // Higher freq, less darkening
    }

    /**
     * @brief Soft saturation - continuous curve without discontinuities
     */
    inline float softSaturate(float x, float knee) const
    {
        // Soft curve: x / (1 + |x| * knee)
        return x / (1.0f + std::abs(x) * knee);
    }

    /**
     * @brief Process a single sample with transformer coloration
     */
    float processSample(int /*channel*/, float input)
    {
        if (colorAmount < 0.001f)
            return input;

        // === IRON CORE SATURATION ===
        // Very gentle magnetic saturation - subtle compression
        float x = input;

        // Soft saturation instead of tanh to avoid harsh transitions
        float satAmount = 0.2f + colorAmount * 0.4f;  // 0.2 to 0.6 knee
        float saturation = softSaturate(x * (1.0f + colorAmount * 0.5f), satAmount);

        // Add very subtle odd harmonics - transformer "iron" character
        // Use soft saturation on the harmonic too
        float thirdHarmonic = softSaturate(x * x * x * 0.1f * colorAmount, 0.5f);
        saturation += thirdHarmonic;

        // Gentle gain compensation
        saturation *= 0.95f / (1.0f + colorAmount * 0.2f);

        // Smooth blend - mostly dry with subtle wet
        float wetAmount = colorAmount * 0.5f;  // Max 50% wet
        return input * (1.0f - wetAmount) + saturation * wetAmount;
    }

    /**
     * @brief Process a stereo buffer with transformer coloration
     */
    void processBlock(juce::AudioBuffer<float>& buffer)
    {
        if (colorAmount < 0.001f)
            return;  // Bypass

        // Apply subtle saturation
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto* data = buffer.getWritePointer(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                data[sample] = processSample(channel, data[sample]);
            }
        }

        // Apply EQ shaping (low thump, high silk)
        auto block = juce::dsp::AudioBlock<float>(buffer);
        auto context = juce::dsp::ProcessContextReplacing<float>(block);
        lowShelf.process(context);
        highShelf.process(context);
    }

private:
    double sampleRate = 44100.0;
    float colorAmount = 0.0f;

    // EQ for transformer character
    juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> lowShelf;

    juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> highShelf;
};
