# This was set up using the JUCE CMAKE template: 
# https://github.com/juce-framework/JUCE/blob/master/examples/CMake/AudioPlugin/CMakeLists.txt

cmake_minimum_required(VERSION 3.25...4.3.2)

if(APPLE)
    message(STATUS "enabling FAT executable and minimum OS version")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum OS X deployment version")
    message(STATUS "CMAKE_OSX_DEPLOYMENT_TARGET: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()

project(AudioFilePlayer VERSION 0.1.0)


# Compiler ID
message(STATUS "Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Compiler Version: ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Compiler Path: ${CMAKE_CXX_COMPILER}")

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lib/JUCE)

include_directories(${CMAKE_SOURCE_DIR}/inc)

# Set output directories AFTER JUCE inclusion
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Makes it easier to distinguish the two
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(JUCE_OUTPUT_NAME "${PROJECT_NAME}_debug")
else()
    set(JUCE_OUTPUT_NAME "${PROJECT_NAME}")
endif()

juce_add_plugin(${JUCE_OUTPUT_NAME}
    # VERSION ...                               # Set this if the plugin version is different to the project version
    ICON_BIG ${CMAKE_SOURCE_DIR}/res/img/icon_512.png # ICON_* arguments specify a path to an image file to use as an icon for the Standalone version
    # ICON_SMALL ...
    COMPANY_NAME RoBr # Specify the name of the plugin's author
    IS_SYNTH FALSE # Is this a synth or an effect?
    NEEDS_MIDI_INPUT FALSE # Does the plugin need midi input?
    NEEDS_MIDI_OUTPUT FALSE # Does the plugin need midi output?
    IS_MIDI_EFFECT FALSE # Is this plugin a MIDI effect?
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE # Does the editor need keyboard focus?
    COPY_PLUGIN_AFTER_BUILD TRUE # Should the plugin be installed to a default location after building?
    PLUGIN_MANUFACTURER_CODE RoBr # A four-character manufacturer id with at least one upper-case character
    PLUGIN_CODE RB01 # A unique four-character plugin id with exactly one upper-case character
    # GarageBand 10.3 requires the first letter to be upper-case, and the remaining letters to be lower-case
    FORMATS AU VST3 Standalone # The formats to build. Other valid formats are: AAX Unity VST AU AUv3
    PRODUCT_NAME ${JUCE_OUTPUT_NAME}) # The name of the final executable, which can differ from the target name

juce_generate_juce_header(${JUCE_OUTPUT_NAME})

add_subdirectory(src)

target_compile_definitions(${JUCE_OUTPUT_NAME}
    PUBLIC
    # JUCE_WEB_BROWSER and JUCE_USE_CURL would be on by default, but you might not need them.
    JUCE_WEB_BROWSER=0 # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_plugin` call
    JUCE_USE_CURL=0 # If you remove this, add `NEEDS_CURL TRUE` to the `juce_add_plugin` call
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(${JUCE_OUTPUT_NAME}
    PRIVATE
    # AudioPluginData           # If we'd created a binary data target, we'd link to it here
    juce::juce_audio_utils
    # juce::juce_gui_basics
    # juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    # juce::juce_recommended_warning_flags
)
