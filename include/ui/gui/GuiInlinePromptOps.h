#pragma once

#include <functional>
#include <string>

#include "AppActions.h"
#include "ApplicationSession.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct SynthPatch;

struct GuiInlinePromptExecutionContext {
    ApplicationSession& session;
    InlinePromptState& inlinePrompt;
    int& midiImportRowsPerBeat;
    int& midiImportPatternRows;
    bool& midiImportSplitByTrack;
    AppActionResult& lastAction;
    bool& synthWindowNeedsRedraw;
    double& targetSongLengthMinutes;
    int& activePatternRows;
    bool& keyboardSelectionActive;
    int& viewStartRow;

    std::function<void()> clearInlinePrompt;
    std::function<void(const AppActionRequest&)> runLifecycleAction;
    std::function<void(const AppActionRequest&)> runAction;
    std::function<void()> handleDeferredPostSave;
    std::function<void(int)> selectInstrument;
    std::function<bool(int, const SynthPatch&, bool)> applyPatchToInstrument;
    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<bool(const std::string&)> executeTemporalPasteSpec;
};

void executeInlinePromptOps(const GuiInlinePromptExecutionContext& context);

} // namespace arachno
