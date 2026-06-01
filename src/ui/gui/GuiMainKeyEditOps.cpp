#include "ui/gui/GuiMainKeyEditOps.h"

#include <algorithm>

#include <X11/keysym.h>

#include "GuiInput.h"

namespace arachno {

GuiMainKeyEditResult handleMainKeyEditOps(const GuiMainKeyEditContext& context) {
    GuiMainKeyEditResult result;

    if (context.ctrlDown) {
        const int octaveDigit = context.resolvedDigit();
        if (octaveDigit >= 0 && octaveDigit <= 8) {
            context.setArmedOctave(octaveDigit);
            if (!context.synthWindowVisible) {
                context.applyArmedOctaveToSelection();
            } else {
                context.synthPreviewMidi = std::clamp((context.armedOctave * 12) + (context.synthPreviewMidi % 12), 0, 127);
                context.paintNoteMidi = context.synthPreviewMidi;
                context.ensureSynthKeyboardShowsMidi(context.synthPreviewMidi);
                result.synthWindowNeedsRedraw = true;
            }
            result.consumed = true;
            result.needsRedraw = true;
            return result;
        }
    }

    if (context.ctrlDown && normalizeLetterKey(context.key) == XK_c) {
        context.runActionById("editor.selection.copy");
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && normalizeLetterKey(context.key) == XK_x) {
        context.runActionById("editor.selection.cut");
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && !context.shiftDown && normalizeLetterKey(context.key) == XK_v) {
        context.runActionById("editor.selection.paste");
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && normalizeLetterKey(context.key) == XK_a) {
        context.selectAll();
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }

    if (context.key == XK_Tab) {
        context.stepAdvance = !context.stepAdvance;
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.keyMatches(XK_Return) || context.keyMatches(XK_KP_Enter)) {
        const std::pair<int, int> cursor = context.currentCursor();
        context.paintNoteAt(cursor.first, cursor.second, context.paintNoteMidi);
        if (context.stepAdvance) {
            context.ensurePatternRowsForRow(cursor.first + 1);
            context.runActionById("editor.navigation.down");
        } else {
            context.refreshSnapshot();
        }
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.key == XK_minus || context.key == XK_KP_Subtract) {
        context.setArmedOctave(context.armedOctave - 1);
        if (!context.synthWindowVisible) {
            context.applyArmedOctaveToSelection();
        }
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.key == XK_equal || context.key == XK_plus || context.key == XK_KP_Add) {
        context.setArmedOctave(context.armedOctave + 1);
        if (!context.synthWindowVisible) {
            context.applyArmedOctaveToSelection();
        }
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.key == XK_comma) {
        context.defaultVelocity = std::max(0.05f, context.defaultVelocity - 0.05f);
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.key == XK_period) {
        context.defaultVelocity = std::min(1.0f, context.defaultVelocity + 0.05f);
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.key == XK_bracketleft || context.key == XK_bracketright) {
        const int count = std::max(0, context.currentInstrumentCount());
        if (count > 0) {
            if (context.key == XK_bracketleft) {
                context.cycleInstrument(-1);
            } else {
                context.cycleInstrument(1);
            }
        }
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.key == XK_backslash) {
        context.auditionArmedInstrument();
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.altDown) {
        const int digit = context.resolvedDigit();
        if (digit >= 0) {
            context.selectInstrumentIndex(digit);
            result.consumed = true;
            result.needsRedraw = true;
            return result;
        }
    }
    if (context.key == XK_BackSpace || context.key == XK_Delete) {
        context.keyboardSelectionActive = false;
        context.runActionById("editor.step.clear");
        if (context.stepAdvance) {
            const std::pair<int, int> cursor = context.currentCursor();
            context.ensurePatternRowsForRow(cursor.first + 1);
            context.runActionById("editor.navigation.down");
        }
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }

    auto applyKeyboardRangeSelection = [&]() {
        const std::pair<int, int> cursor = context.currentCursor();
        context.applySelectionRange(
            context.keyboardSelectionAnchorRow,
            context.keyboardSelectionAnchorTrack,
            cursor.first,
            cursor.second);
    };

    auto moveByArrow = [&](const std::string& actionId) {
        if (actionId == "editor.navigation.down") {
            const std::pair<int, int> cursor = context.currentCursor();
            context.ensurePatternRowsForRow(cursor.first + 1);
        }
        if (context.shiftDown) {
            const std::pair<int, int> before = context.currentCursor();
            if (!context.keyboardSelectionActive) {
                context.keyboardSelectionAnchorRow = before.first;
                context.keyboardSelectionAnchorTrack = before.second;
                context.keyboardSelectionActive = true;
            }
            context.runActionById(actionId);
            applyKeyboardRangeSelection();
        } else {
            context.keyboardSelectionActive = false;
            context.runActionById(actionId);
        }
        result.consumed = true;
        result.needsRedraw = true;
    };

    if (context.keyMatches(XK_Up) || context.keyMatches(XK_KP_Up)) {
        moveByArrow("editor.navigation.up");
        return result;
    }
    if (context.keyMatches(XK_Down) || context.keyMatches(XK_KP_Down)) {
        moveByArrow("editor.navigation.down");
        return result;
    }
    if (context.keyMatches(XK_Left) || context.keyMatches(XK_KP_Left)) {
        moveByArrow("editor.navigation.left");
        return result;
    }
    if (context.keyMatches(XK_Right) || context.keyMatches(XK_KP_Right)) {
        moveByArrow("editor.navigation.right");
        return result;
    }
    if (context.key == XK_Home) {
        const std::pair<int, int> cursor = context.currentCursor();
        context.keyboardSelectionActive = false;
        context.moveCursor(0, cursor.second);
        context.viewStartRow = 0;
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.key == XK_End) {
        const std::pair<int, int> cursor = context.currentCursor();
        context.keyboardSelectionActive = false;
        context.moveCursor(std::max(0, context.activePatternRows - 1), cursor.second);
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.key == XK_Page_Up) {
        if (context.shiftDown) {
            const std::pair<int, int> before = context.currentCursor();
            if (!context.keyboardSelectionActive) {
                context.keyboardSelectionAnchorRow = before.first;
                context.keyboardSelectionAnchorTrack = before.second;
                context.keyboardSelectionActive = true;
            }
            const int targetRow = std::clamp(
                before.first - std::max(1, context.requestedRowCount),
                0,
                std::max(0, context.activePatternRows - 1));
            context.moveCursor(targetRow, before.second);
            applyKeyboardRangeSelection();
        } else {
            context.lockManualScroll();
            context.viewStartRow = std::max(0, context.viewStartRow - context.requestedRowCount);
            context.refreshSnapshot();
        }
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.key == XK_Page_Down) {
        if (context.shiftDown) {
            const std::pair<int, int> before = context.currentCursor();
            if (!context.keyboardSelectionActive) {
                context.keyboardSelectionAnchorRow = before.first;
                context.keyboardSelectionAnchorTrack = before.second;
                context.keyboardSelectionActive = true;
            }
            const int targetRow = std::clamp(
                before.first + std::max(1, context.requestedRowCount),
                0,
                std::max(0, context.activePatternRows - 1));
            context.moveCursor(targetRow, before.second);
            applyKeyboardRangeSelection();
        } else {
            context.lockManualScroll();
            context.viewStartRow = std::max(0, context.viewStartRow + context.requestedRowCount);
            context.refreshSnapshot();
        }
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }

    return result;
}

} // namespace arachno
