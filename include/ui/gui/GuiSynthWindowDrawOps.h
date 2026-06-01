#pragma once

#include <array>
#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "ApplicationSession.h"
#include "Synthesizer.h"
#include "ui/gui/GuiSynthComposerOps.h"
#include "ui/gui/GuiSynthTopSectionOps.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiSynthWindowThemeColors {
    unsigned long background = 0;
    unsigned long panel = 0;
    unsigned long gridHeader = 0;
    unsigned long gridLine = 0;
    unsigned long text = 0;
    unsigned long mutedText = 0;
    unsigned long button = 0;
    unsigned long buttonActive = 0;
    unsigned long buttonLabel = 0;
    unsigned long buttonLabelActive = 0;
    unsigned long selection = 0;
    unsigned long selectionText = 0;
    unsigned long pianoWhite = 0;
    unsigned long pianoBlack = 0;
};

struct GuiSynthWindowScopeColors {
    unsigned long tealMain = 0;
    unsigned long tealGlow = 0;
    unsigned long tealDark = 0;
    unsigned long waveSine = 0;
    unsigned long waveSquare = 0;
    unsigned long waveSaw = 0;
    unsigned long waveTriangle = 0;
    unsigned long waveNoise = 0;
    unsigned long waveSupersaw = 0;
};

struct GuiSynthWindowDrawContext {
    ApplicationSession& session;
    int synthWindowWidth = 0;
    int synthWindowHeight = 0;
    float defaultVelocity = 0.8f;
    int synthPreviewMidi = 60;
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

    GuiSynthWindowThemeColors theme;
    GuiSynthWindowScopeColors scope;

    std::function<int()> clampInstrumentIndex;
    std::function<void()> copyToWindowAndFlush;

    std::function<void(int, int, int, int, unsigned long)> drawFilledRect;
    std::function<void(int, int, int, int, unsigned long)> drawRect;
    std::function<void(int, int, const std::string&, unsigned long)> drawText;
    std::function<void(const UiRect&, const std::string&, bool)> drawButton;
    std::function<void(const UiRect&, double, bool)> drawKnob;
    std::function<std::string(const std::string&, int)> fitText;
    std::function<int(const std::string&)> textWidth;
    std::function<void(unsigned long)> setStrokeColor;
    std::function<void(int, int, int, int)> drawLine;
    std::function<void(int, int)> drawPoint;
    std::function<void(int, int, int, int)> setClipRect;
    std::function<void()> clearClip;
};

void drawSynthWindowContent(const GuiSynthWindowDrawContext& context);

} // namespace arachno
