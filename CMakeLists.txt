cmake_minimum_required(VERSION 3.22)
project(SofGlitch VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)

set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE" CACHE PATH "Path to a JUCE 8 checkout")
add_subdirectory(${JUCE_DIR} JUCE)

juce_add_plugin(SofGlitch
    COMPANY_NAME "SofAudio"
    PLUGIN_MANUFACTURER_CODE Sofa
    PLUGIN_CODE Bto1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "SofGlitch"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
)

juce_add_binary_data(SgAssets SOURCES
    Assets/Silkscreen-Regular.ttf
    Assets/Silkscreen-Bold.ttf
)

target_sources(SofGlitch PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(SofGlitch PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_MODAL_LOOPS_PERMITTED=1
)

target_link_libraries(SofGlitch PRIVATE
    SgAssets
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_gui_basics
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# ---- console test of the rule engine
juce_add_console_app(SofGlitchTest PRODUCT_NAME "SofGlitchTest")
target_sources(SofGlitchTest PRIVATE Tests/EngineTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(SofGlitchTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_Name="SofGlitch" JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(SofGlitchTest PRIVATE SgAssets juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
