cmake_minimum_required(VERSION 3.15)

# Set compilers
set(CMAKE_C_COMPILER /usr/bin/gcc-10)
set(CMAKE_CXX_COMPILER /usr/bin/g++-10)

project(ArachnoTracker VERSION 1.0)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add JUCE
add_subdirectory(juce)

# Include JUCE directories
include_directories(${CMAKE_SOURCE_DIR}/juce)
include_directories(${CMAKE_SOURCE_DIR}/src)

# Include GTK and other necessary paths
include_directories(/usr/include/gtk-3.0)
include_directories(/usr/include/glib-2.0)
include_directories(/usr/lib/x86_64-linux-gnu/glib-2.0/include/)
include_directories(/usr/include/pango-1.0)
include_directories(/usr/include/harfbuzz) 
include_directories(/usr/include/cairo)
include_directories(/usr/include/gdk-pixbuf-2.0)
include_directories(/usr/include/webkitgtk-4.0)
include_directories(/usr/include/atk-1.0)
include_directories(/usr/include/libsoup-2.4)

# Find and include pybind11
find_package(pybind11 REQUIRED)
include_directories(${pybind11_INCLUDE_DIRS})

# Find and include SDL2
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

# Find and include Boost
find_package(Boost REQUIRED COMPONENTS system filesystem log log_setup)
include_directories(${Boost_INCLUDE_DIRS})

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
    Boost::system             # Boost system library
    Boost::filesystem         # Boost filesystem library
    Boost::log                # Boost log library
    Boost::log_setup          # Boost log setup library
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
