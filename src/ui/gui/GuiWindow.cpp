#include "GUI.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <functional>
#include <limits>
#include <map>
#include <stdexcept>
#include <thread>
#include <vector>
#include <utility>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>

#include "AppActions.h"
#include "ui/gui/GuiAudioRuntime.h"
#include "ui/gui/GuiEditorWindowStateOps.h"
#include "ui/gui/GuiInlinePromptOps.h"
#include "ui/gui/GuiMainRealtimeAudioOps.h"
#include "ui/gui/GuiMidiInput.h"
#include "ui/gui/GuiSynthParamSection.h"
#include "ui/gui/GuiSynthPreviewOps.h"
#include "ui/gui/GuiSynthWindowEventOps.h"
#include "ui/gui/GuiWindowShutdownOps.h"
#include "ui/gui/GuiWindowBootstrapOps.h"
#include "ui/gui/GuiWindowCoreBindingsOps.h"
#include "ui/gui/GuiWindowCoreStateOps.h"
#include "ui/gui/GuiWindowEditingBindingsOps.h"
#include "ui/gui/GuiWindowInstrumentSynthOps.h"
#include "ui/gui/GuiWindowMainInteractionFactoryOps.h"
#include "ui/gui/GuiWindowPromptFlowOps.h"
#include "ui/gui/GuiWindowRenderAdapterOps.h"
#include "ui/gui/GuiWindowRunLoopAdapterOps.h"
#include "ui/gui/GuiWindowSessionOps.h"
#include "ui/gui/GuiWindowSynthInteractionFactoryOps.h"
#include "ui/gui/GuiWindowSynthLifecycleBindingsOps.h"
#include "ui/gui/SynthUiData.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

