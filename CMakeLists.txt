# This was set up using the JUCE CMAKE template: 
# https://github.com/juce-framework/JUCE/blob/master/examples/CMake/AudioPlugin/CMakeLists.txt

cmake_minimum_required(VERSION 3.21)
project(Template VERSION 0.0.1)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include JUCE subdirectory FIRST (critical for toolchain setup)
set(JUCE_DIR ${CMAKE_SOURCE_DIR}/lib/JUCE)
add_subdirectory(${JUCE_DIR} JUCE_build)

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
    # ICON_BIG ...                              # ICON_* arguments specify a path to an image file to use as an icon for the Standalone version
    # ICON_SMALL ...
    # COMPANY_NAME ...                          # Specify the name of the plugin's author
    # IS_SYNTH TRUE/FALSE                       # Is this a synth or an effect?
    # NEEDS_MIDI_INPUT TRUE/FALSE               # Does the plugin need midi input?
    # NEEDS_MIDI_OUTPUT TRUE/FALSE              # Does the plugin need midi output?
    # IS_MIDI_EFFECT TRUE/FALSE                 # Is this plugin a MIDI effect?
    # EDITOR_WANTS_KEYBOARD_FOCUS TRUE/FALSE    # Does the editor need keyboard focus?
    # COPY_PLUGIN_AFTER_BUILD TRUE/FALSE        # Should the plugin be installed to a default location after building?
    # PLUGIN_MANUFACTURER_CODE                  # A four-character manufacturer id with at least one upper-case character
    # PLUGIN_CODE Rb00                          # A unique four-character plugin id with exactly one upper-case character
                                                # GarageBand 10.3 requires the first letter to be upper-case, and the remaining letters to be lower-case
    FORMATS AU VST3 Standalone                  # The formats to build. Other valid formats are: AAX Unity VST AU AUv3
    PRODUCT_NAME ${JUCE_OUTPUT_NAME})           # The name of the final executable, which can differ from the target name

juce_generate_juce_header(${JUCE_OUTPUT_NAME})

file(GLOB_RECURSE SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")
# add any other sources here, or use the global recursion
target_sources(${JUCE_OUTPUT_NAME}
    PRIVATE
        src/PluginEditor.cpp
        src/PluginProcessor.cpp)

target_compile_definitions(${JUCE_OUTPUT_NAME}
    PUBLIC
        # JUCE_WEB_BROWSER and JUCE_USE_CURL would be on by default, but you might not need them.
        JUCE_WEB_BROWSER=0  # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_plugin` call
        JUCE_USE_CURL=0     # If you remove this, add `NEEDS_CURL TRUE` to the `juce_add_plugin` call
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(${JUCE_OUTPUT_NAME}
    PRIVATE
        # AudioPluginData           # If we'd created a binary data target, we'd link to it here
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)