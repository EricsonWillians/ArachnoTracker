cmake_minimum_required(VERSION 3.15)

set(CMAKE_C_COMPILER /usr/bin/gcc-10)
set(CMAKE_CXX_COMPILER /usr/bin/g++-10)

project(ArachnoTracker VERSION 1.0)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add JUCE
add_subdirectory(juce)

# Add the JUCE include directory
include_directories(${CMAKE_SOURCE_DIR}/juce)
include_directories(${CMAKE_SOURCE_DIR}/src)

# Add the GTK include path
include_directories(/usr/include/gtk-3.0)

# Add the glib include path
include_directories(/usr/include/glib-2.0)
include_directories(/usr/lib/x86_64-linux-gnu/glib-2.0/include/)

# Add the pango include path
include_directories(/usr/include/pango-1.0)

# Add the harfbuzz include path
include_directories(/usr/include/harfbuzz) # Because of this guy it's needed to use G++ 10

# Add the cairo include path
include_directories(/usr/include/cairo)

# Add the gdk-pixbuf include path
include_directories(/usr/include/gdk-pixbuf-2.0)

# Add the webkitgtk-4.0 include path
include_directories(/usr/include/webkitgtk-4.0)

# Add the atk include path
include_directories(/usr/include/atk-1.0)

# Add the libsoup-2.4 include path
include_directories(/usr/include/libsoup-2.4)

# Find and include pybind11
find_package(pybind11 REQUIRED)
include_directories(${pybind11_INCLUDE_DIRS})

# Find and include SDL2
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

# Include directories for other dependencies
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    /usr/local/include          # Assuming fmt and spdlog are installed here
    /mnt/storage/boost/include  # Adjust path if Boost is installed elsewhere
)

# Link directories for dependencies
link_directories(
    /usr/local/lib              # Assuming fmt and spdlog libraries are here
    /mnt/storage/boost/lib      # Adjust path if Boost is installed elsewhere
)

# Add the executable
add_executable(ArachnoTracker
    src/main.cpp
    src/audio/AudioEngine.cpp
    src/audio/Mixer.cpp
    src/audio/Synthesizer.cpp
    src/cli/CLI.cpp
    src/cli/CLIRenderer.cpp
    src/cli/CommandParser.cpp
    src/core/Effect.cpp
    src/core/Instrument.cpp
    src/core/Note.cpp
    src/core/Pattern.cpp
    src/core/PatternRow.cpp
    src/core/PatternStep.cpp
    src/core/Tracker.cpp
    src/ui/GUI.cpp
    src/ui/InstrumentPanel.cpp
    src/ui/PatternEditor.cpp
    src/utils/Config.cpp
    src/utils/FileIO.cpp
    src/utils/Logger.cpp
)

# Link libraries
target_link_libraries(ArachnoTracker
    fmt                       # fmt library
    spdlog                    # spdlog library
    boost_system              # Boost system library
    boost_filesystem          # Boost filesystem library
    pybind11::module          # pybind11 module
    ${SDL2_LIBRARIES}         # SDL2 libraries
    juce::juce_audio_utils    # JUCE audio utilities module
    juce::juce_dsp            # JUCE DSP module
    juce::juce_audio_devices  # JUCE audio devices (includes MIDI)
    curl                      # Add curl to the linked libraries
)

# Ensure spdlog uses the standalone fmt library
target_compile_definitions(ArachnoTracker PRIVATE SPDLOG_FMT_EXTERNAL)

# Additional logging for build type and other relevant info
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}")
message(STATUS "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "CMAKE_CXX_FLAGS_RELEASE: ${CMAKE_CXX_FLAGS_RELEASE}")
