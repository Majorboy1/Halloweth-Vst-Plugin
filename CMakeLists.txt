cmake_minimum_required(VERSION 3.22)

project(HALLOWETH VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Link the MSVC runtime statically so the plugin has no VC++ redistributable
# dependency - hosts load it on machines that never installed the redist.
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ------------------------------------------------------------------------------
# JUCE
# ------------------------------------------------------------------------------
# Prefer a local JUCE checkout (git submodule / manual clone) so builds work
# offline and are reproducible. Falls back to a pinned download if it is absent.
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/JUCE/CMakeLists.txt")
    message(STATUS "HALLOWETH: using local JUCE at ThirdParty/JUCE")
    add_subdirectory(ThirdParty/JUCE EXCLUDE_FROM_ALL)
else()
    message(STATUS "HALLOWETH: ThirdParty/JUCE not found - fetching JUCE 8.0.8")
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/juce.git
        GIT_TAG        8.0.8
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# Use sfizz for SFZ instruments without initializing its optional test/tools
# submodules during a normal Halloweth configure.
set(SFIZZ_JACK OFF CACHE BOOL "" FORCE)
set(SFIZZ_RENDER OFF CACHE BOOL "" FORCE)
set(SFIZZ_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(SFIZZ_TESTS OFF CACHE BOOL "" FORCE)
set(SFIZZ_DEMOS OFF CACHE BOOL "" FORCE)
set(SFIZZ_DEVTOOLS OFF CACHE BOOL "" FORCE)
set(SFIZZ_SHARED OFF CACHE BOOL "" FORCE)
set(SFIZZ_GIT_SUBMODULE_CHECK OFF CACHE BOOL "" FORCE)
add_subdirectory(ThirdParty/sfizz EXCLUDE_FROM_ALL)

# ------------------------------------------------------------------------------
# Plugin target
# ------------------------------------------------------------------------------
juce_add_plugin(HALLOWETH
    COMPANY_NAME                "Halloweth Audio"
    BUNDLE_ID                   "com.halloweth.audio.halloweth"
    PLUGIN_MANUFACTURER_CODE    Hwth
    PLUGIN_CODE                 Hwl1
    PRODUCT_NAME                "HALLOWETH"
    DESCRIPTION                 "Cinematic hybrid synth and premium world-instrument platform"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Instrument Synth
    FORMATS                     VST3 Standalone)

target_sources(HALLOWETH PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/AudioEngine/AudioEngine.cpp
    Source/AudioEngine/SfzInstrument.cpp
    Source/AudioEngine/SynthVoice.cpp
    Source/AudioEngine/SampleVoice.cpp
    Source/AudioEngine/SampleLoader.cpp
    Source/Instruments/SampleMap.cpp
    Source/Instruments/DrumKit.cpp
    Source/Instruments/InstrumentLoader.cpp
    Source/Effects/EffectChain.cpp
    Source/Presets/PresetManager.cpp
    Source/Presets/FactoryPresets.cpp
    Source/UI/HallowethLookAndFeel.cpp
    Source/UI/NavigationBar.cpp
    Source/UI/MidiActivityLight.cpp
    Source/UI/PresetBar.cpp
    Source/UI/Knobs/LabeledKnob.cpp
    Source/UI/Pages/HomePage.cpp
    Source/UI/Pages/InstrumentsPage.cpp
    Source/UI/Pages/DrumsPage.cpp
    Source/UI/Pages/EffectsPage.cpp
    Source/UI/Pages/LibraryPage.cpp
    Source/UI/Pages/PlaceholderPage.cpp)

target_include_directories(HALLOWETH PRIVATE Source)

target_compile_definitions(HALLOWETH PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(HALLOWETH
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
        sfizz::static
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# JUCE 8 refuses to compile under MinGW (see juce_core/juce_TargetPlatform.h).
# Fail with an actionable message rather than a wall of compiler errors.
if (MINGW)
    message(FATAL_ERROR
        "HALLOWETH requires MSVC on Windows. JUCE 8 does not support MinGW.\n"
        "Install Visual Studio 2022 Build Tools with the 'Desktop development with C++' "
        "workload, then delete the build directory and reconfigure.")
endif()

if (MSVC)
    # /utf-8 keeps the non-ASCII separators used in UI strings intact.
    target_compile_options(HALLOWETH PRIVATE /MP /permissive- /utf-8)
endif()

add_custom_command(TARGET HALLOWETH_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Resources/GuitarSamples"
        "$<TARGET_FILE_DIR:HALLOWETH_VST3>/../Resources/GuitarSamples"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Licenses"
        "$<TARGET_FILE_DIR:HALLOWETH_VST3>/../Resources/Licenses")

add_custom_command(TARGET HALLOWETH_Standalone POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Resources/GuitarSamples"
        "$<TARGET_FILE_DIR:HALLOWETH_Standalone>/Resources/GuitarSamples"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Licenses"
        "$<TARGET_FILE_DIR:HALLOWETH_Standalone>/Resources/Licenses")

# ------------------------------------------------------------------------------
# Automated tests
# ------------------------------------------------------------------------------
option(HALLOWETH_BUILD_TESTS "Build the HALLOWETH engine test suite" ON)

if (HALLOWETH_BUILD_TESTS)
    enable_testing()

    juce_add_console_app(HALLOWETH_Tests PRODUCT_NAME "HALLOWETH Tests")

    target_sources(HALLOWETH_Tests PRIVATE Tests/AudioEngineTests.cpp)
    target_include_directories(HALLOWETH_Tests PRIVATE Source)

    target_link_libraries(HALLOWETH_Tests
        PRIVATE
            HALLOWETH
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags)

    add_custom_command(TARGET HALLOWETH_Tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/Resources/GuitarSamples"
            "$<TARGET_FILE_DIR:HALLOWETH_Tests>/Resources/GuitarSamples"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Licenses"
            "$<TARGET_FILE_DIR:HALLOWETH_Tests>/Resources/Licenses")

    add_test(NAME AudioEngine COMMAND HALLOWETH_Tests)
endif()
