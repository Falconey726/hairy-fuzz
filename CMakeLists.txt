cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(PhilsFuzz VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use a local JUCE checkout with -DJUCE_PATH=/path/to/JUCE, otherwise download it.
set(JUCE_PATH "" CACHE PATH "Optional path to a local JUCE checkout")
if(JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(PhilsFuzz
    COMPANY_NAME "Falconey"
    BUNDLE_ID "com.falconey.philsfuzz"
    PLUGIN_MANUFACTURER_CODE Flcn
    PLUGIN_CODE Phfz
    PLUGIN_NAME "Phil's Fuzz"
    DESCRIPTION "70s fuzz with germanium/silicon switch and six distortion modes"
    PRODUCT_NAME "Phils Fuzz"
    FORMATS AU VST3 Standalone
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Distortion
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(PhilsFuzzData SOURCES Resources/phil.png)
set_target_properties(PhilsFuzzData PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(PhilsFuzz PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(PhilsFuzz PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(PhilsFuzz
    PRIVATE
        PhilsFuzzData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional offline render test (renders audio + an editor screenshot).
option(PHILSFUZZ_BUILD_TESTS "Build the offline render test" OFF)
if(PHILSFUZZ_BUILD_TESTS)
    juce_add_console_app(PhilsFuzzRenderTest PRODUCT_NAME "PhilsFuzzRenderTest")
    target_sources(PhilsFuzzRenderTest PRIVATE tests/RenderTest.cpp)
    target_link_libraries(PhilsFuzzRenderTest PRIVATE PhilsFuzz juce::juce_audio_formats)
endif()
