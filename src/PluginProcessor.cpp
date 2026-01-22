#include "PluginProcessor.h"
#include "PluginEditor.h"

FatPressorAudioProcessor::FatPressorAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
    , presetManager(parameters)
{
    // Get raw parameter pointers for real-time access
    thresholdParam = parameters.getRawParameterValue("threshold");
    ratioParam = parameters.getRawParameterValue("ratio");
    attackParam = parameters.getRawParameterValue("attack");
    releaseParam = parameters.getRawParameterValue("release");
    fatParam = parameters.getRawParameterValue("fat");
    outputParam = parameters.getRawParameterValue("output");
    mixParam = parameters.getRawParameterValue("mix");

    // Initialize preset manager
    presetManager.initialize();
}

FatPressorAudioProcessor::~FatPressorAudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout
FatPressorAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Threshold: -60 to 0 dB, default -20
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "threshold", 1 },
        "Threshold",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f),
        -20.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    // Ratio: 1:1 to 20:1, default 4:1, skewed for finer control at low ratios
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "ratio", 1 },
        "Ratio",
        juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f, 0.5f),
        4.0f,
        juce::AudioParameterFloatAttributes().withLabel(":1")));

    // Attack: 0.1 to 100 ms, default 10, logarithmic skew
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "attack", 1 },
        "Attack",
        juce::NormalisableRange<float>(0.1f, 100.0f, 0.1f, 0.3f),
        10.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    // Release: 10 to 1000 ms, default 100, logarithmic skew
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "release", 1 },
        "Release",
        juce::NormalisableRange<float>(10.0f, 1000.0f, 1.0f, 0.3f),
        100.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    // FAT: 0 to 100%, default 50 - the signature warmth control
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "fat", 1 },
        "FAT",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        50.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // Output: -12 to +12 dB, default 0
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "output", 1 },
        "Output",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    // Mix: 0 to 100%, default 100
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "mix", 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    return { params.begin(), params.end() };
}

void FatPressorAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    // Initialize smoothed parameters - fast response for real-time feel
    // Compression params: very fast (2ms) - user expects instant response
    thresholdSmoothed.reset(sampleRate, 0.002);
    ratioSmoothed.reset(sampleRate, 0.002);
    // Timing params: no smoothing needed (envelope follower handles timing)
    attackSmoothed.reset(sampleRate, 0.001);
    releaseSmoothed.reset(sampleRate, 0.001);
    // FAT: moderate smoothing (5ms) to avoid clicks in saturation
    fatSmoothed.reset(sampleRate, 0.005);
    // Output/Mix: short smoothing (3ms) for click-free but responsive
    outputSmoothed.reset(sampleRate, 0.003);
    mixSmoothed.reset(sampleRate, 0.003);

    // Set initial values
    thresholdSmoothed.setCurrentAndTargetValue(thresholdParam->load());
    ratioSmoothed.setCurrentAndTargetValue(ratioParam->load());
    attackSmoothed.setCurrentAndTargetValue(attackParam->load());
    releaseSmoothed.setCurrentAndTargetValue(releaseParam->load());
    fatSmoothed.setCurrentAndTargetValue(fatParam->load());
    outputSmoothed.setCurrentAndTargetValue(outputParam->load());
    mixSmoothed.setCurrentAndTargetValue(mixParam->load());

    // Prepare DSP components
    sidechainDetector.prepare(sampleRate, samplesPerBlock);
    envelopeFollower.prepare(sampleRate, samplesPerBlock);
    envelopeFollower.setAttackMs(attackParam->load());
    envelopeFollower.setReleaseMs(releaseParam->load());
    gainComputer.prepare(sampleRate, samplesPerBlock);
    gainComputer.setThreshold(thresholdParam->load());
    gainComputer.setRatio(ratioParam->load());
    gainComputer.setKneeWidth(6.0f);  // 6dB soft knee per spec
    tubeSaturation.prepare(sampleRate, samplesPerBlock);
    tubeSaturation.setDrive(fatParam->load() / 100.0f);  // FAT controls tube drive
    transformerColoration.prepare(sampleRate, samplesPerBlock);
    transformerColoration.setAmount(fatParam->load() / 100.0f);  // FAT controls transformer color
}

void FatPressorAudioProcessor::releaseResources()
{
    // Release DSP resources
}

