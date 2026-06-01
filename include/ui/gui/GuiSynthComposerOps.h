#pragma once

#include <array>
#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "ApplicationSession.h"
#include "Synthesizer.h"
#include "ui/gui/GuiSynthInlinePromptSection.h"
#include "ui/gui/GuiSynthKeyboardSection.h"
#include "ui/gui/GuiSynthParamSection.h"
#include "ui/gui/GuiSynthScopeSection.h"
#include "ui/gui/SynthUiData.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiSynthComposerContext {
    ApplicationSession& session;
    const SynthPatch& patch;
    int instrument = 0;
    int panelLeft = 0;
    int panelRight = 0;
    int oscTop = 0;
    int oscRowHeight = 0;
    int synthPreviewMidi = 60;
    float defaultVelocity = 0.8f;

    int& synthKeyboardBaseOctave;
    int& synthKeyboardVisibleOctaves;
    std::string midiStatusText;
    std::vector<PianoKeyHit>& synthKeyboardHits;
    std::vector<SynthWindowHit>& synthWindowHits;

    int synthWindowWidth = 0;
    int synthWindowHeight = 0;
    int& synthParamPage;
    int& synthParamOscTarget;
    int& synthParamScroll;
    UiRect& synthParamViewport;
    int& synthParamContentHeight;
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
    std::vector<FileBrowserHit>& fileBrowserHits;
    UiRect& fileBrowserListRect;
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

    GuiSynthScopeThemeColors scopeTheme;
    GuiSynthScopeTraceColors scopeTrace;
    GuiSynthKeyboardThemeColors keyboardTheme;
    GuiSynthParamSectionThemeColors paramTheme;
    GuiSynthInlinePromptThemeColors inlinePromptTheme;
    unsigned long activeKeyColor = 0;
    unsigned long footerTextColor = 0;

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

void drawSynthComposerSections(const GuiSynthComposerContext& context);

} // namespace arachno
