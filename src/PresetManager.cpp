#include "PresetManager.h"

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& apvts_)
    : apvts(apvts_)
{
    // Set up directory paths
#if JUCE_MAC
    presetsDirectory = juce::File::getSpecialLocation(
        juce::File::userApplicationDataDirectory)
        .getChildFile("Application Support/Sylfo/FatPressor");
#elif JUCE_WINDOWS
    presetsDirectory = juce::File::getSpecialLocation(
        juce::File::userApplicationDataDirectory)
        .getChildFile("Sylfo/FatPressor");
#else
    presetsDirectory = juce::File::getSpecialLocation(
        juce::File::userApplicationDataDirectory)
        .getChildFile(".sylfo/FatPressor");
#endif

    factoryDirectory = presetsDirectory.getChildFile("Factory");
    userDirectory = presetsDirectory.getChildFile("User");
}

PresetManager::~PresetManager()
{
    listeners.clear();
}

void PresetManager::initialize()
{
    createDirectoryStructure();

    // Install factory presets if not already done
    if (!areFactoryPresetsInstalled())
        installFactoryPresets();

    scanPresets();

    // Load first preset if available
    if (!allPresets.isEmpty())
    {
        currentPreset = allPresets[0];
        currentPresetIndex = 0;
    }
    else
    {
        // Default preset
        currentPreset.name = "Default";
        currentPreset.category = "Uncategorized";
        currentPreset.isFactory = false;
    }
}

void PresetManager::createDirectoryStructure()
{
    // Create main directory
    if (!presetsDirectory.exists())
        presetsDirectory.createDirectory();

    // Create Factory subdirectories
    for (const auto& category : categories)
    {
        auto factoryCatDir = factoryDirectory.getChildFile(category);
        if (!factoryCatDir.exists())
            factoryCatDir.createDirectory();
    }

    // Create User subdirectories
    for (const auto& category : categories)
    {
        auto userCatDir = userDirectory.getChildFile(category);
        if (!userCatDir.exists())
            userCatDir.createDirectory();
    }
}

bool PresetManager::areFactoryPresetsInstalled() const
{
    // Check if at least one factory preset exists
    auto drumsDir = factoryDirectory.getChildFile("Drums");
    return drumsDir.exists() &&
           drumsDir.getNumberOfChildFiles(juce::File::findFiles, "*.fppreset") > 0;
}

