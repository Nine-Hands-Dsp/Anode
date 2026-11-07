# Outil hors plugin : compile le processeur + la fenetre d'ANODE sous Linux, exporte des captures PNG et des sons de test
cmake_minimum_required(VERSION 3.22)
project(AnodeRender VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
add_subdirectory(${JUCE_DIR} JUCE)
set(W ${CMAKE_CURRENT_SOURCE_DIR}/../..)
juce_add_binary_data(AnodeAssets SOURCES
  ${W}/background.jpg ${W}/knob_strip.png ${W}/selector_strip.png ${W}/toggle_strip.png
  ${W}/tubezone_6L6.jpg ${W}/tubezone_EL34.jpg ${W}/tubezone_EL84.jpg ${W}/tubezone_KT88.jpg
  ${W}/glow_6L6.png ${W}/glow_EL34.png ${W}/glow_EL84.png ${W}/glow_KT88.png
  ${W}/redplate_6L6.png ${W}/redplate_EL34.png ${W}/redplate_EL84.png ${W}/redplate_KT88.png ${W}/Poppins-Medium.ttf)
juce_add_console_app(AnodeRender PRODUCT_NAME "AnodeRender")
target_sources(AnodeRender PRIVATE main.cpp ${W}/PluginProcessor.cpp ${W}/PluginEditor.cpp)
target_include_directories(AnodeRender PRIVATE ${W})
target_compile_definitions(AnodeRender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_ALSA=0 JUCE_JACK=0
  JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XSHM=0 JucePlugin_Name="Anode" JucePlugin_Build_VST3=0
  JucePlugin_VersionString="1.2.1" JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(AnodeRender PRIVATE AnodeAssets juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
