#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>

/**
 * @brief Hybrid RMS-Peak Sidechain Detector
 *
 * Blends RMS and Peak detection for optical compressor character:
 * - 70% RMS: Smooth, musical response (optical character)
 * - 30% Peak: Transient awareness (prevents pumping)
 *
 * The RMS window is ~10ms for good transient response while
 * maintaining the smooth optical feel.
 */
class SidechainDetector
{
public:
    SidechainDetector() = default;

    void prepare(double newSampleRate, int samplesPerBlock)
    {
        sampleRate = newSampleRate;

        // RMS window of ~10ms for good balance
        rmsWindowSize = static_cast<int>(sampleRate * 0.01);
        rmsWindowSize = std::max(1, rmsWindowSize);

        // Circular buffer for RMS calculation
        rmsBuffer.resize(static_cast<size_t>(rmsWindowSize), 0.0f);
        rmsWriteIndex = 0;
        rmsSum = 0.0f;

        // Peak detector coefficients (fast attack, medium release)
        // Attack: ~0.1ms, Release: ~50ms
        peakAttackCoeff = static_cast<float>(std::exp(-1.0 / (sampleRate * 0.0001)));
        peakReleaseCoeff = static_cast<float>(std::exp(-1.0 / (sampleRate * 0.05)));
        peakEnvelope = 0.0f;

        juce::ignoreUnused(samplesPerBlock);
    }

    void reset()
    {
        std::fill(rmsBuffer.begin(), rmsBuffer.end(), 0.0f);
        rmsWriteIndex = 0;
        rmsSum = 0.0f;
        peakEnvelope = 0.0f;
    }

    /**
     * @brief Process a single sample and return hybrid detection level in dB
     * @param inputL Left channel sample
     * @param inputR Right channel sample (ignored for mono, averaged for stereo)
     * @return Detection level in dB
     */
    float processSample(float inputL, float inputR)
    {
        // Sum to mono for detection
        float monoInput = (inputL + inputR) * 0.5f;
        float inputSquared = monoInput * monoInput;
        float inputAbs = std::abs(monoInput);

        // === RMS Detection (sliding window) ===
        // Remove oldest sample from sum
        rmsSum -= rmsBuffer[static_cast<size_t>(rmsWriteIndex)];
        // Add new squared sample
        rmsBuffer[static_cast<size_t>(rmsWriteIndex)] = inputSquared;
        rmsSum += inputSquared;
        // Advance write index
        rmsWriteIndex = (rmsWriteIndex + 1) % rmsWindowSize;

        // Calculate RMS
        float rmsLevel = std::sqrt(rmsSum / static_cast<float>(rmsWindowSize));

        // === Peak Detection (envelope follower) ===
        if (inputAbs > peakEnvelope)
            peakEnvelope = peakAttackCoeff * peakEnvelope + (1.0f - peakAttackCoeff) * inputAbs;
        else
            peakEnvelope = peakReleaseCoeff * peakEnvelope;

        // === Hybrid Blend (70% RMS, 30% Peak) ===
        float hybridLevel = rmsLevel * rmsWeight + peakEnvelope * peakWeight;

        // Convert to dB (with floor at -60dB)
        return juce::Decibels::gainToDecibels(hybridLevel, -60.0f);
    }

    /**
     * @brief Process a buffer and return per-sample detection levels in dB
     */
    void processBlock(const float* inputL, const float* inputR, float* detectionOut, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            detectionOut[i] = processSample(inputL[i], inputR[i]);
        }
    }

    // Allow tuning the RMS/Peak blend if needed
    void setBlend(float rmsRatio)
    {
        rmsWeight = juce::jlimit(0.0f, 1.0f, rmsRatio);
        peakWeight = 1.0f - rmsWeight;
    }

private:
    double sampleRate = 44100.0;

    // RMS detection
    std::vector<float> rmsBuffer;
    int rmsWindowSize = 441; // ~10ms at 44.1kHz
    int rmsWriteIndex = 0;
    float rmsSum = 0.0f;

    // Peak detection
    float peakAttackCoeff = 0.0f;
    float peakReleaseCoeff = 0.0f;
    float peakEnvelope = 0.0f;

    // Hybrid blend weights (70% RMS, 30% Peak)
    float rmsWeight = 0.7f;
    float peakWeight = 0.3f;
};
