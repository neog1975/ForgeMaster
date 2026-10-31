cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(ForgeMaster VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)

# JUCE: use a local checkout if JUCE_DIR is given (-DJUCE_DIR=/path/to/JUCE),
# otherwise download the pinned version automatically.
set(JUCE_DIR "" CACHE PATH "Optional path to a local JUCE checkout")
if (JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

option(FORGE_COPY_AFTER_BUILD "Copy the built plug-ins into the system plug-in folders" ON)
option(FORGE_BUILD_TOOLS "Build the offline render/test tools" ON)

juce_add_plugin(ForgeMaster
    COMPANY_NAME "Forge Audio"
    PLUGIN_MANUFACTURER_CODE Frga
    PLUGIN_CODE Frgm
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Forge Master"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Mastering
    COPY_PLUGIN_AFTER_BUILD ${FORGE_COPY_AFTER_BUILD})

target_sources(ForgeMaster PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(ForgeMaster PRIVATE Source)

target_compile_definitions(ForgeMaster PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(ForgeMaster
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if (FORGE_BUILD_TOOLS)
    # Offline render/test host (not part of the plugin)
    juce_add_console_app(ForgeRender PRODUCT_NAME "ForgeRender")
    target_sources(ForgeRender PRIVATE tools/RenderTest.cpp)
    target_compile_definitions(ForgeRender PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(ForgeRender PRIVATE juce::juce_audio_processors juce::juce_audio_formats juce::juce_recommended_config_flags)

    # Drives the processor directly to test the one-click auto master offline
    juce_add_console_app(AutoMasterTest PRODUCT_NAME "AutoMasterTest")
    target_sources(AutoMasterTest PRIVATE tools/AutoMasterTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(AutoMasterTest PRIVATE Source)
    target_compile_definitions(AutoMasterTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Forge Master")
    target_link_libraries(AutoMasterTest PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
