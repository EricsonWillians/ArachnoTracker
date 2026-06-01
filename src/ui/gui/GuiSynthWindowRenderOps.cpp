#include "ui/gui/GuiSynthWindowRenderOps.h"

#include <algorithm>
#include <cmath>

#include <X11/Xutil.h>

namespace arachno {

void drawSynthWindowFromWindowState(const GuiSynthWindowRenderContext& context) {
    if (!context.synthWindowVisible || context.synthWindow == 0 || context.synthGc == nullptr) {
        return;
    }

    context.ensureSynthBackbuffer();
    const UiThemePalette& theme =
        context.themeMode == GuiThemeMode::HighContrast ? context.highContrastTheme : context.dosTheme;
    const Drawable synthDrawTarget = context.synthBackbuffer != 0 ? context.synthBackbuffer : context.synthWindow;

    auto drawFilledRect = [&](int x, int y, int w, int h, unsigned long color) {
        XSetForeground(context.display, context.synthGc, color);
        XFillRectangle(
            context.display,
            synthDrawTarget,
            context.synthGc,
            x,
            y,
            static_cast<unsigned int>(w),
            static_cast<unsigned int>(h));
    };
    auto drawRect = [&](int x, int y, int w, int h, unsigned long color) {
        XSetForeground(context.display, context.synthGc, color);
        XDrawRectangle(
            context.display,
            synthDrawTarget,
            context.synthGc,
            x,
            y,
            static_cast<unsigned int>(w),
            static_cast<unsigned int>(h));
    };
    auto drawText = [&](int x, int y, const std::string& text, unsigned long color) {
        XSetForeground(context.display, context.synthGc, color);
        XDrawString(
            context.display,
            synthDrawTarget,
            context.synthGc,
            x,
            y,
            text.c_str(),
            static_cast<int>(text.size()));
    };
    auto drawButton = [&](const UiRect& rect, const std::string& label, bool active) {
        drawFilledRect(rect.x, rect.y, rect.width, rect.height, active ? theme.buttonActive : theme.button);
        drawRect(rect.x, rect.y, rect.width, rect.height, theme.gridLine);
        drawText(rect.x + 5, rect.y + 14, label, active ? theme.buttonLabelActive : theme.buttonLabel);
    };
    auto drawKnob = [&](const UiRect& rect, double ratio, bool active) {
        const int cx = rect.x + (rect.width / 2);
        const int cy = rect.y + (rect.height / 2);
        const int radiusOuter = std::max(6, std::min(rect.width, rect.height) / 2 - 1);
        const int radiusInner = std::max(3, radiusOuter - 4);
        const int diamOuter = radiusOuter * 2;
        const int diamInner = radiusInner * 2;
        XSetForeground(context.display, context.synthGc, active ? theme.selection : theme.gridLine);
        XFillArc(
            context.display,
            synthDrawTarget,
            context.synthGc,
            cx - radiusOuter,
            cy - radiusOuter,
            static_cast<unsigned int>(diamOuter),
            static_cast<unsigned int>(diamOuter),
            0,
            360 * 64);
        XSetForeground(context.display, context.synthGc, theme.panel);
        XFillArc(
            context.display,
            synthDrawTarget,
            context.synthGc,
            cx - radiusInner,
            cy - radiusInner,
            static_cast<unsigned int>(diamInner),
            static_cast<unsigned int>(diamInner),
            0,
            360 * 64);
        const int startDeg = 225;
        const int spanDeg = static_cast<int>(std::lround(std::clamp(ratio, 0.0, 1.0) * 270.0));
        XSetForeground(context.display, context.synthGc, theme.buttonActive);
        XDrawArc(
            context.display,
            synthDrawTarget,
            context.synthGc,
            cx - radiusOuter,
            cy - radiusOuter,
            static_cast<unsigned int>(diamOuter),
            static_cast<unsigned int>(diamOuter),
            startDeg * 64,
            -spanDeg * 64);
        const double pointerDeg = static_cast<double>(startDeg) - (std::clamp(ratio, 0.0, 1.0) * 270.0);
        const double pointerRad = pointerDeg * 3.14159265358979323846 / 180.0;
        const int pointerLen = std::max(3, radiusInner - 1);
        const int px = cx + static_cast<int>(std::lround(std::cos(pointerRad) * pointerLen));
        const int py = cy - static_cast<int>(std::lround(std::sin(pointerRad) * pointerLen));
        XSetForeground(context.display, context.synthGc, theme.text);
        XDrawLine(context.display, synthDrawTarget, context.synthGc, cx, cy, px, py);
        XDrawPoint(context.display, synthDrawTarget, context.synthGc, px, py);
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

    drawSynthWindowContent(
        GuiSynthWindowDrawContext {
            context.session,
            context.synthWindowWidth,
            context.synthWindowHeight,
            context.defaultVelocity,
            context.synthPreviewMidi,
            context.midiStatusText,
            context.synthWindowHits,
            context.synthKeyboardHits,
            context.fileBrowserHits,
            context.fileBrowserListRect,
            context.synthParamViewport,
            context.synthParamContentHeight,
            context.synthKeyboardBaseOctave,
            context.synthKeyboardVisibleOctaves,
            context.synthParamPage,
            context.synthParamOscTarget,
            context.synthParamScroll,
            context.synthTooltipParam,
            context.synthPointerX,
            context.synthPointerY,
            context.synthTooltipHoverSince,
            context.synthParamDefs,
            context.inlinePrompt,
            context.isSynthInlinePromptKind,
            context.inlinePromptUsesFileBrowser,
            context.refreshFileBrowserEntries,
            context.fileBrowserDirectory,
            context.fileBrowserEntries,
            context.fileBrowserScroll,
            context.fileBrowserSelected,
            context.synthInlinePromptAcceptButton,
            context.synthInlinePromptCancelButton,
            context.synthInlinePromptButtonsVisible,
            context.synthScopeTrackedMidi,
            context.synthScopePhase,
            context.synthScopeLaneSynth,
            context.synthScopeLaneLeft,
            context.synthScopeLaneRight,
            context.synthScopeLaneActive,
            context.synthScopeLaneInstrument,
            context.synthScopeLaneMidi,
            context.synthScopeLaneGateSeconds,
            context.synthScopeRetriggerRequested,
            context.collectSynthPreviewNotes,
            {
                theme.background,
                theme.panel,
                theme.gridHeader,
                theme.gridLine,
                theme.text,
                theme.mutedText,
                theme.button,
                theme.buttonActive,
                theme.buttonLabel,
                theme.buttonLabelActive,
                theme.selection,
                theme.selectionText,
                theme.pianoWhite,
                theme.pianoBlack},
            context.scopeColors,
            context.clampInstrumentIndex,
            [&]() {
                if (context.synthBackbuffer != 0) {
                    XCopyArea(
                        context.display,
                        context.synthBackbuffer,
                        context.synthWindow,
                        context.synthGc,
                        0,
                        0,
                        static_cast<unsigned int>(context.synthWindowWidth),
                        static_cast<unsigned int>(context.synthWindowHeight),
                        0,
                        0);
                }
                XFlush(context.display);
            },
            drawFilledRect,
            drawRect,
            drawText,
            drawButton,
            drawKnob,
            fitText,
            textWidth,
            [&](unsigned long color) { XSetForeground(context.display, context.synthGc, color); },
            [&](int x1, int y1, int x2, int y2) {
                XDrawLine(context.display, synthDrawTarget, context.synthGc, x1, y1, x2, y2);
            },
            [&](int x, int y) { XDrawPoint(context.display, synthDrawTarget, context.synthGc, x, y); },
            [&](int x, int y, int width, int height) {
                XRectangle clipRect {
                    static_cast<short>(x),
                    static_cast<short>(y),
                    static_cast<unsigned short>(width),
                    static_cast<unsigned short>(height)};
                XSetClipRectangles(context.display, context.synthGc, 0, 0, &clipRect, 1, Unsorted);
            },
            [&]() { XSetClipMask(context.display, context.synthGc, None); }});
}

} // namespace arachno
