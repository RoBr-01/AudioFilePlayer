# List all your source files here
target_sources(${JUCE_OUTPUT_NAME} PRIVATE
    DataStructures.hpp
    PluginEditor.cpp
    PluginEditor.hpp
    PluginProcessor.cpp
    PluginProcessor.hpp
)

target_include_directories(${JUCE_OUTPUT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
