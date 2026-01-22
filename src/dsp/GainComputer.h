#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>

/**
 * @brief Soft-Knee Gain Computer for Compression
 *
 * Computes gain reduction based on:
 * - Threshold (dB)
 * - Ratio (1:1 to infinity)
 * - Soft knee width (6dB default for smooth transition)
 *
 * The soft knee provides a gradual onset of compression, which
 * is characteristic of optical and tube compressors.
 *
 * Transfer function:
 * - Below knee: output = input (no compression)
 * - In knee: gradual transition (quadratic interpolation)
 * - Above knee: output = threshold + (input - threshold) / ratio
 */
class GainComputer
{
public:
    GainComputer() = default;

    void prepare(double /*sampleRate*/, int /*samplesPerBlock*/)
    {
        // No sample-rate dependent state
    }

    void reset()
    {
        // No state to reset
    }

    /**
     * @brief Set compression threshold in dB
     */
    void setThreshold(float thresholdDb)
    {
        threshold = thresholdDb;
        updateKneeBounds();
    }

    /**
     * @brief Set compression ratio (e.g., 4.0 for 4:1)
     */
    void setRatio(float newRatio)
    {
        ratio = juce::jmax(1.0f, newRatio);
    }

    /**
     * @brief Set soft knee width in dB (0 = hard knee)
     */
    void setKneeWidth(float kneeDb)
    {
        kneeWidth = juce::jmax(0.0f, kneeDb);
        updateKneeBounds();
    }

    /**
     * @brief Compute gain reduction for a given input level
     * @param inputDb Input level in dB (from envelope follower)
     * @return Gain reduction in dB (negative value)
     */
    float computeGainReduction(float inputDb) const
    {
        float outputDb;

        if (inputDb <= kneeStart)
        {
            // Below knee - no compression
            outputDb = inputDb;
        }
        else if (inputDb >= kneeEnd)
        {
            // Above knee - full compression
            outputDb = threshold + (inputDb - threshold) / ratio;
        }
        else
        {
            // In the knee - smooth quadratic transition
            // This creates a gradual onset of compression
            float kneePosition = (inputDb - kneeStart) / kneeWidth;
            float compressionAmount = kneePosition * kneePosition * 0.5f;
            float slope = 1.0f - (1.0f / ratio);
            outputDb = inputDb - slope * compressionAmount * kneeWidth;
        }

        // Gain reduction = output - input (will be negative when compressing)
        return outputDb - inputDb;
    }

    /**
     * @brief Compute output level for a given input (for visualization)
     * @param inputDb Input level in dB
     * @return Output level in dB
     */
    float computeOutput(float inputDb) const
    {
        return inputDb + computeGainReduction(inputDb);
    }

    /**
     * @brief Process a buffer of envelope levels
     */
    void processBlock(const float* envelopeIn, float* gainReductionOut, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            gainReductionOut[i] = computeGainReduction(envelopeIn[i]);
        }
    }

    // Getters for visualization
    float getThreshold() const { return threshold; }
    float getRatio() const { return ratio; }
    float getKneeWidth() const { return kneeWidth; }
    float getKneeStart() const { return kneeStart; }
    float getKneeEnd() const { return kneeEnd; }

private:
    void updateKneeBounds()
    {
        kneeStart = threshold - kneeWidth * 0.5f;
        kneeEnd = threshold + kneeWidth * 0.5f;
    }

    float threshold = -20.0f;    // dB
    float ratio = 4.0f;          // :1
    float kneeWidth = 6.0f;      // dB (soft knee)

    // Precomputed knee bounds
    float kneeStart = -23.0f;    // threshold - knee/2
    float kneeEnd = -17.0f;      // threshold + knee/2
};
