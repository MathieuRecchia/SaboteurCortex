cmake_minimum_required(VERSION 3.20)
project(SaboteurCortex VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.1
)
FetchContent_MakeAvailable(juce)

juce_add_plugin(SaboteurCortex
    COMPANY_NAME "Saboteur"
    PLUGIN_MANUFACTURER_CODE "SbtR"
    PLUGIN_CODE "SbCx"
    FORMATS VST3 Standalone
    PRODUCT_NAME "Saboteur Cortex"
    NEEDS_JUCE_HEADER TRUE
)

target_sources(SaboteurCortex PRIVATE
    Source/SaboteurCortexProcessor.cpp
    Source/SaboteurCortexEditor.cpp
)

target_compile_features(SaboteurCortex PRIVATE cxx_std_20)
target_compile_definitions(SaboteurCortex PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
)

target_link_libraries(SaboteurCortex PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
)