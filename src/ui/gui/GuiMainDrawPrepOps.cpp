#include "ui/gui/GuiMainDrawPrepOps.h"

#include <algorithm>

namespace arachno {

void resetMainDrawTransientState(const GuiMainDrawResetContext& context) {
    context.transportButtons.clear();
    context.instrumentHitTargets.clear();
    context.instrumentControlHits.clear();
    context.instrumentBrowserHitTargets.clear();
    context.trackHeaderHits.clear();
    context.orderSlotHits.clear();
    context.fileButtons.clear();
    context.themeButtons.clear();
    context.audioPerformanceButtons.clear();
    context.audioTuningDialogHits.clear();
    context.octaveHitTargets.clear();
    context.pianoKeyHits.clear();
    context.trackMetadataHits.clear();
    context.songLengthHits.clear();
    context.midiImportSettingHits.clear();
    context.fileBrowserHits.clear();
    context.inlinePromptButtonsVisible = false;
    context.unsavedPromptChoices.clear();

    context.patternRowsMinus = UiRect {};
    context.patternRowsPlus = UiRect {};
    context.patternRowsValue = UiRect {};
    context.gridTrackPrevButton = UiRect {};
    context.gridTrackNextButton = UiRect {};
    context.patternPrevButton = UiRect {};
    context.patternNextButton = UiRect {};
    context.patternValueButton = UiRect {};
    context.patternNewButton = UiRect {};
    context.patternCloneButton = UiRect {};
    context.patternDeleteButton = UiRect {};
    context.orderPrevButton = UiRect {};
    context.orderNextButton = UiRect {};
    context.orderValueButton = UiRect {};
    context.orderInsertButton = UiRect {};
    context.orderAppendButton = UiRect {};
    context.orderDeleteButton = UiRect {};
    context.stepAdvanceButton = UiRect {};
    context.followPlaybackButton = UiRect {};
    context.instrumentListRect = UiRect {};
    context.instrumentBrowserListRect = UiRect {};
    context.instrumentBrowserAcceptButton = UiRect {};
    context.instrumentBrowserCancelButton = UiRect {};
    context.sidebarViewport = UiRect {};
    context.fileBrowserListRect = UiRect {};
    context.audioTuningDialogRect = UiRect {};
}

GuiMainDrawLayoutResult computeMainDrawLayout(
    int windowWidth,
    int windowHeight,
    int& topPanelHeightState,
    int& sidebarWidthState,
    TrackerWindowLayout& layout) {
    GuiMainDrawLayoutResult result;
    const int minimumTopPanelHeight = 240;
    topPanelHeightState = std::clamp(
        topPanelHeightState,
        minimumTopPanelHeight,
        std::max(minimumTopPanelHeight, windowHeight - 220));

    result.statusHeight = std::max(70, topPanelHeightState - result.headerHeight - 8);
    result.gridTop = result.margin + result.headerHeight + result.statusHeight + 8;
    result.gridBottom = windowHeight - result.margin;
    result.gridHeight = std::max(120, result.gridBottom - result.gridTop);

    sidebarWidthState = std::clamp(sidebarWidthState, 220, std::max(240, windowWidth - 420));
    result.sidebarWidth = sidebarWidthState;
    result.gridWidth = std::max(300, windowWidth - (result.margin * 3) - result.sidebarWidth);
    result.gridLeft = result.margin;
    result.sidebarLeft = result.gridLeft + result.gridWidth + result.margin;

    layout.margin = result.margin;
    layout.topPanelHeight = result.headerHeight + result.statusHeight + 8;
    layout.sidebarWidth = result.sidebarWidth;
    layout.gridLeft = result.gridLeft;
    layout.gridTop = result.gridTop;
    layout.gridWidth = result.gridWidth;
    layout.gridHeight = result.gridHeight;
    layout.sidebarLeft = result.sidebarLeft;
    layout.sidebarHeight = result.gridHeight;
    layout.verticalSplitterX = result.sidebarLeft - (result.margin / 2);
    layout.horizontalSplitterY = result.gridTop - 4;

    return result;
}

GuiMainDrawThemeColors makeMainDrawThemeColors(const UiThemePalette& theme) {
    return GuiMainDrawThemeColors {
        theme.background,
        theme.panel,
        theme.gridHeader,
        theme.gridLine,
        theme.selection,
        theme.cursor,
        theme.text,
        theme.mutedText,
        theme.playhead,
        theme.button,
        theme.buttonActive,
        theme.buttonLabel,
        theme.buttonLabelActive,
        theme.selectionText,
        theme.cursorText,
        theme.playheadText,
        theme.activeTagText};
}

} // namespace arachno
