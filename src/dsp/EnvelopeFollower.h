#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>

/**
 * @brief Optical-Style Envelope Follower with Two-Stage Release
 *
 * Implements the characteristic "program-dependent" behavior of optical compressors:
 * - Standard attack for compression onset
 * - Two-stage release: fast initial release + slow tail (optical character)
 *
 * The two-stage release creates the smooth, musical pumping associated with
 * classic optical compressors like the LA-2A and Tube-Tech CL 1B.
 *
 * Stage 1: Fast release (30% of set time) - catches transient recovery
 * Stage 2: Slow release (150% of set time) - smooth tail, prevents pumping
 * Crossover: When gain reduction drops below 50% of peak
 */
class EnvelopeFollower
{
public:
    EnvelopeFollower() = default;

    void prepare(double newSampleRate, int /*samplesPerBlock*/)
    {
        sampleRate = newSampleRate;
        updateCoefficients();
        reset();
    }

    void reset()
    {
        envelope = 0.0f;
        peakGainReduction = 0.0f;
        inSlowRelease = false;
    }

    /**
     * @brief Set attack time in milliseconds
     */
    void setAttackMs(float attackMs)
    {
        attackTimeMs = juce::jlimit(0.1f, 100.0f, attackMs);
        updateCoefficients();
    }

    /**
     * @brief Set release time in milliseconds
     */
    void setReleaseMs(float releaseMs)
    {
        releaseTimeMs = juce::jlimit(10.0f, 1000.0f, releaseMs);
        updateCoefficients();
    }

    /**
     * @brief Process a detection level and return smoothed envelope
     * @param detectionDb Input detection level in dB (from SidechainDetector)
     * @return Smoothed envelope level in dB
     */
    float processSample(float detectionDb)
    {
        // Convert dB to linear for envelope following
        float detectionLinear = juce::Decibels::decibelsToGain(detectionDb, -60.0f);

        if (detectionLinear > envelope)
        {
            // Attack phase - signal rising
            envelope = attackCoeff * envelope + (1.0f - attackCoeff) * detectionLinear;
            peakGainReduction = envelope;  // Track peak for release stage decision
            inSlowRelease = false;
        }
        else
        {
            // Release phase - signal falling
            // Determine which release stage we're in
            float releaseCoeff;

            if (!inSlowRelease && envelope < peakGainReduction * releaseStageThreshold)
            {
                // Switch to slow release stage
                inSlowRelease = true;
            }

            releaseCoeff = inSlowRelease ? releaseCoeffSlow : releaseCoeffFast;
            envelope = releaseCoeff * envelope + (1.0f - releaseCoeff) * detectionLinear;
        }

        // Convert back to dB
        return juce::Decibels::gainToDecibels(envelope, -60.0f);
    }

    /**
     * @brief Process a buffer of detection levels
     */
    void processBlock(const float* detectionIn, float* envelopeOut, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            envelopeOut[i] = processSample(detectionIn[i]);
        }
    }

    /**
     * @brief Get current envelope value in dB
     */
    float getCurrentEnvelopeDb() const
    {
        return juce::Decibels::gainToDecibels(envelope, -60.0f);
    }

private:
    void updateCoefficients()
    {
        if (sampleRate <= 0.0) return;

        // Attack coefficient
        float attackTimeSec = attackTimeMs / 1000.0f;
        attackCoeff = static_cast<float>(std::exp(-1.0 / (sampleRate * attackTimeSec)));

        // Two-stage release coefficients
        float releaseTimeSec = releaseTimeMs / 1000.0f;

        // Stage 1: Fast release (30% of set time) - immediate transient recovery
        float fastReleaseTime = releaseTimeSec * fastReleaseRatio;
        releaseCoeffFast = static_cast<float>(std::exp(-1.0 / (sampleRate * fastReleaseTime)));

        // Stage 2: Slow release (150% of set time) - smooth optical tail
        float slowReleaseTime = releaseTimeSec * slowReleaseRatio;
        releaseCoeffSlow = static_cast<float>(std::exp(-1.0 / (sampleRate * slowReleaseTime)));
    }

    double sampleRate = 44100.0;

    // Timing parameters
    float attackTimeMs = 10.0f;
    float releaseTimeMs = 100.0f;

    // Envelope state
    float envelope = 0.0f;
    float peakGainReduction = 0.0f;
    bool inSlowRelease = false;

    // Coefficients
    float attackCoeff = 0.0f;
    float releaseCoeffFast = 0.0f;
    float releaseCoeffSlow = 0.0f;

    // Two-stage release configuration (optical character)
    static constexpr float fastReleaseRatio = 0.3f;      // Stage 1: 30% of release time
    static constexpr float slowReleaseRatio = 1.5f;      // Stage 2: 150% of release time
    static constexpr float releaseStageThreshold = 0.5f; // Switch at 50% of peak
};
