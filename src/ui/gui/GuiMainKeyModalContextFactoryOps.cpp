#include "ui/gui/GuiMainKeyModalContextFactoryOps.h"

namespace arachno {

GuiMainKeyModalContext makeMainKeyModalContextFromState(const GuiMainKeyModalContextFactoryInput& input) {
    return GuiMainKeyModalContext {
        input.key,
        input.ctrlDown,
        input.shiftDown,
        input.altDown,
        input.lookupCount,
        input.lookupBuffer,
        input.keyMatches,
        input.resolvedDigit,
        input.unsavedPromptActive,
        input.resolveUnsavedPrompt,
        input.audioTuningDialogActive,
        input.playbackSampleRate,
        input.adjustAudioCustomLevel,
        input.setAudioPerformanceMode,
        input.inlinePrompt,
        input.synthWindowVisible,
        input.isSynthInlinePromptKind,
        input.inlinePromptUsesFileBrowser,
        input.fileBrowserSelected,
        input.fileBrowserScroll,
        input.fileBrowserListRect,
        input.fileBrowserEntries,
        input.fileBrowserDirectory,
        input.refreshFileBrowserEntries,
        input.executeInlinePrompt,
        input.cancelInlinePrompt,
        input.instrumentBrowserActive,
        input.instrumentBrowserSelected,
        input.instrumentBrowserScroll,
        input.instrumentBrowserQuery,
        input.closeInstrumentBrowser,
        input.cycleInstrumentBy,
        input.syncInstrumentBrowserSelectionFromArmed,
        input.filteredInstrumentCount};
}

} // namespace arachno
