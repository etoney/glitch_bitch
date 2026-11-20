cmake_minimum_required(VERSION 3.22)
# Must come BEFORE project(), or CMake ignores them: universal (Apple Silicon + Intel), macOS 11+
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
project(GLITCHWITCH VERSION 1.0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(GLITCHWITCH_JUCE_DIR)
    add_subdirectory(${GLITCHWITCH_JUCE_DIR} juce)   # local JUCE checkout (offline builds)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(GlitchWitch
    COMPANY_NAME "Castle Danger"
    PRODUCT_NAME "Glitch Witch"
    BUNDLE_ID com.castledanger.glitchwitch
    PLUGIN_MANUFACTURER_CODE Cdgr
    PLUGIN_CODE Gwch
    FORMATS VST3 AU Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(GlitchWitch PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(GlitchWitch PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(GlitchWitch
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline DSP test (no JUCE): behaviour checks + demo renders
add_executable(glitchwitch_render test/render_glitch.cpp)
target_include_directories(glitchwitch_render PRIVATE Source)

# Dev tool (not built by CI): offline run of the real plugin + editor screenshots
option(GLITCHWITCH_SNAPSHOT "Build the snapshot dev tool" OFF)
if(GLITCHWITCH_SNAPSHOT)
    add_executable(glitchwitch_snapshot test/snapshot.cpp)
    target_link_libraries(glitchwitch_snapshot PRIVATE GlitchWitch)
    target_include_directories(glitchwitch_snapshot PRIVATE $<TARGET_PROPERTY:GlitchWitch,INCLUDE_DIRECTORIES>)
    target_compile_definitions(glitchwitch_snapshot PRIVATE $<TARGET_PROPERTY:GlitchWitch,COMPILE_DEFINITIONS> JUCE_MODAL_LOOPS_PERMITTED=1)
endif()
