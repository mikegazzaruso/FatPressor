#pragma once

#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>

/**
 * @brief FatPressor WebView-based Editor
 *
 * Modern UI with:
 * - Interactive compression curve graph
 * - Draggable threshold/ratio
 * - FAT knob (hero control)
 * - Real-time metering
 * - Preset system
 *
 * IMPORTANT: Member declaration order matters!
 * 1. Relays FIRST
 * 2. WebView SECOND
 * 3. Attachments LAST
 */
class FatPressorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer,
                                        private PresetManager::Listener
{
public:
    explicit FatPressorAudioProcessorEditor(FatPressorAudioProcessor&);
    ~FatPressorAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String& url);

    // PresetManager::Listener
    void presetChanged(const PresetManager::PresetInfo& newPreset) override;
    void presetListChanged() override;

    // Send preset list to WebView
    void sendPresetListToWebView();

    // Force sync all parameter values to WebView
    void syncAllParametersToWebView();

    FatPressorAudioProcessor& processorRef;

    // ═══════════════════════════════════════════════════════════════
    // MEMBER ORDER IS CRITICAL - DO NOT REORDER
    // ═══════════════════════════════════════════════════════════════

    // 1. RELAYS FIRST (created before WebView)
    std::unique_ptr<juce::WebSliderRelay> thresholdRelay;
    std::unique_ptr<juce::WebSliderRelay> ratioRelay;
    std::unique_ptr<juce::WebSliderRelay> attackRelay;
    std::unique_ptr<juce::WebSliderRelay> releaseRelay;
    std::unique_ptr<juce::WebSliderRelay> fatRelay;
    std::unique_ptr<juce::WebSliderRelay> outputRelay;
    std::unique_ptr<juce::WebSliderRelay> mixRelay;

    // 2. WEBVIEW SECOND
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // 3. ATTACHMENTS LAST (created after WebView and Relays exist)
    std::unique_ptr<juce::WebSliderParameterAttachment> thresholdAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> ratioAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> attackAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> releaseAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> fatAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> outputAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> mixAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FatPressorAudioProcessorEditor)
};
