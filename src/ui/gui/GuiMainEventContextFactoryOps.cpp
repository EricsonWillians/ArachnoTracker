#include "ui/gui/GuiMainEventContextFactoryOps.h"

namespace arachno {

GuiMainModalMouseContext makeMainModalMouseContextFromState(const GuiMainModalMouseContextFactoryInput& input) {
    return GuiMainModalMouseContext {
        input.unsavedPrompt,
        input.unsavedPromptChoices,
        input.resolveUnsavedPrompt,
        input.instrumentBrowserActive,
        input.instrumentBrowserAcceptButton,
        input.instrumentBrowserCancelButton,
        input.instrumentBrowserListRect,
        input.instrumentBrowserHitTargets,
        input.instrumentBrowserSelected,
        input.instrumentBrowserScroll,
        input.closeInstrumentBrowser,
        input.audioTuningDialogActive,
        input.audioTuningDialogHits,
        input.audioTuningDialogRect,
        input.playbackSampleRate,
        input.setAudioPerformanceMode,
        input.adjustAudioCustomLevel,
        input.resetAudioCustomLevel,
        input.inlinePrompt,
        input.synthWindowVisible,
        input.isSynthInlinePromptKind,
        input.inlinePromptUsesFileBrowser,
        input.inlinePromptButtonsVisible,
        input.inlinePromptAcceptButton,
        input.inlinePromptCancelButton,
        input.fileBrowserHits,
        input.fileBrowserListRect,
        input.fileBrowserScroll,
        input.fileBrowserSelected,
        input.fileBrowserEntries,
        input.fileBrowserDirectory,
        input.refreshFileBrowserEntries,
        input.executeInlinePrompt,
        input.cancelInlinePrompt};
}

GuiMainMotionContext makeMainMotionContextFromState(const GuiMainMotionContextFactoryInput& input) {
    return GuiMainMotionContext {
        input.mx,
        input.my,
        input.stateMask,
        input.pointerX,
        input.pointerY,
        input.windowWidth,
        input.windowHeight,
        input.layout,
        input.resizingSidebar,
        input.resizingTopPanel,
        input.draggingPatternRows,
        input.paintingNotes,
        input.draggingSelection,
        input.patternResizeAnchorY,
        input.patternResizeStartRows,
        input.topPanelHeightState,
        input.sidebarWidthState,
        input.paintNoteMidi,
        input.lastPaintRow,
        input.lastPaintTrack,
        input.dragAnchorRow,
        input.dragAnchorTrack,
        input.viewStartRow,
        input.hoveredTrackHeader,
        input.trackHeaderHits,
        input.resizePatternRows,
        input.gridPositionToCell,
        input.paintNoteAt,
        input.refreshSnapshot,
        input.applySelectionRange,
        input.lockManualScroll};
}

} // namespace arachno
