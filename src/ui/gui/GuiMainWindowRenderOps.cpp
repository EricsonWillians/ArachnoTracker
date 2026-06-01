#include "ui/gui/GuiMainWindowRenderOps.h"

#include <algorithm>

#include <X11/Xutil.h>

#include "ui/gui/GuiMainDrawPrepOps.h"
#include "ui/gui/GuiMainWindowContentDrawOps.h"

namespace arachno {

void renderMainWindowFromState(const GuiMainWindowRenderContext& context) {
    resetMainDrawTransientState(
        GuiMainDrawResetContext {
            context.transportButtons,
            context.instrumentHitTargets,
            context.instrumentControlHits,
            context.instrumentBrowserHitTargets,
            context.trackHeaderHits,
            context.orderSlotHits,
            context.fileButtons,
            context.themeButtons,
            context.audioPerformanceButtons,
            context.audioTuningDialogHits,
            context.octaveHitTargets,
            context.pianoKeyHits,
            context.trackMetadataHits,
            context.songLengthHits,
            context.midiImportSettingHits,
            context.fileBrowserHits,
            context.unsavedPromptChoices,
            context.inlinePromptButtonsVisible,
            context.patternRowsMinus,
            context.patternRowsPlus,
            context.patternRowsValue,
            context.gridTrackPrevButton,
            context.gridTrackNextButton,
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
            context.stepAdvanceButton,
            context.followPlaybackButton,
            context.instrumentListRect,
            context.instrumentBrowserListRect,
            context.instrumentBrowserAcceptButton,
            context.instrumentBrowserCancelButton,
            context.sidebarViewport,
            context.fileBrowserListRect,
            context.audioTuningDialogRect});

    const GuiMainDrawLayoutResult drawLayout = computeMainDrawLayout(
        context.windowWidth,
        context.windowHeight,
        context.topPanelHeightState,
        context.sidebarWidthState,
        context.layout);

    const int lineHeight = drawLayout.lineHeight;
    const int margin = drawLayout.margin;
    const int headerHeight = drawLayout.headerHeight;
    const int statusHeight = drawLayout.statusHeight;
    const int gridTop = drawLayout.gridTop;
    const int gridHeight = drawLayout.gridHeight;
    const int sidebarWidth = drawLayout.sidebarWidth;
    const int gridWidth = drawLayout.gridWidth;
    const int gridLeft = drawLayout.gridLeft;
    const int sidebarLeft = drawLayout.sidebarLeft;

    const UiThemePalette& theme = context.themeMode == GuiThemeMode::HighContrast ? context.highContrastTheme : context.dosTheme;
    const GuiMainDrawThemeColors colors = makeMainDrawThemeColors(theme);
    const unsigned long colorBackground = colors.background;
    const unsigned long colorPanel = colors.panel;
    const unsigned long colorGridHeader = colors.gridHeader;
    const unsigned long colorGridLine = colors.gridLine;
    const unsigned long colorSelection = colors.selection;
    const unsigned long colorCursor = colors.cursor;
    const unsigned long colorText = colors.text;
    const unsigned long colorMutedText = colors.mutedText;
    const unsigned long colorPlayhead = colors.playhead;
    const unsigned long colorButton = colors.button;
    const unsigned long colorButtonActive = colors.buttonActive;
    const unsigned long colorButtonLabel = colors.buttonLabel;
    const unsigned long colorButtonLabelActive = colors.buttonLabelActive;
    const unsigned long colorSelectionText = colors.selectionText;
    const unsigned long colorCursorText = colors.cursorText;
    const unsigned long colorPlayheadText = colors.playheadText;
    const unsigned long colorActiveTagText = colors.activeTagText;
    const Drawable drawTarget = context.trackerBackbuffer != 0 ? context.trackerBackbuffer : context.window;

    XSetForeground(context.display, context.gc, colorBackground);
    XFillRectangle(
        context.display,
        drawTarget,
        context.gc,
        0,
        0,
        static_cast<unsigned int>(context.windowWidth),
        static_cast<unsigned int>(context.windowHeight));

    auto drawFilledRect = [&](int x, int y, int w, int h, unsigned long color) {
        XSetForeground(context.display, context.gc, color);
        XFillRectangle(context.display, drawTarget, context.gc, x, y, static_cast<unsigned int>(w), static_cast<unsigned int>(h));
    };
    auto drawRect = [&](int x, int y, int w, int h, unsigned long color) {
        XSetForeground(context.display, context.gc, color);
        XDrawRectangle(context.display, drawTarget, context.gc, x, y, static_cast<unsigned int>(w), static_cast<unsigned int>(h));
    };
    auto drawText = [&](int x, int y, const std::string& text, unsigned long color) {
        XSetForeground(context.display, context.gc, color);
        XDrawString(context.display, drawTarget, context.gc, x, y, text.c_str(), static_cast<int>(text.size()));
    };
    auto controlTextBaseline = [&](int y, int h) {
        const int ascent = context.uiFont != nullptr ? context.uiFont->ascent : 9;
        const int descent = context.uiFont != nullptr ? context.uiFont->descent : 3;
        const int textHeight = ascent + descent;
        const int topPad = std::max(0, (h - textHeight) / 2);
        return y + topPad + ascent;
    };
    auto drawButton = [&](const UiRect& rect, const std::string& label, bool active) {
        drawFilledRect(rect.x, rect.y, rect.width, rect.height, active ? colorButtonActive : colorButton);
        drawRect(rect.x, rect.y, rect.width, rect.height, colorGridLine);
        drawText(
            rect.x + 8,
            controlTextBaseline(rect.y, rect.height),
            label,
            active ? colorButtonLabelActive : colorButtonLabel);
    };
    auto textWidth = [&](const std::string& text) {
        if (context.uiFont != nullptr) {
            return XTextWidth(context.uiFont, text.c_str(), static_cast<int>(text.size()));
        }
        return static_cast<int>(text.size()) * 8;
    };
    auto fitText = [&](const std::string& text, int maxWidth) {
        if (maxWidth <= 10 || textWidth(text) <= maxWidth) {
            return text;
        }
        const std::string ellipsis = "...";
        std::string trimmed = text;
        while (!trimmed.empty() && textWidth(trimmed + ellipsis) > maxWidth) {
            trimmed.pop_back();
        }
        return trimmed.empty() ? ellipsis : (trimmed + ellipsis);
    };
    const AppSessionSnapshot snap = context.activeSnapshot();
    const PlaybackSnapshot playback = context.session.playback().snapshot();

    drawMainWindowContent(
        GuiMainWindowContentDrawContext {
            snap,
            playback,
            context.audioRuntime,
            context.windowWidth,
            context.windowHeight,
            margin,
            headerHeight,
            statusHeight,
            lineHeight,
            gridTop,
            gridHeight,
            sidebarWidth,
            gridWidth,
            gridLeft,
            sidebarLeft,
            context.gridTrackStart,
            context.requestedRowCount,
            context.pointerX,
            context.pointerY,
            context.hoveredTrackHeader,
            colorBackground,
            colorPanel,
            colorGridHeader,
            colorGridLine,
            colorSelection,
            colorCursor,
            colorText,
            colorMutedText,
            colorPlayhead,
            colorButton,
            colorButtonActive,
            colorButtonLabel,
            colorButtonLabelActive,
            colorSelectionText,
            colorCursorText,
            colorPlayheadText,
            colorActiveTagText,
            theme,
            context.layout,
            context.synthWindowVisible,
            context.audioTuningDialogActive,
            context.selectedOrderIndex,
            context.armedInstrument,
            context.armedOctave,
            context.defaultVelocity,
            context.stepAdvance,
            context.followPlayback,
            context.themeMode,
            context.activePatternRows,
            context.paintNoteMidi,
            context.lastAction,
            context.targetSongLengthMinutes,
            context.midiImportRowsPerBeat,
            context.midiImportPatternRows,
            context.midiImportSplitByTrack,
            context.instrumentListVisibleRows,
            context.instrumentListStart,
            context.clampInstrumentListWindow,
            context.transportButtons,
            context.fileButtons,
            context.themeButtons,
            context.audioPerformanceButtons,
            context.orderSlotHits,
            context.trackHeaderHits,
            context.octaveHitTargets,
            context.pianoKeyHits,
            context.trackMetadataHits,
            context.songLengthHits,
            context.midiImportSettingHits,
            context.instrumentControlHits,
            context.instrumentHitTargets,
            context.instrumentListRect,
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
            context.patternRowsMinus,
            context.patternRowsValue,
            context.patternRowsPlus,
            context.stepAdvanceButton,
            context.followPlaybackButton,
            context.gridTrackPrevButton,
            context.gridTrackNextButton,
            context.sidebarViewport,
            context.sidebarScrollOffset,
            context.sidebarContentHeight,
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
            context.inlinePromptButtonsVisible,
            context.inlinePrompt,
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
            context.audioTuningDialogRect,
            context.audioTuningDialogHits,
            context.filteredInstrumentIndices,
            drawFilledRect,
            drawRect,
            drawText,
            drawButton,
            controlTextBaseline,
            fitText,
            textWidth,
            [&](int x, int y, int w, int h) {
                XRectangle clip;
                clip.x = static_cast<short>(x);
                clip.y = static_cast<short>(y);
                clip.width = static_cast<unsigned short>(std::max(0, w));
                clip.height = static_cast<unsigned short>(std::max(0, h));
                XSetClipRectangles(context.display, context.gc, 0, 0, &clip, 1, Unsorted);
            },
            [&]() { XSetClipMask(context.display, context.gc, None); }});

    if (context.trackerBackbuffer != 0) {
        XCopyArea(
            context.display,
            context.trackerBackbuffer,
            context.window,
            context.gc,
            0,
            0,
            static_cast<unsigned int>(context.windowWidth),
            static_cast<unsigned int>(context.windowHeight),
            0,
            0);
    }
    XFlush(context.display);
}

} // namespace arachno
