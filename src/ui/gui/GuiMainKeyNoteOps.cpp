#include "ui/gui/GuiMainKeyNoteOps.h"

#include <algorithm>

#include <X11/keysym.h>

#include "GuiInput.h"

namespace arachno {

GuiMainKeyNoteResult handleMainKeyNoteOps(const GuiMainKeyNoteContext& context) {
    GuiMainKeyNoteResult result;

    if (context.synthWindowVisible && !context.ctrlDown && !context.altDown) {
        auto setSynthOctaveFromShortcut = [&](int octave) {
            context.setArmedOctave(octave);
            context.synthPreviewMidi = std::clamp(((context.armedOctave + 1) * 12) + (context.synthPreviewMidi % 12), 0, 127);
            context.paintNoteMidi = context.synthPreviewMidi;
            context.ensureSynthKeyboardShowsMidi(context.synthPreviewMidi);
            result.synthWindowNeedsRedraw = true;
        };

        int midiNote = 0;
        if (trackerKeyToMidi(context.key, context.armedOctave, midiNote)) {
            if (context.claimSynthPreviewKey(context.keycode, midiNote)) {
                context.auditionSynthPreviewMidi(midiNote);
            }
            result.consumed = true;
            result.needsRedraw = true;
            return result;
        }
        if (context.keyMatches(XK_Return) || context.keyMatches(XK_KP_Enter) || context.key == XK_space) {
            if (context.claimSynthPreviewKey(context.keycode, context.synthPreviewMidi)) {
                context.auditionSynthPreviewMidi(context.synthPreviewMidi);
            }
            result.consumed = true;
            result.needsRedraw = true;
            return result;
        }
        if (context.key == XK_minus || context.key == XK_KP_Subtract) {
            setSynthOctaveFromShortcut(context.armedOctave - 1);
            result.consumed = true;
            result.needsRedraw = true;
            return result;
        }
        if (context.key == XK_equal || context.key == XK_plus || context.key == XK_KP_Add) {
            setSynthOctaveFromShortcut(context.armedOctave + 1);
            result.consumed = true;
            result.needsRedraw = true;
            return result;
        }
        if (context.key == XK_bracketleft || context.key == XK_Page_Down || context.key == XK_KP_Page_Down) {
            setSynthOctaveFromShortcut(context.armedOctave - 1);
            result.consumed = true;
            result.needsRedraw = true;
            return result;
        }
        if (context.key == XK_bracketright || context.key == XK_Page_Up || context.key == XK_KP_Page_Up) {
            setSynthOctaveFromShortcut(context.armedOctave + 1);
            result.consumed = true;
            result.needsRedraw = true;
            return result;
        }
    }

    if (context.shiftDown && !context.ctrlDown && !context.altDown) {
        const int digit = context.resolvedDigit();
        if (digit >= 0) {
            const int mapped = digit == 0 ? 9 : digit - 1;
            context.selectInstrument(mapped);
            result.consumed = true;
            result.needsRedraw = true;
            return result;
        }
    }

    if (context.altDown && !context.ctrlDown && !context.shiftDown) {
        const int digit = context.resolvedDigit();
        if (digit >= 0) {
            const int count = context.instrumentCount();
            if (count > 0) {
                const int mapped = std::clamp(digit, 0, count - 1);
                context.selectInstrument(mapped);
                result.consumed = true;
                result.needsRedraw = true;
                return result;
            }
        }
    }

    if (!context.synthWindowVisible && !context.ctrlDown && !context.altDown && context.key == XK_Caps_Lock) {
        (void)context.applyTrackerNoteOffAtCursor(context.stepAdvance);
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }

    if (!context.ctrlDown && !context.altDown) {
        int midiNote = 0;
        if (trackerKeyToMidi(context.key, context.armedOctave, midiNote)) {
            context.paintNoteMidi = midiNote;
            const int count = context.instrumentCount();
            if (count > 0) {
                context.armedInstrument = std::clamp(context.armedInstrument, 0, count - 1);
                (void)context.applyTrackerNoteAtCursor(context.armedInstrument, midiNote, context.stepAdvance);
            }
            result.consumed = true;
            result.needsRedraw = true;
            return result;
        }
    }

    return result;
}

} // namespace arachno
