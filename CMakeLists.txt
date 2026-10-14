cmake_minimum_required(VERSION 3.22)
project(Susurro VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# En Mac: binario universal (Apple Silicon + Intel)
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(SUSURRO_FORMATS AU VST3)
else()
    set(SUSURRO_FORMATS VST3)
endif()

juce_add_plugin(Susurro
    COMPANY_NAME "0600"
    PLUGIN_MANUFACTURER_CODE Ox06
    PLUGIN_CODE Susu
    FORMATS ${SUSURRO_FORMATS}
    PRODUCT_NAME "Susurro"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(Susurro PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Susurro PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Susurro
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- Prueba offline (solo para desarrollo, no se instala) ---
option(SUSURRO_BUILD_TEST "Build offline render test" OFF)
if(SUSURRO_BUILD_TEST)
    juce_add_console_app(SusurroRender PRODUCT_NAME "SusurroRender")
    target_sources(SusurroRender PRIVATE test/RenderTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(SusurroRender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JucePlugin_Name="Susurro" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0 JucePlugin_ProducesMidiOutput=0
        JucePlugin_IsMidiEffect=0 JucePlugin_EditorRequiresKeyboardFocus=0 JucePlugin_ManufacturerCode=0x4f783036 JucePlugin_PluginCode=0x53757375)
    target_link_libraries(SusurroRender PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_audio_formats
        juce::juce_gui_basics PUBLIC juce::juce_recommended_config_flags)
endif()
