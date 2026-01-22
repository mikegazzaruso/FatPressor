#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/SidechainDetector.h"
#include "dsp/EnvelopeFollower.h"
#include "dsp/GainComputer.h"
#include "dsp/TubeSaturation.h"
#include "dsp/TransformerColoration.h"
#include "PresetManager.h"

/**
 * @brief FatPressor - Tube-Driven Optical Compressor
 *
 * A warm, character compressor with:
 * - Tube saturation (pre-compression)
 * - Soft-knee optical compression
 * - Transformer coloration (post-compression)
 * - Signature FAT control for instant warmth
 *
 * Signal Flow:
 * Input → TubeSaturation → Compression → TransformerColor → Output → Mix
 */
class FatPressorAudioProcessor : public juce::AudioProcessor
{
public:
    FatPressorAudioProcessor();
    ~FatPressorAudioProcessor() override;

    // AudioProcessor interface
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // Editor
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // Plugin info
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Programs (presets) - delegated to PresetManager
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    // State
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Parameters
    juce::AudioProcessorValueTreeState parameters;

    // Preset Manager
    PresetManager presetManager;

    // Metering (atomic for thread-safe UI access)
    std::atomic<float> inputLevelL { -60.0f };
    std::atomic<float> inputLevelR { -60.0f };
    std::atomic<float> outputLevelL { -60.0f };
    std::atomic<float> outputLevelR { -60.0f };
    std::atomic<float> gainReduction { 0.0f };

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // DSP components
    SidechainDetector sidechainDetector;
    EnvelopeFollower envelopeFollower;
    GainComputer gainComputer;
    TubeSaturation tubeSaturation;
    TransformerColoration transformerColoration;
    // CompressorCore compressor;        // Task 8

    // Parameter pointers for real-time access
    std::atomic<float>* thresholdParam = nullptr;
    std::atomic<float>* ratioParam = nullptr;
    std::atomic<float>* attackParam = nullptr;
    std::atomic<float>* releaseParam = nullptr;
    std::atomic<float>* fatParam = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* mixParam = nullptr;

    // Smoothed parameters for zipper-free automation
    juce::SmoothedValue<float> thresholdSmoothed;
    juce::SmoothedValue<float> ratioSmoothed;
    juce::SmoothedValue<float> attackSmoothed;
    juce::SmoothedValue<float> releaseSmoothed;
    juce::SmoothedValue<float> fatSmoothed;
    juce::SmoothedValue<float> outputSmoothed;
    juce::SmoothedValue<float> mixSmoothed;

    // Sample rate for DSP
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FatPressorAudioProcessor)
};
