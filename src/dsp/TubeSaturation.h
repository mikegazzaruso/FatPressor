#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>

/**
 * @brief Tube Saturation with Even-Order Harmonics
 *
 * Pre-compression saturation stage that adds warm tube character:
 * - Asymmetric waveshaping for even-order harmonics (2nd, 4th, 6th)
 * - Soft clipping with musical overtones
 * - DC offset compensation
 *
 * The asymmetric transfer function creates even harmonics which
 * are perceived as "warm" and "full" compared to odd harmonics
 * (which sound more "edgy" or "harsh").
 *
 * Controlled by the FAT parameter (0-100%).
 */
class TubeSaturation
{
public:
    TubeSaturation() = default;

    void prepare(double newSampleRate, int samplesPerBlock)
    {
        sampleRate = newSampleRate;

        juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32>(samplesPerBlock), 2 };

        // DC blocker to remove offset from asymmetric waveshaping
        dcBlocker.prepare(spec);
        *dcBlocker.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 5.0f);

        // Low-pass filter for saturation sidechain - focus warmth on lows
        lowpassForSat.prepare(spec);
        *lowpassForSat.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 800.0f);

        // Highpass to extract highs that bypass saturation
        highpassForClean.prepare(spec);
        *highpassForClean.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 600.0f);
    }

    void reset()
    {
        dcBlocker.reset();
        lowpassForSat.reset();
        highpassForClean.reset();
    }

    /**
     * @brief Set saturation amount (0.0 to 1.0)
     * Maps from FAT parameter percentage
     */
    void setDrive(float driveAmount)
    {
        drive = juce::jlimit(0.0f, 1.0f, driveAmount);
        // Gentle drive - 100% FAT = subtle warmth, not aggressive
        driveScaled = 1.0f + drive * 3.0f;  // Up to 4x max
    }

    /**
     * @brief Soft saturation curve - continuous and smooth
     */
    inline float softSaturate(float x, float amount) const
    {
        // Soft polynomial saturation - no discontinuities
        // At low levels: nearly linear. At high levels: gentle compression
        // Soft knee curve: x / (1 + |x|*amount) - always smooth
        float sat = x / (1.0f + std::abs(x) * amount);

        return sat;
    }

    /**
     * @brief Process a single sample with tube saturation
     * @param input Input sample
     * @return Saturated output sample
     */
    float processSample(float input)
    {
        if (drive < 0.001f)
            return input;

        // === WARM TUBE SATURATION ===
        // Gentle drive - avoid harsh overdriving
        float x = input * (1.0f + drive * 1.5f);  // Max 2.5x gain

        // Asymmetric waveshaping - key to even harmonics (warm, not harsh)
        // Use smooth tanh with controlled input levels
        float tanhInput = std::tanh(x * 0.8f);

        // Slight asymmetry for even harmonics - very subtle
        float asymmetry = 0.0f;
        if (x < 0.0f)
        {
            asymmetry = x * std::abs(x) * 0.08f * drive;  // Subtle 2nd harmonic
        }

        float output = tanhInput + asymmetry;

        // Add gentle even harmonics (2nd) - the "butter"
        // Apply BEFORE saturation, then saturate the result
        float harmonic = input * std::abs(input) * 0.15f * drive;
        output += harmonic;

        // Final soft saturation to keep everything smooth
        output = softSaturate(output, 0.3f + drive * 0.5f);

        // Gain compensation - keep levels consistent
        output *= 0.85f / (1.0f + drive * 0.3f);

        // Wet/dry blend
        return input * (1.0f - drive) + output * drive;
    }

    /**
     * @brief Process a stereo buffer with tube saturation
     * Uses multiband approach: saturate lows, keep highs clean
     */
    void processBlock(juce::AudioBuffer<float>& buffer)
    {
        if (drive < 0.001f)
            return;  // Bypass

        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();

        // Create temp buffers for multiband processing
        juce::AudioBuffer<float> lowBand(numChannels, numSamples);
        juce::AudioBuffer<float> highBand(numChannels, numSamples);

        // Copy to both bands
        for (int ch = 0; ch < numChannels; ++ch)
        {
            lowBand.copyFrom(ch, 0, buffer, ch, 0, numSamples);
            highBand.copyFrom(ch, 0, buffer, ch, 0, numSamples);
        }

        // Filter: extract lows (<800Hz) and highs (>600Hz)
        auto lowBlock = juce::dsp::AudioBlock<float>(lowBand);
        auto highBlock = juce::dsp::AudioBlock<float>(highBand);
        lowpassForSat.process(juce::dsp::ProcessContextReplacing<float>(lowBlock));
        highpassForClean.process(juce::dsp::ProcessContextReplacing<float>(highBlock));

        // Saturate ONLY the low band - this is where the "belly" lives
        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* lowData = lowBand.getWritePointer(channel);
            for (int sample = 0; sample < numSamples; ++sample)
            {
                lowData[sample] = processSample(lowData[sample]);
            }
        }

        // Recombine: saturated lows + clean highs (with crossfade zone)
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* outData = buffer.getWritePointer(ch);
            const auto* lowData = lowBand.getReadPointer(ch);
            const auto* highData = highBand.getReadPointer(ch);

            for (int sample = 0; sample < numSamples; ++sample)
            {
                // Blend: full saturated lows + reduced highs (to avoid phasing)
                outData[sample] = lowData[sample] + highData[sample] * (1.0f - drive * 0.3f);
            }
        }

        // Remove DC offset
        auto block = juce::dsp::AudioBlock<float>(buffer);
        dcBlocker.process(juce::dsp::ProcessContextReplacing<float>(block));
    }

private:
    double sampleRate = 44100.0;

    float drive = 0.0f;         // 0.0 to 1.0
    float driveScaled = 1.0f;   // 1.0 to 16.0

    // DC blocker (highpass at 5Hz)
    juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> dcBlocker;

    // Multiband filters for "belly" focused saturation
    juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> lowpassForSat;   // Extract lows for saturation

    juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> highpassForClean; // Extract highs to keep clean
};