bool FatPressorAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void FatPressorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    // Clear unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    // Update smoothed parameter targets
    thresholdSmoothed.setTargetValue(thresholdParam->load());
    ratioSmoothed.setTargetValue(ratioParam->load());
    attackSmoothed.setTargetValue(attackParam->load());
    releaseSmoothed.setTargetValue(releaseParam->load());
    fatSmoothed.setTargetValue(fatParam->load());
    outputSmoothed.setTargetValue(outputParam->load());
    mixSmoothed.setTargetValue(mixParam->load());

    // Measure input level
    float inL = buffer.getMagnitude(0, 0, numSamples);
    float inR = buffer.getNumChannels() > 1 ? buffer.getMagnitude(1, 0, numSamples) : inL;
    inputLevelL.store(juce::Decibels::gainToDecibels(inL, -60.0f));
    inputLevelR.store(juce::Decibels::gainToDecibels(inR, -60.0f));

    // Store dry signal for mix
    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer);

    // ============================================
    // FULL DSP CHAIN: FatPressor Compression
    // ============================================
    // Signal flow: Input → TubeSat → Compression → Transformer → Output → Mix

    // Skip smoothing to target for fast response - these need instant feel
    thresholdSmoothed.skip(numSamples);
    ratioSmoothed.skip(numSamples);
    attackSmoothed.skip(numSamples);
    releaseSmoothed.skip(numSamples);
    fatSmoothed.skip(numSamples);

    // Get current parameter values (now at target after skip)
    const float currentThreshold = thresholdSmoothed.getCurrentValue();
    const float currentRatio = ratioSmoothed.getCurrentValue();
    const float currentAttack = attackSmoothed.getCurrentValue();
    const float currentRelease = releaseSmoothed.getCurrentValue();
    const float currentFat = fatSmoothed.getCurrentValue() / 100.0f;  // 0-1

    // Update envelope follower timing
    envelopeFollower.setAttackMs(currentAttack);
    envelopeFollower.setReleaseMs(currentRelease);

    // Update gain computer settings
    gainComputer.setThreshold(currentThreshold);
    gainComputer.setRatio(currentRatio);

    // Update saturation amounts
    tubeSaturation.setDrive(currentFat);
    transformerColoration.setAmount(currentFat);

    // 1. PRE-COMPRESSION: Tube Saturation (adds warmth before compression)
    tubeSaturation.processBlock(buffer);

    // 2. COMPRESSION: Per-sample processing for smooth gain reduction
    float peakGainReduction = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Get stereo samples
        float leftSample = buffer.getSample(0, sample);
        float rightSample = buffer.getNumChannels() > 1 ? buffer.getSample(1, sample) : leftSample;

        // Sidechain detection (hybrid RMS-Peak)
        float detectionDb = sidechainDetector.processSample(leftSample, rightSample);

        // Envelope following (attack/release with two-stage optical release)
        float envelopeDb = envelopeFollower.processSample(detectionDb);

        // Gain computation (soft-knee compression)
        float grDb = gainComputer.computeGainReduction(envelopeDb);

        // Track peak gain reduction for metering
        if (grDb < peakGainReduction)
            peakGainReduction = grDb;

        // Convert gain reduction to linear
        float grLinear = juce::Decibels::decibelsToGain(grDb);

        // Apply gain reduction to all channels
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            float* channelData = buffer.getWritePointer(channel);
            channelData[sample] *= grLinear;
        }
    }

    // Store gain reduction for metering (positive value for display)
    gainReduction.store(-peakGainReduction);

    // 3. POST-COMPRESSION: Transformer Coloration (adds "iron" character)
    transformerColoration.processBlock(buffer);

    // 4. OUTPUT GAIN AND MIX
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* wetData = buffer.getWritePointer(channel);
        const auto* dryData = dryBuffer.getReadPointer(channel);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float outputGainDb = outputSmoothed.getNextValue();
            const float mixPercent = mixSmoothed.getNextValue();
            const float outputGain = juce::Decibels::decibelsToGain(outputGainDb);
            const float mixAmount = mixPercent / 100.0f;

            // Apply output gain to wet signal
            float wetSample = wetData[sample] * outputGain;

            // Blend wet/dry
            wetData[sample] = wetSample * mixAmount + dryData[sample] * (1.0f - mixAmount);
        }

        // Reset smoothed values for next channel
        outputSmoothed.setCurrentAndTargetValue(outputParam->load());
        mixSmoothed.setCurrentAndTargetValue(mixParam->load());
    }

    // Measure output level
    float outL = buffer.getMagnitude(0, 0, numSamples);
    float outR = buffer.getNumChannels() > 1 ? buffer.getMagnitude(1, 0, numSamples) : outL;
    outputLevelL.store(juce::Decibels::gainToDecibels(outL, -60.0f));
    outputLevelR.store(juce::Decibels::gainToDecibels(outR, -60.0f));
}

juce::AudioProcessorEditor* FatPressorAudioProcessor::createEditor()
{
    return new FatPressorAudioProcessorEditor(*this);
}

void FatPressorAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void FatPressorAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

// Program (Preset) methods - delegated to PresetManager
int FatPressorAudioProcessor::getNumPrograms()
{
    int count = presetManager.getTotalPresetCount();
    return count > 0 ? count : 1;  // Always report at least 1 program
}

int FatPressorAudioProcessor::getCurrentProgram()
{
    return presetManager.getCurrentPresetIndex();
}

void FatPressorAudioProcessor::setCurrentProgram(int index)
{
    presetManager.loadPresetByIndex(index);
}

const juce::String FatPressorAudioProcessor::getProgramName(int index)
{
    auto presets = presetManager.getAllPresets();
    if (index >= 0 && index < presets.size())
        return presets[index].name;
    return {};
}

void FatPressorAudioProcessor::changeProgramName(int /*index*/, const juce::String& /*newName*/)
{
    // Factory presets can't be renamed
    // User presets could be renamed here if needed
}

// Plugin instantiation
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FatPressorAudioProcessor();
}