void PresetManager::installFactoryPresets()
{
    createDirectoryStructure();

    // === DRUMS CATEGORY (5 presets) ===

    // 1. Punchy Kick - Fast attack, medium release, moderate compression
    createFactoryPreset("Punchy Kick", "Drums",
        -24.0f,   // threshold
        4.0f,     // ratio
        5.0f,     // attack (fast for transient punch)
        150.0f,   // release
        45.0f,    // fat (moderate warmth)
        2.0f,     // output
        100.0f);  // mix

    // 2. Snare Snap - Very fast attack, quick release
    createFactoryPreset("Snare Snap", "Drums",
        -18.0f,   // threshold
        3.5f,     // ratio
        1.0f,     // attack (super fast)
        80.0f,    // release (quick)
        35.0f,    // fat
        3.0f,     // output
        100.0f);  // mix

    // 3. Room Glue - Gentle compression for room mics
    createFactoryPreset("Room Glue", "Drums",
        -30.0f,   // threshold (catch more)
        2.5f,     // ratio (gentle)
        15.0f,    // attack (let transients through)
        300.0f,   // release (smooth)
        55.0f,    // fat (warm room sound)
        1.0f,     // output
        100.0f);  // mix

    // 4. Parallel Smash - Heavy compression for parallel blend
    createFactoryPreset("Parallel Smash", "Drums",
        -35.0f,   // threshold (heavy compression)
        8.0f,     // ratio (aggressive)
        3.0f,     // attack
        120.0f,   // release
        75.0f,    // fat (lots of color)
        6.0f,     // output (makeup gain)
        50.0f);   // mix (parallel blend)

    // 5. Drum Bus - Overall drum bus glue
    createFactoryPreset("Drum Bus", "Drums",
        -20.0f,   // threshold
        3.0f,     // ratio
        10.0f,    // attack (preserve transients)
        200.0f,   // release
        40.0f,    // fat
        2.0f,     // output
        100.0f);  // mix

    // === VOCALS CATEGORY (5 presets) ===

    // 6. Gentle Lead - Transparent vocal compression
    createFactoryPreset("Gentle Lead", "Vocals",
        -22.0f,   // threshold
        2.5f,     // ratio (gentle)
        12.0f,    // attack
        180.0f,   // release
        25.0f,    // fat (subtle warmth)
        1.5f,     // output
        100.0f);  // mix

    // 7. Radio Ready - Aggressive broadcast-style
    createFactoryPreset("Radio Ready", "Vocals",
        -18.0f,   // threshold
        5.0f,     // ratio (more aggressive)
        8.0f,     // attack
        150.0f,   // release
        50.0f,    // fat (present)
        4.0f,     // output
        100.0f);  // mix

    // 8. Intimate - Soft, close vocal sound
    createFactoryPreset("Intimate", "Vocals",
        -28.0f,   // threshold
        2.0f,     // ratio (very gentle)
        20.0f,    // attack (slow)
        250.0f,   // release
        60.0f,    // fat (warm, intimate)
        0.0f,     // output
        100.0f);  // mix

    // 9. De-Harsh - Tame harsh vocals
    createFactoryPreset("De-Harsh", "Vocals",
        -16.0f,   // threshold
        3.0f,     // ratio
        5.0f,     // attack
        200.0f,   // release
        70.0f,    // fat (round off harshness)
        2.0f,     // output
        85.0f);   // mix

    // 10. Background Vox - Sit vocals back in mix
    createFactoryPreset("Background Vox", "Vocals",
        -25.0f,   // threshold
        4.0f,     // ratio
        15.0f,    // attack
        300.0f,   // release
        35.0f,    // fat
        -2.0f,    // output (slightly reduced)
        100.0f);  // mix

    // === BASS CATEGORY (5 presets) ===

    // 11. Tight Low - Controlled bass
    createFactoryPreset("Tight Low", "Bass",
        -20.0f,   // threshold
        4.5f,     // ratio
        8.0f,     // attack
        100.0f,   // release (quick for tight bass)
        30.0f,    // fat
        2.0f,     // output
        100.0f);  // mix

    // 12. Tube Warmth - Warm bass saturation
    createFactoryPreset("Tube Warmth", "Bass",
        -25.0f,   // threshold
        3.0f,     // ratio
        15.0f,    // attack
        200.0f,   // release
        80.0f,    // fat (lots of tube warmth)
        1.0f,     // output
        100.0f);  // mix

    // 13. Slap Bass - Preserve attack, control sustain
    createFactoryPreset("Slap Bass", "Bass",
        -18.0f,   // threshold
        3.5f,     // ratio
        2.0f,     // attack (very fast after transient)
        80.0f,    // release
        40.0f,    // fat
        3.0f,     // output
        100.0f);  // mix

    // 14. Sub Control - Tame sub frequencies
    createFactoryPreset("Sub Control", "Bass",
        -30.0f,   // threshold (catch subs)
        6.0f,     // ratio (control)
        20.0f,    // attack
        250.0f,   // release (smooth)
        20.0f,    // fat (clean-ish)
        4.0f,     // output
        100.0f);  // mix

    // 15. Vintage Bass - Classic bass compression
    createFactoryPreset("Vintage Bass", "Bass",
        -22.0f,   // threshold
        3.0f,     // ratio
        12.0f,    // attack
        180.0f,   // release
        65.0f,    // fat (vintage color)
        2.0f,     // output
        100.0f);  // mix

    // === MIX BUS CATEGORY (5 presets) ===

    // 16. Glue Master - Classic mix bus glue
    createFactoryPreset("Glue Master", "Mix Bus",
        -18.0f,   // threshold
        2.0f,     // ratio (gentle)
        25.0f,    // attack (slow, preserve transients)
        300.0f,   // release (slow, musical)
        30.0f,    // fat (subtle)
        1.0f,     // output
        100.0f);  // mix

    // 17. Loud & Proud - Aggressive mastering style
    createFactoryPreset("Loud & Proud", "Mix Bus",
        -12.0f,   // threshold (heavy)
        3.5f,     // ratio
        15.0f,    // attack
        200.0f,   // release
        50.0f,    // fat
        4.0f,     // output (loud)
        100.0f);  // mix

    // 18. Transparent - Minimal coloration
    createFactoryPreset("Transparent", "Mix Bus",
        -24.0f,   // threshold
        1.5f,     // ratio (very gentle)
        30.0f,    // attack (very slow)
        400.0f,   // release
        10.0f,    // fat (minimal)
        0.5f,     // output
        100.0f);  // mix

    // 19. Analog Sum - Analog console vibe
    createFactoryPreset("Analog Sum", "Mix Bus",
        -20.0f,   // threshold
        2.5f,     // ratio
        20.0f,    // attack
        350.0f,   // release
        55.0f,    // fat (analog warmth)
        1.5f,     // output
        100.0f);  // mix

    // 20. Final Touch - Light final polish
    createFactoryPreset("Final Touch", "Mix Bus",
        -22.0f,   // threshold
        2.0f,     // ratio
        30.0f,    // attack
        500.0f,   // release (very slow)
        25.0f,    // fat
        1.0f,     // output
        100.0f);  // mix
}

