cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0"     CACHE STRING "Minimum macOS version")

project(TapeBank VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static MSVC runtime so the .vst3 doesn't need the VC++ redist
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(TAPEBANK_JUCE_PATH "" CACHE PATH "Optional path to a local JUCE checkout")
set(TAPEBANK_JUCE_TAG  "8.0.13" CACHE STRING "JUCE git tag to fetch when no local path is given")
# Copying to C:/Program Files/Common Files/VST3 needs admin on Windows, so OFF by default
option(TAPEBANK_COPY_AFTER_BUILD "Copy the VST3 into the system plug-in folder after building" OFF)

if(TAPEBANK_JUCE_PATH)
    add_subdirectory("${TAPEBANK_JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    set(FETCHCONTENT_QUIET OFF)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${TAPEBANK_JUCE_TAG}
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(TapeBank
    PRODUCT_NAME                "TapeBank"
    COMPANY_NAME                "YourName"
    BUNDLE_ID                   "com.yourname.tapebank"
    PLUGIN_MANUFACTURER_CODE    Ynme
    PLUGIN_CODE                 Tpbk
    FORMATS                     VST3 Standalone
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Instrument Sampler
    COPY_PLUGIN_AFTER_BUILD     ${TAPEBANK_COPY_AFTER_BUILD})

target_sources(TapeBank PRIVATE
    Source/PluginProcessor.cpp   # Step 2, wired in Step 5
    Source/PluginEditor.cpp      # Step 6
    Source/SamplerEngine.cpp     # Step 3
    Source/TapeProcessor.cpp)    # Step 4

target_include_directories(TapeBank PRIVATE Source)

target_compile_definitions(TapeBank PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(TapeBank
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Copy ./Samples into each build so there's a default bank
set(TAPEBANK_SAMPLES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Samples")
if(EXISTS "${TAPEBANK_SAMPLES_DIR}")
    add_custom_command(TARGET TapeBank_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${TAPEBANK_SAMPLES_DIR}"
                "$<TARGET_FILE_DIR:TapeBank_VST3>/../Resources/Samples"
        COMMENT "Copying sample bank into VST3 bundle")
    if(APPLE)
        set(_standalone_samples "$<TARGET_FILE_DIR:TapeBank_Standalone>/../Resources/Samples")
    else()
        set(_standalone_samples "$<TARGET_FILE_DIR:TapeBank_Standalone>/Samples")
    endif()
    add_custom_command(TARGET TapeBank_Standalone POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${TAPEBANK_SAMPLES_DIR}" "${_standalone_samples}"
        COMMENT "Copying sample bank next to Standalone app")
endif()
