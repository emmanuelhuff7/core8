cmake_minimum_required(VERSION 3.22)

# Universal binary (Apple Silicon + Intel), macOS 11+
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

project(CORE8 VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use a local JUCE checkout with -DJUCE_PATH=/path/to/JUCE, otherwise download JUCE 8.0.4.
if(DEFINED JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

set(CORE8_FORMATS VST3)
if(APPLE)
    list(APPEND CORE8_FORMATS AU)
endif()

juce_add_plugin(CORE8
    COMPANY_NAME "ZVLE"
    COMPANY_WEBSITE ""
    BUNDLE_ID "com.zvle.core8"
    PLUGIN_MANUFACTURER_CODE Zvle
    PLUGIN_CODE Cor8
    FORMATS ${CORE8_FORMATS}
    PRODUCT_NAME "CORE8"
    DESCRIPTION "DJ transition effects"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Delay Reverb)

target_sources(CORE8 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DSP/FxEngine.cpp
    Source/UI/FxDisplay.cpp)

target_compile_definitions(CORE8 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(CORE8
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
