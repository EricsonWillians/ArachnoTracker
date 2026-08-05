#include "ui/gui/GuiWindowPromptFlowOps.h"

#include "ui/gui/GuiFileBrowserOps.h"
#include "ui/gui/GuiInlinePromptOps.h"
#include "ui/gui/GuiUnsavedPromptOps.h"

namespace arachno {

GuiWindowPromptFlowContext makePromptFlowContextFromWindowState(const GuiWindowPromptFlowFactoryInput& input) {
    return GuiWindowPromptFlowContext {
        input.session,
        input.inlinePrompt,
        input.fileBrowserDirectory,
        input.fileBrowserEntries,
        input.fileBrowserHits,
        input.fileBrowserScroll,
        input.fileBrowserSelected,
        input.inlinePromptUsesFileBrowser,
        input.initFileBrowserFromPrompt,
        input.inlinePromptButtonsVisible,
        input.synthInlinePromptButtonsVisible,
        input.hasDeferredPostSaveAction,
        input.deferredPostSaveAction,
        input.unsavedPrompt,
        input.unsavedPromptChoices,
        input.midiImportRowsPerBeat,
        input.midiImportPatternRows,
        input.midiImportSplitByTrack,
        input.lastAction,
        input.synthWindowNeedsRedraw,
        input.refreshSnapshot,
        input.targetSongLengthMinutes,
        input.activePatternRows,
        input.keyboardSelectionActive,
        input.viewStartRow,
        input.clearInlinePrompt,
        [](const AppActionRequest&) {},
        input.runAction,
        []() {},
        input.selectInstrument,
        input.applyPatchToInstrument,
        input.activeSnapshot,
        input.executeTemporalPasteSpec,
        input.beginInlinePrompt};
}

GuiFileBrowserState makePromptFlowFileBrowserState(const GuiWindowPromptFlowContext& context) {
    return GuiFileBrowserState {
        context.inlinePrompt,
        context.fileBrowserDirectory,
        context.fileBrowserEntries,
        context.fileBrowserHits,
        context.fileBrowserScroll,
        context.fileBrowserSelected};
}

void refreshPromptFlowFileBrowserEntries(const GuiWindowPromptFlowContext& context) {
    ::arachno::refreshFileBrowserEntries(makePromptFlowFileBrowserState(context));
}

void initPromptFlowFileBrowserFromPrompt(const GuiWindowPromptFlowContext& context) {
    ::arachno::initFileBrowserFromPrompt(makePromptFlowFileBrowserState(context));
}

void cancelPromptFlowInlinePrompt(const GuiWindowPromptFlowContext& context) {
    cancelInlinePromptFromWindowState(
        GuiCancelInlinePromptContext {
            context.inlinePrompt,
            context.hasDeferredPostSaveAction,
            context.clearInlinePrompt,
            context.unsavedPrompt,
            context.deferredPostSaveAction});
}

void clearPromptFlowUnsavedPrompt(const GuiWindowPromptFlowContext& context) {
    clearUnsavedPromptFromWindowState(context.unsavedPrompt, context.unsavedPromptChoices);
}

void runPromptFlowLifecycleAction(const GuiWindowPromptFlowContext& context, const AppActionRequest& request) {
    runLifecycleActionFromWindowState(
        GuiLifecycleActionContext {
            [&](const AppActionRequest& action) { return context.runAction(action); },
            context.unsavedPrompt,
            context.hasDeferredPostSaveAction,
            context.deferredPostSaveAction,
            context.refreshSnapshot,
            context.beginInlinePrompt},
        request);
}

void handlePromptFlowDeferredPostSave(const GuiWindowPromptFlowContext& context) {
    handleDeferredPostSaveActionFromWindowState(
        context.hasDeferredPostSaveAction,
        context.deferredPostSaveAction,
        [&](const AppActionRequest& deferred) { runPromptFlowLifecycleAction(context, deferred); });
}

void executePromptFlowInlinePrompt(const GuiWindowPromptFlowContext& context) {
    executeInlinePromptOps(
        GuiInlinePromptExecutionContext {
            context.session,
            context.inlinePrompt,
            context.midiImportRowsPerBeat,
            context.midiImportPatternRows,
            context.midiImportSplitByTrack,
            context.lastAction,
            context.synthWindowNeedsRedraw,
            context.targetSongLengthMinutes,
            context.activePatternRows,
            context.keyboardSelectionActive,
            context.viewStartRow,
            context.clearInlinePrompt,
            [&](const AppActionRequest& request) { runPromptFlowLifecycleAction(context, request); },
            [&](const AppActionRequest& request) { (void)context.runAction(request); },
            context.handleDeferredPostSaveAction,
            context.selectInstrument,
            context.applyPatchToInstrument,
            context.activeSnapshot,
            context.executeTemporalPasteSpec});
}

void resolvePromptFlowUnsavedChoice(const GuiWindowPromptFlowContext& context, UnsavedChangesChoice choice) {
    resolveUnsavedPromptChoice(
        GuiUnsavedPromptResolveContext {
            context.unsavedPrompt,
            context.lastAction,
            context.hasDeferredPostSaveAction,
            context.deferredPostSaveAction,
            [&]() { clearPromptFlowUnsavedPrompt(context); },
            context.activeSnapshot,
            context.beginInlinePrompt,
            [&](const AppActionRequest& request) { runPromptFlowLifecycleAction(context, request); }},
        choice);
}

void wirePromptFlowCallbacksFromWindowState(
    GuiWindowPromptFlowContext& context,
    std::function<void()>& refreshFileBrowserEntries,
    std::function<void()>& initFileBrowserFromPrompt,
    std::function<void()>& cancelInlinePrompt,
    std::function<void()>& executeInlinePrompt,
    std::function<void(UnsavedChangesChoice)>& resolveUnsavedPrompt) {
    refreshFileBrowserEntries = [&]() {
        refreshPromptFlowFileBrowserEntries(context);
    };

    initFileBrowserFromPrompt = [&]() {
        initPromptFlowFileBrowserFromPrompt(context);
    };

    cancelInlinePrompt = [&]() {
        cancelPromptFlowInlinePrompt(context);
    };

    context.runLifecycleAction = [&](const AppActionRequest& request) {
        runPromptFlowLifecycleAction(context, request);
    };

    context.handleDeferredPostSaveAction = [&]() {
        handlePromptFlowDeferredPostSave(context);
    };

    executeInlinePrompt = [&]() {
        executePromptFlowInlinePrompt(context);
    };

    resolveUnsavedPrompt = [&](UnsavedChangesChoice choice) {
        resolvePromptFlowUnsavedChoice(context, choice);
    };
}

} // namespace arachno
