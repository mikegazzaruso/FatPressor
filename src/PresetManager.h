#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_data_structures/juce_data_structures.h>

/**
 * @brief Preset Manager for FatPressor
 *
 * Manages factory and user presets with file-based storage.
 *
 * Storage Structure:
 * ~/Library/Application Support/Sylfo/FatPressor/
 * ├── Factory/           # Read-only presets (copied on first run)
 * │   ├── Drums/
 * │   ├── Vocals/
 * │   ├── Bass/
 * │   └── Mix Bus/
 * └── User/              # User-created presets
 *     ├── Drums/
 *     ├── Vocals/
 *     ├── Bass/
 *     ├── Mix Bus/
 *     └── Uncategorized/
 */
class PresetManager
{
public:
    // Categories
    static inline const juce::StringArray categories = {
        "Drums", "Vocals", "Bass", "Mix Bus", "Uncategorized"
    };

    // Preset info structure
    struct PresetInfo
    {
        juce::String name;
        juce::String category;
        juce::File file;
        bool isFactory;

        bool operator==(const PresetInfo& other) const {
            return file == other.file;
        }
    };

    PresetManager(juce::AudioProcessorValueTreeState& apvts);
    ~PresetManager();

    // Initialization
    void initialize();

    // Preset listing
    juce::Array<PresetInfo> getAllPresets() const;
    juce::Array<PresetInfo> getPresetsForCategory(const juce::String& category) const;
    juce::Array<PresetInfo> getFactoryPresets() const;
    juce::Array<PresetInfo> getUserPresets() const;

    // Current preset
    const PresetInfo& getCurrentPreset() const { return currentPreset; }
    int getCurrentPresetIndex() const;
    int getTotalPresetCount() const { return allPresets.size(); }

    // Navigation
    void loadPreset(const PresetInfo& preset);
    void loadPresetByIndex(int index);
    void loadNextPreset();
    void loadPreviousPreset();

    // Save user preset
    bool saveUserPreset(const juce::String& name, const juce::String& category);
    bool deleteUserPreset(const PresetInfo& preset);
    bool deleteUserPreset(int index);

    // Factory preset installation
    void installFactoryPresets();
    bool areFactoryPresetsInstalled() const;

    // Listeners
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void presetChanged(const PresetInfo& newPreset) = 0;
        virtual void presetListChanged() = 0;
    };

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

private:
    juce::AudioProcessorValueTreeState& apvts;

    juce::File presetsDirectory;
    juce::File factoryDirectory;
    juce::File userDirectory;

    juce::Array<PresetInfo> allPresets;
    PresetInfo currentPreset;
    int currentPresetIndex = 0;

    juce::ListenerList<Listener> listeners;

    // File extension
    static constexpr const char* presetExtension = ".fppreset";

    // Internal methods
    void createDirectoryStructure();
    void scanPresets();
    void scanDirectory(const juce::File& directory, bool isFactory);

    bool savePresetToFile(const juce::File& file, const juce::String& name,
                          const juce::String& category, bool isFactory);
    bool loadPresetFromFile(const juce::File& file);

    // Factory preset creation helper
    void createFactoryPreset(const juce::String& name, const juce::String& category,
                             float threshold, float ratio, float attack, float release,
                             float fat, float output, float mix);

    void notifyPresetChanged();
    void notifyPresetListChanged();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};