void PresetManager::scanPresets()
{
    allPresets.clear();

    // Scan factory presets first
    scanDirectory(factoryDirectory, true);

    // Then user presets
    scanDirectory(userDirectory, false);
}

void PresetManager::scanDirectory(const juce::File& directory, bool isFactory)
{
    if (!directory.exists())
        return;

    for (const auto& category : categories)
    {
        auto categoryDir = directory.getChildFile(category);
        if (!categoryDir.exists())
            continue;

        auto presetFiles = categoryDir.findChildFiles(
            juce::File::findFiles, false, juce::String("*") + presetExtension);

        for (const auto& file : presetFiles)
        {
            PresetInfo info;
            info.name = file.getFileNameWithoutExtension();
            info.category = category;
            info.file = file;
            info.isFactory = isFactory;

            allPresets.add(info);
        }
    }
}

juce::Array<PresetManager::PresetInfo> PresetManager::getAllPresets() const
{
    return allPresets;
}

juce::Array<PresetManager::PresetInfo> PresetManager::getPresetsForCategory(
    const juce::String& category) const
{
    juce::Array<PresetInfo> result;

    for (const auto& preset : allPresets)
    {
        if (preset.category == category)
            result.add(preset);
    }

    return result;
}

juce::Array<PresetManager::PresetInfo> PresetManager::getFactoryPresets() const
{
    juce::Array<PresetInfo> result;

    for (const auto& preset : allPresets)
    {
        if (preset.isFactory)
            result.add(preset);
    }

    return result;
}

juce::Array<PresetManager::PresetInfo> PresetManager::getUserPresets() const
{
    juce::Array<PresetInfo> result;

    for (const auto& preset : allPresets)
    {
        if (!preset.isFactory)
            result.add(preset);
    }

    return result;
}

int PresetManager::getCurrentPresetIndex() const
{
    return currentPresetIndex;
}

void PresetManager::loadPreset(const PresetInfo& preset)
{
    DBG("[PresetManager] loadPreset: " + preset.name);

    if (loadPresetFromFile(preset.file))
    {
        DBG("[PresetManager] loadPresetFromFile succeeded");
        currentPreset = preset;

        // Update index
        for (int i = 0; i < allPresets.size(); ++i)
        {
            if (allPresets[i].file == preset.file)
            {
                currentPresetIndex = i;
                break;
            }
        }

        DBG("[PresetManager] Calling notifyPresetChanged, index=" + juce::String(currentPresetIndex));
        notifyPresetChanged();
    }
    else
    {
        DBG("[PresetManager] loadPresetFromFile FAILED!");
    }
}

