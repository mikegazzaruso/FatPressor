# ENHANCE-001: Preset System Implementation

## Requested
2026-01-22

## Description
Implement the preset system that is currently only present graphically in the UI.
Features:
- 20 factory presets divided into 4 categories (read-only)
- User presets with category organization (read/write)
- Presets stored on filesystem
- Full UI integration (browser, navigation, save dialog)

## Impact Analysis

### Parameters
- No new parameters (presets save/load existing 7 parameters)

### DSP
- No DSP changes (PresetManager is separate from audio processing)

### UI
- Implement preset navigation handlers (prev/next)
- Implement preset browser modal with categories
- Implement save dialog for user presets
- Wire to C++ PresetManager via native functions

### New Files
- `src/PresetManager.h` - Preset management class
- `src/PresetManager.cpp` - Implementation
- `presets/Factory/` - Factory preset files
- `ui/public/js/presets.js` - Preset UI logic

## Categories & Factory Presets

### Drums (5)
1. Punchy Kick
2. Snare Snap
3. Room Glue
4. Parallel Smash
5. Drum Bus

### Vocals (5)
1. Gentle Lead
2. Radio Ready
3. Intimate
4. De-Harsh
5. Background Vox

### Bass (5)
1. Tight Low
2. Tube Warmth
3. Slap Bass
4. Sub Control
5. Vintage Bass

### Mix Bus (5)
1. Glue Master
2. Loud & Proud
3. Transparent
4. Analog Sum
5. Final Touch

## Storage Structure

```
~/Library/Application Support/Sylfo/FatPressor/
├── Factory/           # Read-only (copied on first run)
│   ├── Drums/
│   │   ├── Punchy Kick.fppreset
│   │   └── ...
│   ├── Vocals/
│   ├── Bass/
│   └── Mix Bus/
└── User/              # User-created presets
    ├── Drums/
    ├── Vocals/
    ├── Bass/
    ├── Mix Bus/
    └── Uncategorized/
```

## Preset File Format (.fppreset)

XML format compatible with JUCE ValueTree:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<FatPressorPreset version="1" name="Punchy Kick" category="Drums" factory="true">
  <Parameters>
    <PARAM id="threshold" value="-18.0"/>
    <PARAM id="ratio" value="4.0"/>
    <PARAM id="attack" value="5.0"/>
    <PARAM id="release" value="150.0"/>
    <PARAM id="fat" value="65.0"/>
    <PARAM id="output" value="3.0"/>
    <PARAM id="mix" value="100.0"/>
  </Parameters>
</FatPressorPreset>
```

## Amendment to Spec
See spec.yaml changes (presets section added)

## Approved
2026-01-22

## Tasks
- task_E001: Implement PresetManager C++ class
- task_E002: Create 20 factory presets with tuned values
- task_E003: Implement preset browser UI (JavaScript)
- task_E004: Wire PresetManager to WebView native functions
- task_E005: Test preset system end-to-end
- task_E006: Version bump to 0.1.0

## Completed
[pending]
