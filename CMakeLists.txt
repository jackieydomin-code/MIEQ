cmake_minimum_required(VERSION 3.22)

# Runtime estático: el plugin no necesita instalar los Redistributables de Visual C++
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(MiEQ VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

option(MIEQ_COPY_AFTER_BUILD "Copiar el VST3 a la carpeta del sistema tras compilar (requiere admin en Windows)" OFF)

juce_add_plugin(MiEQ
    COMPANY_NAME            "MiEQ"
    PLUGIN_MANUFACTURER_CODE Mieq
    PLUGIN_CODE             MeQ1
    FORMATS                 VST3
    PRODUCT_NAME            "Mi EQ"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    VST3_CATEGORIES         Fx EQ
    COPY_PLUGIN_AFTER_BUILD ${MIEQ_COPY_AFTER_BUILD})

target_sources(MiEQ PRIVATE
    Source/Parameters.h
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp)

target_compile_definitions(MiEQ PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MiEQ
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