void PresetManager::loadPresetByIndex(int index)
{
    if (index >= 0 && index < allPresets.size())
    {
        loadPreset(allPresets[index]);
    }
}

void PresetManager::loadNextPreset()
{
    if (allPresets.isEmpty())
        return;

    int nextIndex = (currentPresetIndex + 1) % allPresets.size();
    loadPresetByIndex(nextIndex);
}

void PresetManager::loadPreviousPreset()
{
    if (allPresets.isEmpty())
        return;

    int prevIndex = currentPresetIndex - 1;
    if (prevIndex < 0)
        prevIndex = allPresets.size() - 1;

    loadPresetByIndex(prevIndex);
}

bool PresetManager::saveUserPreset(const juce::String& name, const juce::String& category)
{
    // Validate category
    juce::String safeCategory = category;
    if (!categories.contains(safeCategory))
        safeCategory = "Uncategorized";

    // Create file path
    auto categoryDir = userDirectory.getChildFile(safeCategory);
    if (!categoryDir.exists())
        categoryDir.createDirectory();

    auto presetFile = categoryDir.getChildFile(name + presetExtension);

    // Don't overwrite factory presets (they're in a different directory anyway)
    // But check if trying to use a factory preset name
    auto factoryCategoryDir = factoryDirectory.getChildFile(safeCategory);
    auto factoryFile = factoryCategoryDir.getChildFile(name + presetExtension);
    if (factoryFile.exists())
    {
        // Can't overwrite factory preset - append "(User)" to name
        presetFile = categoryDir.getChildFile(name + " (User)" + presetExtension);
    }

    if (savePresetToFile(presetFile, name, safeCategory, false))
    {
        // Rescan and update
        scanPresets();
        notifyPresetListChanged();

        // Set as current
        for (int i = 0; i < allPresets.size(); ++i)
        {
            if (allPresets[i].file == presetFile)
            {
                currentPreset = allPresets[i];
                currentPresetIndex = i;
                notifyPresetChanged();
                break;
            }
        }

        return true;
    }

    return false;
}

bool PresetManager::deleteUserPreset(const PresetInfo& preset)
{
    DBG("[PresetManager] deleteUserPreset called for: " + preset.name);
    DBG("[PresetManager] isFactory: " + juce::String(preset.isFactory ? "true" : "false"));
    DBG("[PresetManager] file: " + preset.file.getFullPathName());

    // Can't delete factory presets
    if (preset.isFactory)
    {
        DBG("[PresetManager] Cannot delete factory preset");
        return false;
    }

    DBG("[PresetManager] Attempting to delete file...");
    if (preset.file.deleteFile())
    {
        DBG("[PresetManager] File deleted successfully");
        scanPresets();
        notifyPresetListChanged();

        // If deleted current preset, load first available
        if (currentPreset.file == preset.file && !allPresets.isEmpty())
        {
            loadPresetByIndex(0);
        }

        return true;
    }

    DBG("[PresetManager] Failed to delete file");
    return false;
}

bool PresetManager::deleteUserPreset(int index)
{
    DBG("[PresetManager] deleteUserPreset(int) called with index: " + juce::String(index));
    DBG("[PresetManager] allPresets.size(): " + juce::String(allPresets.size()));

    if (index < 0 || index >= allPresets.size())
    {
        DBG("[PresetManager] Index out of range");
        return false;
    }

    const auto& preset = allPresets[index];
    DBG("[PresetManager] Preset at index: " + preset.name + " isFactory: " + juce::String(preset.isFactory ? "true" : "false"));

    return deleteUserPreset(preset);
}

