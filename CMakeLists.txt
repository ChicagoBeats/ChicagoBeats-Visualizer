cmake_minimum_required(VERSION 3.15)
project(ChicagoBeatsVisualizer VERSION 0.1.0)

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 6.1.6
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(ChicagoBeatsVisualizer
  COMPANY_NAME "Chicago Beats"
  PLUGIN_MANUFACTURER_CODE Cbts
  PLUGIN_CODE Cbvz
  FORMATS VST3 Standalone
  PRODUCT_NAME "Chicago Beats Visualizer"
  NEEDS_MIDI_INPUT TRUE
  IS_SYNTH FALSE)

juce_generate_juce_header(ChicagoBeatsVisualizer)

target_sources(ChicagoBeatsVisualizer PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

target_compile_definitions(ChicagoBeatsVisualizer PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(ChicagoBeatsVisualizer
  PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_opengl
  PUBLIC juce::juce_recommended_config_flags)