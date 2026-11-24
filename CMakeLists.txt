cmake_minimum_required(VERSION 3.22)

project(MultiRack VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static MSVC runtime: the plugin does not need the Visual C++ redistributable on the user's PC.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)

# A local JUCE checkout can be used with -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.12
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

set(MR_FORMATS VST3)
if(NOT MR_NO_STANDALONE)
    list(APPEND MR_FORMATS Standalone)
endif()

juce_add_plugin(MultiRack
    COMPANY_NAME                "MultiRack Audio"
    COMPANY_WEBSITE             ""
    PLUGIN_MANUFACTURER_CODE    Mrka
    PLUGIN_CODE                 Mrk1
    FORMATS                     ${MR_FORMATS}
    PRODUCT_NAME                "Multi Rack"
    DESCRIPTION                 "Modular channel-strip rack"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Dynamics EQ
    BUNDLE_ID                   com.multirack.multirack)

target_sources(MultiRack PRIVATE
    Source/ModuleDefs.cpp
    Source/Modules.cpp
    Source/PluginProcessor.cpp
    Source/Presets.cpp
    Source/PluginEditor.cpp
    Source/UI/LookAndFeel.cpp
    Source/UI/Widgets.cpp
    Source/UI/ModulePanel.cpp
    Source/UI/RackViews.cpp)

target_include_directories(MultiRack PRIVATE Source)

target_compile_definitions(MultiRack PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_SILENCE_XCODE_15_LINKER_WARNING=1)

target_link_libraries(MultiRack
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional offline test harness: -DMR_BUILD_TESTS=ON
option(MR_BUILD_TESTS "Build the offline DSP/UI test harness" OFF)
if(MR_BUILD_TESTS)
    juce_add_console_app(MultiRackTest PRODUCT_NAME "MultiRackTest")
    target_sources(MultiRackTest PRIVATE
        tests/TestMain.cpp
        Source/ModuleDefs.cpp
        Source/Modules.cpp
        Source/PluginProcessor.cpp
        Source/Presets.cpp
        Source/PluginEditor.cpp
        Source/UI/LookAndFeel.cpp
        Source/UI/Widgets.cpp
        Source/UI/ModulePanel.cpp
        Source/UI/RackViews.cpp)
    target_include_directories(MultiRackTest PRIVATE Source)
    target_compile_definitions(MultiRackTest PRIVATE
        JucePlugin_Name="Multi Rack"
        JucePlugin_VersionString="1.0.0"
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(MultiRackTest PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
