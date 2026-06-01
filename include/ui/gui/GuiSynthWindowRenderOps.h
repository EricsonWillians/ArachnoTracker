#pragma once

#include <array>
#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <X11/Xlib.h>

#include "ApplicationSession.h"
#include "Synthesizer.h"
#include "ui/gui/GuiSynthWindowDrawOps.h"
#include "ui/gui/GuiThemePaletteOps.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiSynthWindowRenderContext {
    Display* display = nullptr;
    XFontStruct* uiFont = nullptr;

    bool synthWindowVisible = false;
    Window synthWindow = 0;
    GC synthGc = nullptr;
    Pixmap synthBackbuffer = 0;
    int synthWindowWidth = 0;
    int synthWindowHeight = 0;
    GuiThemeMode themeMode = GuiThemeMode::Dos;
    const UiThemePalette& dosTheme;
    const UiThemePalette& highContrastTheme;
    const GuiSynthWindowScopeColors& scopeColors;
    std::function<void()> ensureSynthBackbuffer;

    ApplicationSession& session;
    float defaultVelocity = 0.8f;
    int& synthPreviewMidi;
    std::string midiStatusText;

    std::vector<SynthWindowHit>& synthWindowHits;
    std::vector<PianoKeyHit>& synthKeyboardHits;
    std::vector<FileBrowserHit>& fileBrowserHits;
    UiRect& fileBrowserListRect;
    UiRect& synthParamViewport;
    int& synthParamContentHeight;

    int& synthKeyboardBaseOctave;
    int& synthKeyboardVisibleOctaves;
    int& synthParamPage;
    int& synthParamOscTarget;
    int& synthParamScroll;
    std::string& synthTooltipParam;
    int synthPointerX = 0;
    int synthPointerY = 0;
    std::chrono::steady_clock::time_point synthTooltipHoverSince {};
    const std::vector<SynthParamDef>& synthParamDefs;

    InlinePromptState& inlinePrompt;
    std::function<bool(InlinePromptKind)> isSynthInlinePromptKind;
    std::function<bool(InlinePromptKind)> inlinePromptUsesFileBrowser;
    std::function<void()> refreshFileBrowserEntries;
    std::filesystem::path& fileBrowserDirectory;
    std::vector<FileBrowserEntry>& fileBrowserEntries;
    int& fileBrowserScroll;
    int& fileBrowserSelected;
    UiRect& synthInlinePromptAcceptButton;
    UiRect& synthInlinePromptCancelButton;
    bool& synthInlinePromptButtonsVisible;

    int& synthScopeTrackedMidi;
    std::array<double, 4>& synthScopePhase;
    std::array<Synthesizer, 4>& synthScopeLaneSynth;
    std::array<std::array<float, kSynthScopeTraceSamples>, 4>& synthScopeLaneLeft;
    std::array<std::array<float, kSynthScopeTraceSamples>, 4>& synthScopeLaneRight;
    bool& synthScopeLaneActive;
    int& synthScopeLaneInstrument;
    int& synthScopeLaneMidi;
    double& synthScopeLaneGateSeconds;
    bool& synthScopeRetriggerRequested;
    std::function<std::vector<int>()> collectSynthPreviewNotes;

    std::function<int()> clampInstrumentIndex;
};

void drawSynthWindowFromWindowState(const GuiSynthWindowRenderContext& context);

} // namespace arachno
