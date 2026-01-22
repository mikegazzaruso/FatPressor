# FatPressor - Tube-Driven Optical Compressor
# A VYBR Framework Plugin by Sylfo
#
# ============================================================================
# JUCE LICENSING NOTICE
# ============================================================================
# This plugin uses the JUCE Framework (https://juce.com).
# JUCE is NOT included in this project - it is referenced from your local
# installation. You are responsible for your own JUCE licensing compliance.
#
# Options:
#   - AGPL-3.0: Open source, source code must be available
#   - Commercial: Purchase license from https://juce.com/get-juce/
#
# VYBR framework does not grant any JUCE license.
# ============================================================================

cmake_minimum_required(VERSION 3.24)
project(FatPressor VERSION 0.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE path - references YOUR local JUCE installation
# JUCE is NOT distributed with this plugin
if(NOT DEFINED JUCE_PATH)
    if(WIN32)
        set(JUCE_PATH "C:/JUCE" CACHE PATH "Path to JUCE")
    else()
        set(JUCE_PATH "/Applications/JUCE" CACHE PATH "Path to JUCE")
    endif()
endif()

add_subdirectory(${JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE)

# Plugin definition
juce_add_plugin(FatPressor
    COMPANY_NAME "Sylfo"
    PLUGIN_MANUFACTURER_CODE Sylf
    PLUGIN_CODE FtPr
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "FatPressor"

    BUNDLE_ID "com.sylfo.fatpressor"

    # Effect plugin settings
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE

    # WebView support (REQUIRED for WebView UI)
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE          # Windows WebView2 support
)

# Source files
target_sources(FatPressor
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginProcessor.h
        src/PluginEditor.cpp
        src/PluginEditor.h
        src/PresetManager.cpp
        src/PresetManager.h
)

# Binary resources (UI files)
juce_add_binary_data(FatPressorData
    SOURCES
        ui/public/index.html
        ui/public/css/style.css
        ui/public/js/main.js
        ui/public/js/juce/index.js
        ui/public/js/juce/check_native_interop.js
)

# Link libraries
target_link_libraries(FatPressor
    PRIVATE
        FatPressorData
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra        # Required for WebView
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Generate JUCE header (MUST come after target_link_libraries)
juce_generate_juce_header(FatPressor)

# Compile definitions
target_compile_definitions(FatPressor
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2=1   # Enable WebView2 on Windows
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

# macOS specific
if(APPLE)
    find_library(IOKIT_FRAMEWORK IOKit)
    if(IOKIT_FRAMEWORK)
        target_link_libraries(FatPressor PRIVATE ${IOKIT_FRAMEWORK})
    endif()
endif()
