cmake_minimum_required(VERSION 3.22)

project(MoteField VERSION 0.3.13 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 91ad83ae34a81e0833b1a2b0866f54846370ae53
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(JUCE)

set(MOTEFIELD_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND MOTEFIELD_FORMATS AU)
endif()

option(MOTEFIELD_BUILD_AAX "Build AAX Native (requires PACE signing for retail Pro Tools)" OFF)
set(MOTEFIELD_AAX_SDK_PATH "" CACHE PATH "Optional external AAX SDK; empty uses JUCE's bundled SDK")
if(MOTEFIELD_BUILD_AAX)
    if(NOT APPLE AND NOT WIN32)
        message(FATAL_ERROR "MoteField AAX supports macOS and Windows only")
    endif()
    if(MOTEFIELD_AAX_SDK_PATH)
        juce_set_aax_sdk_path("${MOTEFIELD_AAX_SDK_PATH}")
    endif()
    list(APPEND MOTEFIELD_FORMATS AAX)
endif()

juce_add_plugin(MoteField
    COMPANY_NAME "Rango Labs"
    COMPANY_WEBSITE "https://www.rangolabs.co"
    BUNDLE_ID "com.rangolabs.motefield"
    PLUGIN_MANUFACTURER_CODE Rngo
    PLUGIN_CODE MtFd
    FORMATS ${MOTEFIELD_FORMATS}
    PRODUCT_NAME "MoteField"
    DESCRIPTION "Tempo-aware granular, glitch, delay, freeze, and phrase-loop effect"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
    AAX_IDENTIFIER "com.rangolabs.motefield"
    AAX_CATEGORY Effect
    DISABLE_AAX_MULTI_MONO TRUE
)

juce_add_binary_data(RangoAssets SOURCES Assets/RangoLogo.png Assets/OpenSauceMedium.ttf Assets/OpenSauceBold.ttf Assets/HardwarePrint.svg Assets/AcidEnclosure-v1.png Assets/AcidKnob-v1.png)
juce_generate_juce_header(MoteField)

target_sources(MoteField PRIVATE
    Source/DSP.cpp
    Source/DSP.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/Presets.cpp
    Source/Performance.cpp
    Source/PerformancePanel.h
    Source/Appearance.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

target_compile_definitions(MoteField PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JucePlugin_AAXDisableAudioSuite=1
)

target_link_libraries(MoteField
    PRIVATE
        RangoAssets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

option(MOTEFIELD_ENABLE_LTO "Enable link-time optimization for release packaging" OFF)
if(MOTEFIELD_ENABLE_LTO)
    target_link_libraries(MoteField PUBLIC juce::juce_recommended_lto_flags)
endif()

option(MOTEFIELD_BUILD_TESTS "Build the standalone DSP test suite" ON)
option(MOTEFIELD_BUILD_PREVIEW "Build the compiled UI preview and interaction harness" OFF)
if(MOTEFIELD_BUILD_PREVIEW)
    add_executable(MoteFieldPreview Tests/Preview.cpp)
    target_link_libraries(MoteFieldPreview PRIVATE MoteField)
    target_include_directories(MoteFieldPreview PRIVATE Source $<TARGET_PROPERTY:MoteField,INCLUDE_DIRECTORIES>)
    target_compile_definitions(MoteFieldPreview PRIVATE $<TARGET_PROPERTY:MoteField,COMPILE_DEFINITIONS>)
    add_executable(MoteFieldPromo Tests/Promo.cpp)
    target_link_libraries(MoteFieldPromo PRIVATE MoteField)
    target_include_directories(MoteFieldPromo PRIVATE Source $<TARGET_PROPERTY:MoteField,INCLUDE_DIRECTORIES>)
    target_compile_definitions(MoteFieldPromo PRIVATE $<TARGET_PROPERTY:MoteField,COMPILE_DEFINITIONS>)
    add_executable(MoteFieldDemo Tests/Demo.cpp)
    target_link_libraries(MoteFieldDemo PRIVATE MoteField)
    target_include_directories(MoteFieldDemo PRIVATE Source $<TARGET_PROPERTY:MoteField,INCLUDE_DIRECTORIES>)
    target_compile_definitions(MoteFieldDemo PRIVATE $<TARGET_PROPERTY:MoteField,COMPILE_DEFINITIONS>)
endif()
if(MOTEFIELD_BUILD_TESTS)
    enable_testing()
    add_executable(MoteFieldDSPTests
        Source/DSP.cpp
        Source/DSP.h
        Tests/DSPTests.cpp
    )
    target_include_directories(MoteFieldDSPTests PRIVATE Source)
    target_compile_features(MoteFieldDSPTests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(MoteFieldDSPTests PRIVATE /W4)
    else()
        target_compile_options(MoteFieldDSPTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME MoteFieldDSPTests COMMAND MoteFieldDSPTests)
    add_executable(MoteFieldClickTests Source/DSP.cpp Tests/ClickTests.cpp)
    target_include_directories(MoteFieldClickTests PRIVATE Source)
    target_compile_features(MoteFieldClickTests PRIVATE cxx_std_20)
    add_test(NAME MoteFieldClickTests COMMAND MoteFieldClickTests)
endif()
