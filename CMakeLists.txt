cmake_minimum_required(VERSION 3.15)

project(ArachnoTracker VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(X11 REQUIRED)

add_library(arachno_core
    src/audio/AudioEngine.cpp
    src/audio/Mixer.cpp
    src/audio/AudioRuntime.cpp
    src/audio/RealtimePlayback.cpp
    src/audio/Synthesizer.cpp
    src/core/Instrument.cpp
    src/core/Note.cpp
    src/core/Pattern.cpp
    src/core/PatternRow.cpp
    src/core/PatternStep.cpp
    src/core/StepEffects.cpp
    src/core/Tracker.cpp
    src/ui/AppAsync.cpp
    src/ui/AppActions.cpp
    src/ui/AppEvent.cpp
    src/ui/AppSettings.cpp
    src/ui/AppTask.cpp
    src/ui/ApplicationSession.cpp
    src/ui/EditorActions.cpp
    src/ui/EditorCommandPalette.cpp
    src/ui/EditorShortcuts.cpp
    src/ui/EditorViewModel.cpp
    src/ui/GUI.cpp
    src/ui/gui/GuiInput.cpp
    src/ui/gui/GuiShell.cpp
    src/ui/gui/GuiWindow.cpp
    src/ui/PatternEditor.cpp
    src/ui/PatternView.cpp
    src/ui/ProjectLifecycle.cpp
    src/utils/AutoSave.cpp
    src/utils/ExportWorkflow.cpp
    src/utils/FileIO.cpp
    src/utils/FileCompatibility.cpp
    src/utils/MidiImporter.cpp
    src/utils/MidiExporter.cpp
    src/utils/PatchIO.cpp
    src/utils/ProjectDiagnostics.cpp
    src/utils/ProjectIO.cpp
    src/utils/ScriptIntegration.cpp
)

target_include_directories(arachno_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(arachno_core PUBLIC ${X11_INCLUDE_DIR})

target_link_libraries(arachno_core PUBLIC Threads::Threads ${X11_LIBRARIES})

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

    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(
            NAME arachno_python_sdk_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/python/tests
        )
        set_tests_properties(arachno_python_sdk_tests PROPERTIES
            ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/python"
        )
    endif()
endif()