bool PresetManager::savePresetToFile(const juce::File& file, const juce::String& name,
                                      const juce::String& category, bool isFactory)
{
    // Create XML structure
    auto xml = std::make_unique<juce::XmlElement>("FatPressorPreset");
    xml->setAttribute("version", 1);
    xml->setAttribute("name", name);
    xml->setAttribute("category", category);
    xml->setAttribute("factory", isFactory);

    // Add parameters from APVTS
    auto state = apvts.copyState();
    auto paramsXml = state.createXml();
    if (paramsXml)
    {
        xml->addChildElement(paramsXml.release());
    }

    // Write to file
    return xml->writeTo(file);
}

bool PresetManager::loadPresetFromFile(const juce::File& file)
{
    DBG("[PresetManager] loadPresetFromFile: " + file.getFullPathName());

    if (!file.exists())
    {
        DBG("[PresetManager] ERROR: File does not exist!");
        return false;
    }

    auto xml = juce::XmlDocument::parse(file);
    if (!xml || !xml->hasTagName("FatPressorPreset"))
    {
        DBG("[PresetManager] ERROR: Invalid XML or wrong tag name");
        return false;
    }

    // Find Parameters element
    auto* paramsXml = xml->getChildByName("Parameters");
    if (paramsXml)
    {
        DBG("[PresetManager] Found Parameters element, iterating...");
        int paramCount = 0;

        // Iterate through PARAM elements and set each parameter individually
        // This ensures WebSliderAttachments get notified properly
        for (auto* paramXml : paramsXml->getChildIterator())
        {
            if (paramXml->hasTagName("PARAM"))
            {
                juce::String paramId = paramXml->getStringAttribute("id");
                float value = static_cast<float>(paramXml->getDoubleAttribute("value"));

                if (auto* param = apvts.getParameter(paramId))
                {
                    // Convert from actual value to normalized 0-1 range
                    float normalized = param->convertTo0to1(value);
                    DBG("[PresetManager] Setting " + paramId + ": scaled=" + juce::String(value)
                        + " -> normalized=" + juce::String(normalized));
                    param->setValueNotifyingHost(normalized);
                    paramCount++;
                }
                else
                {
                    DBG("[PresetManager] WARNING: Parameter not found: " + paramId);
                }
            }
        }

        DBG("[PresetManager] Set " + juce::String(paramCount) + " parameters");
        return true;
    }

    DBG("[PresetManager] ERROR: No Parameters element found!");
    return false;
}

void PresetManager::addListener(Listener* listener)
{
    listeners.add(listener);
}

void PresetManager::removeListener(Listener* listener)
{
    listeners.remove(listener);
}

void PresetManager::createFactoryPreset(const juce::String& name, const juce::String& category,
                                         float threshold, float ratio, float attack, float release,
                                         float fat, float output, float mix)
{
    auto categoryDir = factoryDirectory.getChildFile(category);
    if (!categoryDir.exists())
        categoryDir.createDirectory();

    auto presetFile = categoryDir.getChildFile(name + presetExtension);

    // Don't overwrite existing factory presets
    if (presetFile.exists())
        return;

    // Temporarily set parameters to preset values
    auto setParam = [this](const juce::String& id, float value) {
        if (auto* param = apvts.getParameter(id))
            param->setValueNotifyingHost(param->convertTo0to1(value));
    };

    setParam("threshold", threshold);
    setParam("ratio", ratio);
    setParam("attack", attack);
    setParam("release", release);
    setParam("fat", fat);
    setParam("output", output);
    setParam("mix", mix);

    // Save using the standard method (ensures correct APVTS format)
    savePresetToFile(presetFile, name, category, true);
}

void PresetManager::notifyPresetChanged()
{
    listeners.call([this](Listener& l) {
        l.presetChanged(currentPreset);
    });
}

void PresetManager::notifyPresetListChanged()
{
    listeners.call([](Listener& l) {
        l.presetListChanged();
    });
}
