#include "ui/gui/GuiMainWindowContentDrawOps.h"

#include <algorithm>
#include <cmath>

#include "ui/gui/GuiMainGridDrawOps.h"
#include "ui/gui/GuiMainModalDrawOps.h"
#include "ui/gui/GuiMainSidebarDrawOps.h"
#include "ui/gui/GuiMainTopPanelDrawOps.h"

namespace arachno {

void drawMainWindowContent(const GuiMainWindowContentDrawContext& context) {
    drawMainTopPanelSection(
        GuiMainTopPanelDrawContext {
            context.snapshot,
            context.playback,
            context.audioRuntime,
            context.windowWidth,
            context.margin,
            context.headerHeight,
            context.statusHeight,
            context.lineHeight,
            context.gridTop,
            context.gridHeight,
            context.layout.verticalSplitterX,
            context.layout.horizontalSplitterY,
            context.synthWindowVisible,
            context.audioTuningDialogActive,
            context.selectedOrderIndex,
            context.armedInstrument,
            context.armedOctave,
            context.defaultVelocity,
            context.stepAdvance,
            context.followPlayback,
            context.themeMode,
            context.colorPanel,
            context.colorGridLine,
            context.colorText,
            context.colorMutedText,
            context.colorCursor,
            context.colorPlayhead,
            context.colorButton,
            context.colorButtonActive,
            context.colorButtonLabel,
            context.colorButtonLabelActive,
            context.colorActiveTagText,
            context.transportButtons,
            context.fileButtons,
            context.themeButtons,
            context.audioPerformanceButtons,
            context.orderSlotHits,
            context.patternPrevButton,
            context.patternNextButton,
            context.patternValueButton,
            context.patternNewButton,
            context.patternCloneButton,
            context.patternDeleteButton,
            context.orderPrevButton,
            context.orderNextButton,
            context.orderValueButton,
            context.orderInsertButton,
            context.orderAppendButton,
            context.orderDeleteButton,
            context.drawFilledRect,
            context.drawRect,
            context.drawText,
            context.drawButton,
            context.controlTextBaseline,
            context.textWidth});

    drawMainGridSection(
        GuiMainGridDrawContext {
            context.snapshot,
            context.playback,
            context.gridLeft,
            context.gridTop,
            context.gridWidth,
            context.gridHeight,
            context.gridTrackStart,
            context.colorPanel,
            context.colorGridLine,
            context.colorGridHeader,
            context.colorText,
            context.colorMutedText,
            context.colorButton,
            context.colorButtonActive,
            context.colorActiveTagText,
            context.colorSelection,
            context.colorCursor,
            context.colorSelectionText,
            context.colorCursorText,
            context.colorPlayhead,
            context.colorPlayheadText,
            context.margin,
            context.windowWidth,
            context.windowHeight,
            context.pointerX,
            context.pointerY,
            context.hoveredTrackHeader,
            context.layout,
            context.requestedRowCount,
            context.gridTrackStart,
            context.trackHeaderHits,
            context.gridTrackPrevButton,
            context.gridTrackNextButton,
            context.drawFilledRect,
            context.drawRect,
            context.drawText,
            context.drawButton,
            context.fitText,
            context.textWidth});

    context.drawFilledRect(
        context.sidebarLeft,
        context.gridTop,
        context.sidebarWidth,
        context.gridHeight,
        context.colorPanel);
    context.drawRect(
        context.sidebarLeft,
        context.gridTop,
        context.sidebarWidth,
        context.gridHeight,
        context.colorGridLine);
    context.sidebarViewport =
        UiRect {context.sidebarLeft + 1, context.gridTop + 1, context.sidebarWidth - 2, context.gridHeight - 2};
    const int sidebarViewportHeight = std::max(1, context.sidebarViewport.height - 8);
    const int maxSidebarScroll = std::max(0, context.sidebarContentHeight - sidebarViewportHeight);
    context.sidebarScrollOffset = std::clamp(context.sidebarScrollOffset, 0, maxSidebarScroll);
    const int sidebarContentStartY = context.gridTop + 20;
    int sy = sidebarContentStartY - context.sidebarScrollOffset;
    context.setClipRect(
        context.sidebarViewport.x,
        context.sidebarViewport.y,
        std::max(0, context.sidebarViewport.width),
        std::max(0, context.sidebarViewport.height));
    sy = drawMainSidebarSections(
        GuiMainSidebarDrawContext {
            context.sidebarLeft,
            context.sidebarWidth,
            context.gridTop,
            context.gridHeight,
            context.colorText,
            context.colorMutedText,
            context.colorSelection,
            context.colorGridLine,
            context.colorButtonActive,
            context.theme.pianoWhite,
            context.theme.pianoBlack,
            context.snapshot,
            context.lastAction,
            context.paintNoteMidi,
            context.armedOctave,
            context.armedInstrument,
            context.activePatternRows,
            context.stepAdvance,
            context.followPlayback,
            context.targetSongLengthMinutes,
            context.midiImportRowsPerBeat,
            context.midiImportPatternRows,
            context.midiImportSplitByTrack,
            context.instrumentListVisibleRows,
            context.instrumentListStart,
            context.clampInstrumentListWindow,
            context.octaveHitTargets,
            context.pianoKeyHits,
            context.patternRowsMinus,
            context.patternRowsValue,
            context.patternRowsPlus,
            context.stepAdvanceButton,
            context.legatoButton,
            context.followPlaybackButton,
            context.trackMetadataHits,
            context.songLengthHits,
            context.midiImportSettingHits,
            context.instrumentControlHits,
            context.instrumentHitTargets,
            context.instrumentListRect,
            context.drawFilledRect,
            context.drawRect,
            context.drawText,
            context.drawButton},
        sy);
    context.clearClip();

    context.sidebarContentHeight = std::max(0, (sy + context.sidebarScrollOffset) - sidebarContentStartY);
    if (context.sidebarContentHeight > sidebarViewportHeight) {
        const int trackX = context.sidebarLeft + context.sidebarWidth - 8;
        const int trackY = context.gridTop + 6;
        const int trackH = std::max(12, context.gridHeight - 12);
        context.drawFilledRect(trackX, trackY, 4, trackH, context.colorGridLine);
        const double visibleRatio =
            static_cast<double>(sidebarViewportHeight) / static_cast<double>(std::max(1, context.sidebarContentHeight));
        const int thumbH = std::max(16, static_cast<int>(std::lround(static_cast<double>(trackH) * visibleRatio)));
        const double scrollRatio =
            static_cast<double>(context.sidebarScrollOffset)
            / static_cast<double>(std::max(1, context.sidebarContentHeight - sidebarViewportHeight));
        const int thumbTravel = std::max(0, trackH - thumbH);
        const int thumbY = trackY + static_cast<int>(std::lround(scrollRatio * static_cast<double>(thumbTravel)));
        context.drawFilledRect(trackX, thumbY, 4, thumbH, context.colorButtonActive);
    }

    drawMainModalOverlays(
        GuiMainModalDrawContext {
            context.windowWidth,
            context.windowHeight,
            context.colorPanel,
            context.colorGridLine,
            context.colorText,
            context.colorMutedText,
            context.colorBackground,
            context.colorSelection,
            context.colorSelectionText,
            context.colorButtonActive,
            context.snapshot,
            context.unsavedPrompt,
            context.unsavedPromptChoices,
            context.instrumentBrowserActive,
            context.instrumentBrowserQuery,
            context.instrumentBrowserScroll,
            context.instrumentBrowserSelected,
            context.instrumentBrowserHitTargets,
            context.instrumentBrowserListRect,
            context.instrumentBrowserAcceptButton,
            context.instrumentBrowserCancelButton,
            context.filteredInstrumentIndices,
            context.armedInstrument,
            context.audioTuningDialogActive,
            context.audioTuningDialogRect,
            context.audioTuningDialogHits,
            context.audioRuntime.performanceMode(),
            context.audioRuntime.customLevel(),
            context.audioRuntime.frameMin(),
            context.audioRuntime.frameMax(),
            context.inlinePrompt,
            context.synthWindowVisible,
            context.isSynthInlinePromptKind,
            context.inlinePromptUsesFileBrowser,
            context.refreshFileBrowserEntries,
            context.fileBrowserEntries,
            context.fileBrowserDirectory,
            context.fileBrowserScroll,
            context.fileBrowserSelected,
            context.fileBrowserHits,
            context.fileBrowserListRect,
            context.inlinePromptAcceptButton,
            context.inlinePromptCancelButton,
            context.inlinePromptButtonsVisible,
            context.drawFilledRect,
            context.drawRect,
            context.drawText,
            context.drawButton,
            context.fitText,
            context.textWidth});
}

} // namespace arachno
