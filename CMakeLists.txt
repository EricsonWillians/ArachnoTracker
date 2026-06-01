cmake_minimum_required(VERSION 3.15)

project(ArachnoTracker VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(X11 REQUIRED)
find_package(ALSA)

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
    src/ui/gui/GuiAudioRuntime.cpp
    src/ui/gui/GuiInstrumentBrowserOps.cpp
    src/ui/gui/GuiGridOps.cpp
    src/ui/gui/GuiGridWindowOps.cpp
    src/ui/gui/GuiMainGridClickOps.cpp
    src/ui/gui/GuiMainDrawPrepOps.cpp
    src/ui/gui/GuiMainGridDrawOps.cpp
    src/ui/gui/GuiMainWindowContentDrawOps.cpp
    src/ui/gui/GuiMainWindowRenderOps.cpp
    src/ui/gui/GuiMainButtonPressDispatchOps.cpp
    src/ui/gui/GuiMainButtonPressContextFactoryOps.cpp
    src/ui/gui/GuiMainEventContextFactoryOps.cpp
    src/ui/gui/GuiMainEventPumpOps.cpp
    src/ui/gui/GuiMainEventScaffoldDispatchOps.cpp
    src/ui/gui/GuiMainWindowEventAdapterOps.cpp
    src/ui/gui/GuiMainWindowInteractionAdapterOps.cpp
    src/ui/gui/GuiMainXEventDispatchWrappersOps.cpp
    src/ui/gui/GuiMainWindowEventDispatchOps.cpp
    src/ui/gui/GuiMainTopPanelDrawOps.cpp
    src/ui/gui/GuiMainKeyCommandOps.cpp
    src/ui/gui/GuiMainKeyCommandContextFactoryOps.cpp
    src/ui/gui/GuiMainKeyEditOps.cpp
    src/ui/gui/GuiMainKeyEditContextFactoryOps.cpp
    src/ui/gui/GuiMainKeyPressDispatchOps.cpp
    src/ui/gui/GuiMainLoopTickOps.cpp
    src/ui/gui/GuiMainRunLoopOps.cpp
    src/ui/gui/GuiMainKeyModalContextFactoryOps.cpp
    src/ui/gui/GuiMainKeyNoteContextFactoryOps.cpp
    src/ui/gui/GuiMainKeyNoteOps.cpp
    src/ui/gui/GuiMainModalDrawOps.cpp
    src/ui/gui/GuiMainSidebarDrawOps.cpp
    src/ui/gui/GuiMainKeyModalOps.cpp
    src/ui/gui/GuiMainModalMouseOps.cpp
    src/ui/gui/GuiMainMotionOps.cpp
    src/ui/gui/GuiMainPointerContextFactoryOps.cpp
    src/ui/gui/GuiMainPrimaryContextFactoryOps.cpp
    src/ui/gui/GuiMainPrimaryClickOps.cpp
    src/ui/gui/GuiMainRealtimeAudioOps.cpp
    src/ui/gui/GuiMainSidebarContextFactoryOps.cpp
    src/ui/gui/GuiMainWheelOps.cpp
    src/ui/gui/GuiMidiInput.cpp
    src/ui/gui/GuiSynthInputOps.cpp
    src/ui/gui/GuiThemePaletteOps.cpp
    src/ui/gui/GuiPatchApplyData.cpp
    src/ui/gui/GuiPatternPromptOps.cpp
    src/ui/gui/GuiSynthPatchActionOps.cpp
    src/ui/gui/GuiPathDefaults.cpp
    src/ui/gui/GuiArrangementOps.cpp
    src/ui/gui/GuiWindowArrangementAdapterOps.cpp
    src/ui/gui/GuiWindowInstrumentSynthOps.cpp
    src/ui/gui/GuiWindowRenderAdapterOps.cpp
    src/ui/gui/GuiWindowRunLoopAdapterOps.cpp
    src/ui/gui/GuiArrangementWindowOps.cpp
    src/ui/gui/GuiFileButtonOps.cpp
    src/ui/gui/GuiSynthInteractionOps.cpp
    src/ui/gui/GuiSynthKeyboardSection.cpp
    src/ui/gui/GuiSynthComposerOps.cpp
    src/ui/gui/GuiSynthInlinePromptEvents.cpp
    src/ui/gui/GuiSynthInlinePromptSection.cpp
    src/ui/gui/GuiSynthEventContextFactoryOps.cpp
    src/ui/gui/GuiSynthAuditionWindowOps.cpp
    src/ui/gui/GuiMainSidebarClickOps.cpp
    src/ui/gui/GuiSynthWindowInteractionAdapterOps.cpp
    src/ui/gui/GuiSynthKeyOps.cpp
    src/ui/gui/GuiSynthMotionOps.cpp
    src/ui/gui/GuiSynthWindowEventOps.cpp
    src/ui/gui/GuiSynthParamSection.cpp
    src/ui/gui/GuiSynthScopeSection.cpp
    src/ui/gui/GuiSynthTopSectionOps.cpp
    src/ui/gui/GuiSessionWindowOps.cpp
    src/ui/gui/GuiSynthWindowDrawOps.cpp
    src/ui/gui/GuiSynthWindowRenderOps.cpp
    src/ui/gui/GuiSynthWindowLifecycleAdapterOps.cpp
    src/ui/gui/GuiSynthWindowLifecycleOps.cpp
    src/ui/gui/GuiSynthPreviewOps.cpp
    src/ui/gui/GuiTrackMetadataOps.cpp
    src/ui/gui/GuiTemporalPaste.cpp
    src/ui/gui/GuiUnsavedPromptOps.cpp
    src/ui/gui/GuiFileBrowserOps.cpp
    src/ui/gui/GuiInlinePromptOps.cpp
    src/ui/gui/GuiWindowShutdownOps.cpp
    src/ui/gui/GuiWindowBootstrapOps.cpp
    src/ui/gui/GuiWindowCoreBindingsOps.cpp
    src/ui/gui/GuiWindowCoreStateOps.cpp
    src/ui/gui/GuiWindowEditingBindingsOps.cpp
    src/ui/gui/GuiWindowMainInteractionFactoryOps.cpp
    src/ui/gui/GuiWindowPromptFlowOps.cpp
    src/ui/gui/GuiWindowSessionOps.cpp
    src/ui/gui/GuiWindowSynthInteractionFactoryOps.cpp
    src/ui/gui/GuiWindowSynthLifecycleBindingsOps.cpp
    src/ui/gui/GuiEditorWindowStateOps.cpp
    src/ui/gui/GuiShell.cpp
    src/ui/gui/SynthUiData.cpp
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
if(ALSA_FOUND)
    if(DEFINED ALSA_INCLUDE_DIRS)
        target_include_directories(arachno_core PUBLIC ${ALSA_INCLUDE_DIRS})
    elseif(DEFINED ALSA_INCLUDE_DIR)
        target_include_directories(arachno_core PUBLIC ${ALSA_INCLUDE_DIR})
    endif()
endif()

target_link_libraries(arachno_core PUBLIC Threads::Threads ${X11_LIBRARIES})
if(ALSA_FOUND)
    target_compile_definitions(arachno_core PRIVATE ARACHNO_HAS_ALSA=1)
    if(TARGET ALSA::ALSA)
        target_link_libraries(arachno_core PUBLIC ALSA::ALSA)
    elseif(DEFINED ALSA_LIBRARIES)
        target_link_libraries(arachno_core PUBLIC ${ALSA_LIBRARIES})
    elseif(DEFINED ALSA_LIBRARY)
        target_link_libraries(arachno_core PUBLIC ${ALSA_LIBRARY})
    endif()
else()
    target_compile_definitions(arachno_core PRIVATE ARACHNO_HAS_ALSA=0)
endif()

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
