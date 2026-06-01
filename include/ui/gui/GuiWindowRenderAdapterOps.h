#pragma once

#include <array>
#include <functional>
#include <string>
#include <vector>

#include <X11/Xlib.h>

#include "AppActions.h"
#include "ApplicationSession.h"
#include "Synthesizer.h"
#include "ui/gui/GuiAudioRuntime.h"
#include "ui/gui/GuiMainWindowInteractionAdapterOps.h"
#include "ui/gui/GuiSynthWindowInteractionAdapterOps.h"
#include "ui/gui/GuiThemePaletteOps.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiWindowMainRenderAdapterContext {
    Display* display = nullptr;
    GC gc = nullptr;
    XFontStruct* uiFont = nullptr;
    Window window = 0;
    Pixmap trackerBackbuffer = 0;
    int& windowWidth;
    int& windowHeight;
    GuiThemeMode& themeMode;
    const UiThemePalette& dosTheme;
    const UiThemePalette& highContrastTheme;
    ApplicationSession& session;
    GuiAudioRuntime& audioRuntime;
    GuiMainWindowInteractionAdapterContext& interaction;
    int& instrumentListVisibleRows;
    int& instrumentListStart;
    const AppActionResult& lastAction;
    UiRect& patternValueButton;
    UiRect& orderValueButton;
    std::function<void()> clampInstrumentListWindow;
    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<bool(InlinePromptKind)> isSynthInlinePromptKind;
    std::function<bool(InlinePromptKind)> inlinePromptUsesFileBrowser;
    std::function<void()> refreshFileBrowserEntries;
    std::function<std::vector<int>(const AppSessionSnapshot&)> filteredInstrumentIndices;
};

void renderMainWindowFromAdapter(const GuiWindowMainRenderAdapterContext& context);

struct GuiWindowSynthRenderAdapterContext {
    Display* display = nullptr;
    XFontStruct* uiFont = nullptr;
    GuiThemeMode themeMode = GuiThemeMode::Dos;
    const UiThemePalette& dosTheme;
    const UiThemePalette& highContrastTheme;
    const GuiSynthWindowScopeColors& scopeColors;
    std::function<void()> ensureSynthBackbuffer;
    GuiSynthWindowInteractionAdapterContext& interaction;
    GC synthGc = nullptr;
    Pixmap synthBackbuffer = 0;
    std::string midiStatusText;
    const std::vector<SynthParamDef>& synthParamDefs;
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
};

void drawSynthWindowFromAdapter(const GuiWindowSynthRenderAdapterContext& context);

} // namespace arachno