int runGuiWindow(
    ApplicationSession& session,
    std::ostream& output,
    const GuiShellOptions& options) {
    auto bootstrapResult = bootstrapGuiWindow(output, "ArachnoTracker");
    if (!bootstrapResult.has_value()) {
        return 2;
    }
    GuiWindowBootstrapResult bootstrap = *bootstrapResult;
    Display* display = bootstrap.display;
    const int screen = bootstrap.screen;
    const Window window = bootstrap.window;
    GC gc = bootstrap.gc;
    Atom wmDelete = bootstrap.wmDelete;
    XFontStruct* uiFont = bootstrap.uiFont;
    const UiThemePalette dosTheme = bootstrap.dosTheme;
    const UiThemePalette highContrastTheme = bootstrap.highContrastTheme;
    GuiThemeMode themeMode = bootstrap.themeMode;
    const GuiSynthWindowScopeColors scopeColors = bootstrap.scopeColors;

    AppActionResult lastAction;
    AppActionResult snapshotResult;
    int windowWidth = 1280;
    int windowHeight = 800;
    Pixmap trackerBackbuffer = 0;
    int trackerBackbufferWidth = 0;
    int trackerBackbufferHeight = 0;
    int armedInstrument = 0;
    int armedOctave = 4;
    float defaultVelocity = 0.80f;
    bool stepAdvance = true;
    bool followPlayback = true;
    int viewStartRow = 0;
    int requestedRowCount = std::max(1, options.snapshotGridRowCount);
    int topPanelHeightState = 210;
    int sidebarWidthState = 300;
    int activePatternRows = 64;
    int selectedOrderIndex = 0;
    TrackerWindowLayout layout;
    bool draggingSelection = false;
    bool resizingSidebar = false;
    bool resizingTopPanel = false;
    bool draggingPatternRows = false;
    int dragAnchorRow = 0;
    int dragAnchorTrack = 0;
    int patternResizeAnchorY = 0;
    int patternResizeStartRows = 64;
    auto lastPlaybackTick = std::chrono::steady_clock::now();

    std::vector<std::pair<UiRect, std::string>> transportButtons;
    std::vector<std::pair<UiRect, int>> instrumentHitTargets;
    std::vector<std::pair<UiRect, std::string>> instrumentControlHits;
    std::vector<std::pair<UiRect, int>> instrumentBrowserHitTargets;
    std::vector<TrackHeaderHit> trackHeaderHits;
    std::vector<OrderSlotHit> orderSlotHits;
    std::vector<std::pair<UiRect, std::string>> fileButtons;
    std::vector<std::pair<UiRect, GuiThemeMode>> themeButtons;
    std::vector<std::pair<UiRect, AudioPerformanceMode>> audioPerformanceButtons;
    std::vector<AudioTuningDialogHit> audioTuningDialogHits;
    std::vector<std::pair<UiRect, int>> octaveHitTargets;
    std::vector<PianoKeyHit> pianoKeyHits;
    std::vector<TrackMetadataHit> trackMetadataHits;
    std::vector<SongLengthHit> songLengthHits;
    std::vector<MidiImportSettingHit> midiImportSettingHits;
    std::vector<SynthWindowHit> synthWindowHits;
    UiRect patternRowsMinus;
    UiRect patternRowsPlus;
    UiRect patternRowsValue;
    UiRect gridTrackPrevButton;
    UiRect gridTrackNextButton;
    UiRect patternPrevButton;
    UiRect patternNextButton;
    UiRect patternValueButton;
    UiRect patternNewButton;
    UiRect patternCloneButton;
    UiRect patternDeleteButton;
    UiRect orderPrevButton;
    UiRect orderNextButton;
    UiRect orderValueButton;
    UiRect orderInsertButton;
    UiRect orderAppendButton;
    UiRect orderDeleteButton;
    UiRect stepAdvanceButton;
    UiRect followPlaybackButton;
    UiRect instrumentListRect;
    UiRect instrumentBrowserListRect;
    UiRect instrumentBrowserAcceptButton;
    UiRect instrumentBrowserCancelButton;
    UiRect sidebarViewport;
    bool paintingNotes = false;
    int paintNoteMidi = 60;
    int lastPaintRow = -1;
    int lastPaintTrack = -1;
    bool keyboardSelectionActive = false;
    int keyboardSelectionAnchorRow = 0;
    int keyboardSelectionAnchorTrack = 0;
    auto manualScrollLockUntil = std::chrono::steady_clock::time_point {};
    GuiMidiInput midiInput;
    GuiAudioRuntime audioRuntime;
    bool previousAudioStreamActive = false;
    std::vector<float> audioLeft;
    std::vector<float> audioRight;
    double audioPendingFrames = 0.0;
    bool audioTuningDialogActive = false;
    int instrumentListStart = 0;
    int instrumentListVisibleRows = 8;
    int sidebarScrollOffset = 0;
    int sidebarContentHeight = 0;
    int gridTrackStart = 0;
    int lastCursorTrackForGridFollow = -1;
    int pointerX = 0;
    int pointerY = 0;
    int hoveredTrackHeader = -1;
    bool instrumentBrowserActive = false;
    std::string instrumentBrowserQuery;
    int instrumentBrowserSelected = 0;
    int instrumentBrowserScroll = 0;
    InlinePromptState inlinePrompt;
    UiRect inlinePromptAcceptButton;
    UiRect inlinePromptCancelButton;
    bool inlinePromptButtonsVisible = false;
    std::filesystem::path fileBrowserDirectory;
    std::vector<FileBrowserEntry> fileBrowserEntries;
    std::vector<FileBrowserHit> fileBrowserHits;
    UiRect fileBrowserListRect;
    UiRect audioTuningDialogRect;
    int fileBrowserScroll = 0;
    int fileBrowserSelected = -1;
    UiRect synthInlinePromptAcceptButton;
    UiRect synthInlinePromptCancelButton;
    bool synthInlinePromptButtonsVisible = false;
    UnsavedDecisionPromptState unsavedPrompt;
    std::vector<std::pair<UiRect, UnsavedChangesChoice>> unsavedPromptChoices;
    bool hasDeferredPostSaveAction = false;
    AppActionRequest deferredPostSaveAction;
    double targetSongLengthMinutes = 3.5;
    int midiImportRowsPerBeat = 4;
    int midiImportPatternRows = 64;
    bool midiImportSplitByTrack = true;
    bool synthWindowVisible = false;
    Window synthWindow = 0;
    GC synthGc = nullptr;
    Pixmap synthBackbuffer = 0;
    int synthBackbufferWidth = 0;
    int synthBackbufferHeight = 0;
    int synthWindowWidth = 980;
    int synthWindowHeight = 900;
    const int synthWindowMinWidth = 900;
    const int synthWindowMinHeight = 760;
    bool synthWindowNeedsRedraw = false;
    int synthPreviewMidi = 60;
    std::array<bool, 256> synthPreviewKeyHeld {};
    std::array<int, 256> synthPreviewKeyMidi {};
    std::array<bool, 128> synthMidiPreviewHeld {};
    std::vector<PianoKeyHit> synthKeyboardHits;
    bool synthPointerDown = false;
    int synthLastPointerMidi = -1;
    int synthKeyboardBaseOctave = 2;
    int synthKeyboardVisibleOctaves = 4;
    int synthParamPage = 0;
    int synthParamOscTarget = -1;
    int synthParamScroll = 0;
    UiRect synthParamViewport;
    int synthParamContentHeight = 0;
    bool synthParamDragActive = false;
    bool synthParamDragKnob = false;
    std::string synthParamDragName;
    UiRect synthParamDragRect;
    int synthParamDragStartX = 0;
    int synthParamDragStartY = 0;
    double synthParamDragStartValue = 0.0;
    double synthParamDragLastValue = std::numeric_limits<double>::quiet_NaN();
    bool synthParamDragDirty = false;
    std::array<double, 4> synthScopePhase {0.0, 0.0, 0.0, 0.0};
    int synthScopeTrackedMidi = 60;
    static constexpr int synthScopeTraceSamples = 192;
    std::array<Synthesizer, 4> synthScopeLaneSynth {
        Synthesizer(static_cast<double>(std::max(8000, session.song().sampleRate))),
        Synthesizer(static_cast<double>(std::max(8000, session.song().sampleRate))),
        Synthesizer(static_cast<double>(std::max(8000, session.song().sampleRate))),
        Synthesizer(static_cast<double>(std::max(8000, session.song().sampleRate)))};
    std::array<std::array<float, synthScopeTraceSamples>, 4> synthScopeLaneLeft {};
    std::array<std::array<float, synthScopeTraceSamples>, 4> synthScopeLaneRight {};
    bool synthScopeLaneActive = false;
    int synthScopeLaneInstrument = -1;
    int synthScopeLaneMidi = -1;
    double synthScopeLaneGateSeconds = 0.25;
    bool synthScopeRetriggerRequested = true;
    int synthPointerX = 0;
    int synthPointerY = 0;
    std::string synthTooltipParam;
    auto synthTooltipHoverSince = std::chrono::steady_clock::time_point {};
    std::function<bool(InlinePromptKind)> inlinePromptUsesFileBrowser;
    std::function<void()> refreshFileBrowserEntries;
    std::function<void()> initFileBrowserFromPrompt;
    std::function<bool(const std::string&)> executeTemporalPasteSpec;
    std::function<std::vector<int>(const AppSessionSnapshot&)> filteredInstrumentIndices;

    GuiWindowCoreStateContext coreStateContext {
        session,
        options,
        snapshotResult,
        lastAction,
        viewStartRow,
        requestedRowCount,
        followPlayback,
        manualScrollLockUntil,
        selectedOrderIndex,
        gridTrackStart,
        layout,
        lastCursorTrackForGridFollow,
        activePatternRows,
        audioRuntime,
        [&]() {
            const Song& song = session.song();
            bool soloActive = false;
            for (const Track& track : song.tracks) {
                if (track.solo) {
                    soloActive = true;
                    break;
                }
            }
            int count = 0;
            for (const Track& track : song.tracks) {
                if (track.muted) {
                    continue;
                }
                if (soloActive && !track.solo) {
                    continue;
                }
                ++count;
            }
            return std::max(1, count);
        }};

    GuiWindowCoreBindings coreBindings = makeWindowCoreBindingsFromState(
        GuiWindowCoreBindingsInput {
            coreStateContext,
            audioRuntime,
            display,
            window,
            screen,
            windowWidth,
            windowHeight,
            trackerBackbuffer,
            trackerBackbufferWidth,
            trackerBackbufferHeight,
            inlinePrompt,
            inlinePromptUsesFileBrowser,
            initFileBrowserFromPrompt,
            fileBrowserDirectory,
            fileBrowserEntries,
            fileBrowserHits,
            fileBrowserScroll,
            fileBrowserSelected,
            inlinePromptButtonsVisible,
            synthInlinePromptButtonsVisible});

    inlinePromptUsesFileBrowser = [&](InlinePromptKind kind) {
        return inlinePromptKindUsesFileBrowser(kind);
    };

    std::function<void(int)> selectInstrument;
    std::function<bool(int, const SynthPatch&, bool)> applyPatchToInstrument;

    GuiWindowPromptFlowContext promptFlowContext = makePromptFlowContextFromWindowState(
        GuiWindowPromptFlowFactoryInput {
            session,
            inlinePrompt,
            fileBrowserDirectory,
            fileBrowserEntries,
            fileBrowserHits,
            fileBrowserScroll,
            fileBrowserSelected,
            inlinePromptUsesFileBrowser,
            initFileBrowserFromPrompt,
            inlinePromptButtonsVisible,
            synthInlinePromptButtonsVisible,
            hasDeferredPostSaveAction,
            deferredPostSaveAction,
            unsavedPrompt,
            unsavedPromptChoices,
            midiImportRowsPerBeat,
            midiImportPatternRows,
            midiImportSplitByTrack,
            lastAction,
            synthWindowNeedsRedraw,
            coreBindings.refreshSnapshot,
            targetSongLengthMinutes,
            activePatternRows,
            keyboardSelectionActive,
            viewStartRow,
            coreBindings.clearInlinePrompt,
            [&](const AppActionRequest& request) { return coreBindings.runAction(request, true); },
            [&](int instrument) { selectInstrument(instrument); },
            [&](int instrument, const SynthPatch& patch, bool fromLivePreview) {
                return applyPatchToInstrument(instrument, patch, fromLivePreview);
            },
            coreBindings.activeSnapshot,
            [&](const std::string& spec) { return executeTemporalPasteSpec(spec); },
            [&](InlinePromptKind kind,
                const std::string& title,
                const std::string& hint,
                const std::string& initialValue,
                int targetTrack,
                int targetInstrument) {
                coreBindings.beginInlinePrompt(kind, title, hint, initialValue, targetTrack, targetInstrument);
            }});

    std::function<void()> cancelInlinePrompt;
    std::function<void()> executeInlinePrompt;
    std::function<void(UnsavedChangesChoice)> resolveUnsavedPrompt;
    wirePromptFlowCallbacksFromWindowState(
        promptFlowContext,
        refreshFileBrowserEntries,
        initFileBrowserFromPrompt,
        cancelInlinePrompt,
        executeInlinePrompt,
        resolveUnsavedPrompt);

    GuiWindowSynthLifecycleBindings synthLifecycleBindings = makeSynthLifecycleBindingsFromWindowState(
        GuiWindowSynthLifecycleBindingsInput {
            display,
            screen,
            wmDelete,
            uiFont,
            synthWindow,
            synthGc,
            synthBackbuffer,
            synthBackbufferWidth,
            synthBackbufferHeight,
            synthWindowWidth,
            synthWindowHeight,
            synthWindowVisible,
            synthPreviewKeyHeld,
            synthPreviewKeyMidi,
            synthMidiPreviewHeld,
            synthPointerDown,
            synthLastPointerMidi,
            synthTooltipParam,
            synthTooltipHoverSince,
            synthScopeRetriggerRequested,
            synthKeyboardBaseOctave,
            synthKeyboardVisibleOctaves,
            synthPreviewMidi,
            paintNoteMidi,
            armedOctave,
            synthWindowNeedsRedraw,
            synthScopeLaneActive,
            synthScopeLaneSynth,
            lastAction,
            audioTuningDialogActive,
            coreBindings.activeSnapshot,
            [&](const AppActionRequest& request) { promptFlowContext.runLifecycleAction(request); },
            [&](const AppActionRequest& request) { return coreBindings.runAction(request, true); },
            [&](InlinePromptKind kind,
                const std::string& title,
                const std::string& hint,
                const std::string& initialValue,
                int targetTrack,
                int targetInstrument) {
                    coreBindings.beginInlinePrompt(kind, title, hint, initialValue, targetTrack, targetInstrument);
                }});
    auto releaseSynthBackbuffer = synthLifecycleBindings.releaseSynthBackbuffer;
    auto ensureSynthBackbuffer = synthLifecycleBindings.ensureSynthBackbuffer;
    auto claimSynthPreviewKey = synthLifecycleBindings.claimSynthPreviewKey;
    auto releaseSynthPreviewKey = synthLifecycleBindings.releaseSynthPreviewKey;
    auto setSynthWindowVisible = synthLifecycleBindings.setSynthWindowVisible;
    auto runFileButtonAction = synthLifecycleBindings.runFileButtonAction;

    GuiEditorWindowStateBindings editorStateBindings = makeEditorWindowStateBindingsFromWindowState(
        GuiEditorWindowStateBindingsInput {
            activePatternRows,
            armedOctave,
            paintNoteMidi,
            synthKeyboardBaseOctave,
            synthKeyboardVisibleOctaves,
            lastAction,
            [&](const AppActionRequest& request) { return coreBindings.runAction(request, true); }});

    GuiWindowEditingBindings editingBindings = makeWindowEditingBindingsFromState(
        GuiWindowEditingBindingsContext {
            selectedOrderIndex,
            keyboardSelectionActive,
            viewStartRow,
            activePatternRows,
            layout,
            gridTrackStart,
            armedInstrument,
            defaultVelocity,
            coreBindings.activeSnapshot,
            [&](const AppActionRequest& request) { return coreBindings.runAction(request, true); },
            coreBindings.runAction,
            [&](InlinePromptKind kind,
                const std::string& title,
                const std::string& hint,
                const std::string& initialValue,
                int targetTrack,
                int targetInstrument) {
                    coreBindings.beginInlinePrompt(kind, title, hint, initialValue, targetTrack, targetInstrument);
                },
            arachno::normalizedPatternNameToken,
            editorStateBindings.ensurePatternRowsForRow,
            editorStateBindings.applyLastActionState});

    executeTemporalPasteSpec = editingBindings.executeTemporalPasteSpec;

    GuiWindowInstrumentSynthContext instrumentSynthContext {
        session,
        armedInstrument,
        instrumentListStart,
        instrumentListVisibleRows,
        synthScopeRetriggerRequested,
        synthPreviewMidi,
        paintNoteMidi,
        defaultVelocity,
        synthWindowNeedsRedraw,
        lastAction,
        instrumentBrowserQuery,
        audioTuningDialogActive,
        instrumentBrowserActive,
        instrumentBrowserScroll,
        instrumentBrowserSelected,
        instrumentBrowserHitTargets,
        instrumentBrowserListRect,
        instrumentBrowserAcceptButton,
        instrumentBrowserCancelButton,
            coreBindings.activeSnapshot,
            [&](const AppActionRequest& request) { return coreBindings.runAction(request, true); },
            [&](const AppActionRequest& request, bool refreshRequested) {
                return coreBindings.runAction(request, refreshRequested);
            },
            coreBindings.refreshSnapshot,
            editorStateBindings.ensureSynthKeyboardShowsMidi,
            editorStateBindings.applyActionResultStatus};

    GuiWindowInstrumentSynthBindings instrumentSynthBindings =
        makeInstrumentSynthBindingsFromWindowState(instrumentSynthContext);

    selectInstrument = instrumentSynthBindings.selectInstrument;
    filteredInstrumentIndices = instrumentSynthBindings.filteredInstrumentIndices;
    auto clampInstrumentListWindow = instrumentSynthBindings.clampInstrumentListWindow;

    auto collectSynthPreviewNotes = [&]() {
        return arachno::collectSynthPreviewNotes(
            synthPreviewKeyHeld,
            synthPreviewKeyMidi,
            synthPointerDown,
            synthLastPointerMidi,
            synthMidiPreviewHeld);
    };

    const std::vector<SynthParamDef>& synthParamDefs = synthParamDefinitions();

    applyPatchToInstrument = instrumentSynthBindings.applyPatchToInstrument;

    startupGuiWindowSession(
        GuiWindowStartupContext {
            coreBindings.runSync,
            coreBindings.refreshSnapshot,
            coreBindings.runAction,
            coreBindings.ensureTrackerBackbuffer,
            midiInput});

    bool running = true;
    bool needsRedraw = true;
    auto lastRefresh = std::chrono::steady_clock::now();

    std::function<void()> draw;
    std::function<void()> drawSynthWindow;

    auto isAutoRepeatRelease = [&](const XEvent& event) {
        return isX11AutoRepeatRelease(display, event);
    };

    GuiSynthWindowInteractionAdapterContext synthInteractionContext =
        makeSynthInteractionContextFromWindowState(
            GuiWindowSynthInteractionFactoryInput {
                session,
                display,
                synthWindow,
                wmDelete,
                synthWindowWidth,
                synthWindowHeight,
                synthWindowMinWidth,
                synthWindowMinHeight,
                synthLifecycleBindings.ensureSynthBackbuffer,
                [&](bool visible) { synthLifecycleBindings.setSynthWindowVisible(visible); },
                synthWindowVisible,
                synthWindowNeedsRedraw,
                needsRedraw,
                synthPointerX,
                synthPointerY,
                synthPointerDown,
                synthLastPointerMidi,
                synthParamDragActive,
                synthParamDragKnob,
                synthParamDragDirty,
                synthParamDragName,
                synthParamDragRect,
                synthParamDragStartX,
                synthParamDragStartY,
                synthParamDragStartValue,
                synthParamDragLastValue,
                synthTooltipParam,
                synthTooltipHoverSince,
                synthParamViewport,
                synthParamContentHeight,
                synthParamScroll,
                synthKeyboardBaseOctave,
                synthKeyboardVisibleOctaves,
                synthParamPage,
                synthParamOscTarget,
                armedOctave,
                armedInstrument,
                synthPreviewMidi,
                paintNoteMidi,
                stepAdvance,
                defaultVelocity,
                synthWindowHits,
                synthKeyboardHits,
                inlinePrompt,
                coreBindings.isSynthInlinePromptKind,
                inlinePromptUsesFileBrowser,
                synthInlinePromptButtonsVisible,
                synthInlinePromptAcceptButton,
                synthInlinePromptCancelButton,
                fileBrowserHits,
                fileBrowserListRect,
                fileBrowserScroll,
                fileBrowserSelected,
                fileBrowserEntries,
                fileBrowserDirectory,
                refreshFileBrowserEntries,
                executeInlinePrompt,
                cancelInlinePrompt,
                editorStateBindings.ensureSynthKeyboardShowsMidi,
                instrumentSynthBindings.cycleInstrumentBy,
                editorStateBindings.setArmedOctave,
                synthLifecycleBindings.claimSynthPreviewKey,
                synthLifecycleBindings.releaseSynthPreviewKey,
                instrumentSynthBindings.auditionSynthPreviewMidi,
                instrumentSynthBindings.auditionSynthPreviewMidiVelocity,
                instrumentSynthBindings.auditionSynthOscillatorPreview,
                instrumentSynthBindings.auditionArmedInstrument,
                [&](int index) { selectInstrument(index); },
                instrumentSynthBindings.clampInstrumentIndex,
                coreBindings.activeSnapshot,
                [&](const AppActionRequest& request) { return coreBindings.runAction(request, true); },
                [&](int inst, const std::string& oscillator, const std::string& wave) {
                    return instrumentSynthBindings.setSynthWaveform(inst, oscillator, wave, true);
                },
                [&](int inst, const std::string& param, double value) {
                    return instrumentSynthBindings.setSynthParameter(inst, param, value, true);
                },
                [&](int inst, const std::string& param, double value, bool refreshRequested) {
                    return instrumentSynthBindings.setSynthParameter(inst, param, value, refreshRequested);
                },
                [&](const std::string& name) { return findSynthParamDef(name); },
                [&](const SynthPatch& patch, const std::string& name) {
                    return getSynthParameterValue(patch, name);
                },
                [&](InlinePromptKind kind,
                    const std::string& title,
                    const std::string& hint,
                    const std::string& value,
                    int track,
                    int instrument) {
                    coreBindings.beginInlinePrompt(kind, title, hint, value, track, instrument);
                },
                coreBindings.refreshSnapshot,
                midiInput,
                synthMidiPreviewHeld});

    GuiMainWindowInteractionAdapterContext mainInteractionContext =
        makeMainInteractionContextFromWindowState(
            GuiWindowMainInteractionFactoryInput {
                unsavedPrompt,
                unsavedPromptChoices,
                [&](UnsavedChangesChoice choice) { resolveUnsavedPrompt(choice); },
                instrumentBrowserActive,
                instrumentBrowserAcceptButton,
                instrumentBrowserCancelButton,
                instrumentBrowserListRect,
                instrumentBrowserHitTargets,
                instrumentBrowserSelected,
                instrumentBrowserScroll,
                instrumentBrowserQuery,
                audioTuningDialogActive,
                audioTuningDialogHits,
                audioTuningDialogRect,
                inlinePrompt,
                synthWindowVisible,
                coreBindings.isSynthInlinePromptKind,
                inlinePromptUsesFileBrowser,
                inlinePromptButtonsVisible,
                inlinePromptAcceptButton,
                inlinePromptCancelButton,
                fileBrowserHits,
                fileBrowserListRect,
                fileBrowserScroll,
                fileBrowserSelected,
                fileBrowserEntries,
                fileBrowserDirectory,
                refreshFileBrowserEntries,
                executeInlinePrompt,
                cancelInlinePrompt,
                layout,
                sidebarViewport,
                instrumentListRect,
                sidebarContentHeight,
                sidebarScrollOffset,
                viewStartRow,
                draggingSelection,
                dragAnchorRow,
                dragAnchorTrack,
                gridTrackStart,
                activePatternRows,
                paintNoteMidi,
                keyboardSelectionActive,
                keyboardSelectionAnchorRow,
                keyboardSelectionAnchorTrack,
                paintingNotes,
                lastPaintRow,
                lastPaintTrack,
                pointerX,
                pointerY,
                windowWidth,
                windowHeight,
                resizingSidebar,
                resizingTopPanel,
                draggingPatternRows,
                patternResizeAnchorY,
                patternResizeStartRows,
                topPanelHeightState,
                sidebarWidthState,
                hoveredTrackHeader,
                trackHeaderHits,
                armedInstrument,
                armedOctave,
                stepAdvance,
                defaultVelocity,
                synthPreviewMidi,
                requestedRowCount,
                midiImportSplitByTrack,
                targetSongLengthMinutes,
                themeMode,
                selectedOrderIndex,
                midiImportRowsPerBeat,
                midiImportPatternRows,
                followPlayback,
                transportButtons,
                fileButtons,
                themeButtons,
                audioPerformanceButtons,
                orderSlotHits,
                gridTrackPrevButton,
                gridTrackNextButton,
                patternPrevButton,
                patternNextButton,
                patternNewButton,
                patternCloneButton,
                patternDeleteButton,
                orderPrevButton,
                orderNextButton,
                orderInsertButton,
                orderAppendButton,
                orderDeleteButton,
                octaveHitTargets,
                pianoKeyHits,
                trackMetadataHits,
                songLengthHits,
                midiImportSettingHits,
                instrumentControlHits,
                instrumentHitTargets,
                patternRowsMinus,
                patternRowsPlus,
                patternRowsValue,
                stepAdvanceButton,
                followPlaybackButton,
                audioRuntime,
                coreBindings.audibleTrackCount,
                coreBindings.setAudioPerformanceMode,
                coreBindings.adjustAudioCustomLevel,
                editorStateBindings.resizePatternRows,
                [&]() { coreBindings.lockManualScroll(1600); },
                coreBindings.refreshSnapshot,
                coreBindings.activeSnapshot,
                [&](const std::string& actionId) {
                    AppActionRequest request;
                    request.actionId = actionId;
                    return coreBindings.runAction(request, true);
                },
                [&](const AppActionRequest& request) { return coreBindings.runAction(request, true); },
                coreBindings.runAction,
                coreBindings.runSync,
                coreBindings.runEvents,
                [&]() { return session.playback().snapshot(); },
                [&]() { return audioRuntime.performanceMode(); },
                editorStateBindings.setArmedOctave,
                editorStateBindings.applyArmedOctaveToSelection,
                editorStateBindings.ensurePatternRowsForRow,
                [&](unsigned int claimKeycode, int midi) { return claimSynthPreviewKey(claimKeycode, midi); },
                [&](bool visible) { setSynthWindowVisible(visible); },
                [&](const std::string& actionId) { runFileButtonAction(actionId); },
                [&](InlinePromptKind kind,
                    const std::string& title,
                    const std::string& hint,
                    const std::string& initialValue,
                    int targetTrack,
                    int targetInstrument) {
                    coreBindings.beginInlinePrompt(kind, title, hint, initialValue, targetTrack, targetInstrument);
                },
                editorStateBindings.ensureSynthKeyboardShowsMidi,
                editingBindings,
                instrumentSynthBindings});

    draw = [&]() {
        renderMainWindowFromAdapter(
            GuiWindowMainRenderAdapterContext {
                display,
                gc,
                uiFont,
                window,
                trackerBackbuffer,
                windowWidth,
                windowHeight,
                themeMode,
                dosTheme,
                highContrastTheme,
                session,
                audioRuntime,
                mainInteractionContext,
                instrumentListVisibleRows,
                instrumentListStart,
                lastAction,
                patternValueButton,
                orderValueButton,
                [&]() { clampInstrumentListWindow(); },
                coreBindings.activeSnapshot,
                coreBindings.isSynthInlinePromptKind,
                [&](InlinePromptKind kind) { return inlinePromptUsesFileBrowser(kind); },
                [&]() { refreshFileBrowserEntries(); },
                [&](const AppSessionSnapshot& snapshot) { return filteredInstrumentIndices(snapshot); }});
    };

    drawSynthWindow = [&]() {
        drawSynthWindowFromAdapter(
            GuiWindowSynthRenderAdapterContext {
                display,
                uiFont,
                themeMode,
                dosTheme,
                highContrastTheme,
                scopeColors,
                synthLifecycleBindings.ensureSynthBackbuffer,
                synthInteractionContext,
                synthGc,
                synthBackbuffer,
                midiInput.status(),
                synthParamDefs,
                synthScopeTrackedMidi,
                synthScopePhase,
                synthScopeLaneSynth,
                synthScopeLaneLeft,
                synthScopeLaneRight,
                synthScopeLaneActive,
                synthScopeLaneInstrument,
                synthScopeLaneMidi,
                synthScopeLaneGateSeconds,
                synthScopeRetriggerRequested,
                [&]() { return collectSynthPreviewNotes(); }});
    };

    runMainLoopFromAdapter(
        GuiWindowRunLoopAdapterContext {
            display,
            window,
            wmDelete,
            running,
            needsRedraw,
            synthWindowNeedsRedraw,
            windowWidth,
            windowHeight,
            resizingSidebar,
            resizingTopPanel,
            draggingSelection,
            draggingPatternRows,
            paintingNotes,
            lastPaintRow,
            lastPaintTrack,
            sidebarViewport,
            layout,
            pointerX,
            pointerY,
            mainInteractionContext,
            synthInteractionContext,
            synthWindowVisible,
            previousAudioStreamActive,
            synthTooltipParam,
            synthTooltipHoverSince,
            lastRefresh,
            isAutoRepeatRelease,
            synthLifecycleBindings.releaseSynthPreviewKey,
            coreBindings.ensureTrackerBackbuffer,
            [&]() { return session.playback().snapshot().sampleRate; },
            [&]() {
                processMainRealtimeAudio(
                    GuiMainRealtimeAudioContext {
                        session,
                        audioRuntime,
                        audioLeft,
                        audioRight,
                        lastPlaybackTick,
                        audioPendingFrames,
                        previousAudioStreamActive,
                        lastAction,
                        coreBindings.openAudioOutput,
                        coreBindings.closeAudioOutput,
                        coreBindings.writeAudioOutput,
                        coreBindings.tuneRealtimeAudioForLoad});
            },
            coreBindings.refreshSnapshot,
            [&]() { draw(); },
            [&]() { drawSynthWindow(); }});

    shutdownGuiWindowSession(
        GuiWindowShutdownContext {
            display,
            uiFont,
            synthWindow,
            synthGc,
            gc,
            window,
            coreBindings.closeAudioOutput,
            midiInput,
            coreBindings.releaseTrackerBackbuffer,
            synthLifecycleBindings.releaseSynthBackbuffer});
    return 0;
}

} // namespace arachno
