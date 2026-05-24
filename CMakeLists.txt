cmake_minimum_required(VERSION 3.15)

project(ArachnoTracker VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(arachno_core
    src/audio/AudioEngine.cpp
    src/audio/Mixer.cpp
    src/audio/Synthesizer.cpp
    src/core/Note.cpp
    src/core/Pattern.cpp
    src/core/PatternRow.cpp
    src/core/PatternStep.cpp
    src/core/Tracker.cpp
    src/ui/PatternEditor.cpp
    src/utils/FileIO.cpp
    src/utils/ProjectIO.cpp
)

target_include_directories(arachno_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(arachno_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

add_executable(ArachnoTracker
    src/main.cpp
)

target_link_libraries(ArachnoTracker PRIVATE arachno_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(arachno_smoke_tests
        tests/test_audio.cpp
        tests/test_tracker.cpp
    )
    target_link_libraries(arachno_smoke_tests PRIVATE arachno_core)
    add_test(NAME arachno_smoke_tests COMMAND arachno_smoke_tests)
endif()
