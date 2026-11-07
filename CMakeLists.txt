cmake_minimum_required(VERSION 3.22)

# macOS : binaire universel (Apple Silicon + Intel), macOS 10.13 minimum (a fixer AVANT project())
if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
endif()

project(Anode VERSION 1.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Windows : runtime C++ statique (pas de redistribuable Visual C++ a installer)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# JUCE : dossier local si JUCE_DIR est fourni, sinon telechargement automatique
set(JUCE_DIR "" CACHE PATH "Copie locale de JUCE (optionnel)")
if(JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
  add_subdirectory(${JUCE_DIR} JUCE)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW ON)
  FetchContent_MakeAvailable(JUCE)
endif()

# visuels integres au plugin (aucun fichier externe a l'execution)
juce_add_binary_data(AnodeAssets SOURCES
  background.jpg knob_strip.png selector_strip.png toggle_strip.png
  tubezone_6L6.jpg tubezone_EL34.jpg tubezone_EL84.jpg tubezone_KT88.jpg
  glow_6L6.png glow_EL34.png glow_EL84.png glow_KT88.png
  redplate_6L6.png redplate_EL34.png redplate_EL84.png redplate_KT88.png
  Poppins-Medium.ttf)
set_target_properties(AnodeAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

juce_add_plugin(Anode
  COMPANY_NAME "NINE HANDS | DSP"
  PLUGIN_MANUFACTURER_CODE NhDs
  PLUGIN_CODE NhAn
  BUNDLE_ID com.ninehandsdsp.anode
  FORMATS VST3
  VST3_CATEGORIES Fx Distortion
  PRODUCT_NAME "Anode"
  VERSION 1.2.0
  DESCRIPTION "Tube power stage used as a guitar / bass preamp (6L6, EL34, EL84, KT88)"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Anode PRIVATE PluginProcessor.cpp PluginEditor.cpp)

if(MSVC)
  target_compile_options(Anode PRIVATE /utf-8)
endif()

target_compile_definitions(Anode PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Anode
  PRIVATE
    AnodeAssets
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
