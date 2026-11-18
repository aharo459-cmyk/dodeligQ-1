cmake_minimum_required(VERSION 3.22)
project(dodeligQ-1 VERSION 1.0.0 LANGUAGES C CXX)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(dodeligQ1
    COMPANY_NAME "dodelig"
    PLUGIN_MANUFACTURER_CODE Dodg
    PLUGIN_CODE DQ01
    FORMATS VST3
    PRODUCT_NAME "dodeligQ-1"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx EQ)

juce_generate_juce_header(dodeligQ1)

target_sources(dodeligQ1 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(dodeligQ1 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(dodeligQ1 PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_audio_processors
    juce::juce_audio_plugin_client)

target_compile_features(dodeligQ1 PRIVATE cxx_std_17)
