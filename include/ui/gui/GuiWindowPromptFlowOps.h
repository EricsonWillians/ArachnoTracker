#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "AppActions.h"
#include "ApplicationSession.h"
#include "Synthesizer.h"
#include "ui/gui/GuiFileBrowserOps.h"
#include "ui/gui/GuiSessionWindowOps.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiWindowPromptFlowFactoryInput {
    ApplicationSession& session;
    InlinePromptState& inlinePrompt;
    std::filesystem::path& fileBrowserDirectory;
    std::vector<FileBrowserEntry>& fileBrowserEntries;
    std::vector<FileBrowserHit>& fileBrowserHits;
    int& fileBrowserScroll;
    int& fileBrowserSelected;
    std::function<bool(InlinePromptKind)>& inlinePromptUsesFileBrowser;
    std::function<void()>& initFileBrowserFromPrompt;
    bool& inlinePromptButtonsVisible;
    bool& synthInlinePromptButtonsVisible;
    bool& hasDeferredPostSaveAction;
    AppActionRequest& deferredPostSaveAction;
    UnsavedDecisionPromptState& unsavedPrompt;
    std::vector<std::pair<UiRect, UnsavedChangesChoice>>& unsavedPromptChoices;
    int& midiImportRowsPerBeat;
    int& midiImportPatternRows;
    bool& midiImportSplitByTrack;
    AppActionResult& lastAction;
    bool& synthWindowNeedsRedraw;
    std::function<void()> refreshSnapshot;
    double& targetSongLengthMinutes;
    int& activePatternRows;
    bool& keyboardSelectionActive;
    int& viewStartRow;
    std::function<void()> clearInlinePrompt;
    std::function<AppActionResult(const AppActionRequest&)> runAction;
    std::function<void(int)> selectInstrument;
    std::function<bool(int, const SynthPatch&, bool)> applyPatchToInstrument;
    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<bool(const std::string&)> executeTemporalPasteSpec;
    std::function<void(
        InlinePromptKind,
        const std::string&,
        const std::string&,
        const std::string&,
        int,
        int)> beginInlinePrompt;
};

struct GuiWindowPromptFlowContext {
    ApplicationSession& session;

    InlinePromptState& inlinePrompt;
    std::filesystem::path& fileBrowserDirectory;
    std::vector<FileBrowserEntry>& fileBrowserEntries;
    std::vector<FileBrowserHit>& fileBrowserHits;
    int& fileBrowserScroll;
    int& fileBrowserSelected;
    std::function<bool(InlinePromptKind)>& inlinePromptUsesFileBrowser;
    std::function<void()>& initFileBrowserFromPrompt;

    bool& inlinePromptButtonsVisible;
    bool& synthInlinePromptButtonsVisible;
    bool& hasDeferredPostSaveAction;
    AppActionRequest& deferredPostSaveAction;
    UnsavedDecisionPromptState& unsavedPrompt;
    std::vector<std::pair<UiRect, UnsavedChangesChoice>>& unsavedPromptChoices;

    int& midiImportRowsPerBeat;
    int& midiImportPatternRows;
    bool& midiImportSplitByTrack;
    AppActionResult& lastAction;
    bool& synthWindowNeedsRedraw;
    std::function<void()> refreshSnapshot;
    double& targetSongLengthMinutes;
    int& activePatternRows;
    bool& keyboardSelectionActive;
    int& viewStartRow;

    std::function<void()> clearInlinePrompt;
    std::function<void(const AppActionRequest&)> runLifecycleAction;
    std::function<AppActionResult(const AppActionRequest&)> runAction;
    std::function<void()> handleDeferredPostSaveAction;
    std::function<void(int)> selectInstrument;
    std::function<bool(int, const SynthPatch&, bool)> applyPatchToInstrument;
    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<bool(const std::string&)> executeTemporalPasteSpec;
    std::function<void(
        InlinePromptKind,
        const std::string&,
        const std::string&,
        const std::string&,
        int,
        int)> beginInlinePrompt;
};

GuiWindowPromptFlowContext makePromptFlowContextFromWindowState(const GuiWindowPromptFlowFactoryInput& input);

GuiFileBrowserState makePromptFlowFileBrowserState(const GuiWindowPromptFlowContext& context);
void refreshPromptFlowFileBrowserEntries(const GuiWindowPromptFlowContext& context);
void initPromptFlowFileBrowserFromPrompt(const GuiWindowPromptFlowContext& context);
void cancelPromptFlowInlinePrompt(const GuiWindowPromptFlowContext& context);
void clearPromptFlowUnsavedPrompt(const GuiWindowPromptFlowContext& context);
void runPromptFlowLifecycleAction(const GuiWindowPromptFlowContext& context, const AppActionRequest& request);
void handlePromptFlowDeferredPostSave(const GuiWindowPromptFlowContext& context);
void executePromptFlowInlinePrompt(const GuiWindowPromptFlowContext& context);
void resolvePromptFlowUnsavedChoice(const GuiWindowPromptFlowContext& context, UnsavedChangesChoice choice);
void wirePromptFlowCallbacksFromWindowState(
    GuiWindowPromptFlowContext& context,
    std::function<void()>& refreshFileBrowserEntries,
    std::function<void()>& initFileBrowserFromPrompt,
    std::function<void()>& cancelInlinePrompt,
    std::function<void()>& executeInlinePrompt,
    std::function<void(UnsavedChangesChoice)>& resolveUnsavedPrompt);

} // namespace arachno
