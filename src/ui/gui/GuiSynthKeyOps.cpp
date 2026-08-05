#include "ui/gui/GuiSynthKeyOps.h"

#include <algorithm>

#include <X11/keysym.h>
#include <X11/Xutil.h>

#include "GuiInput.h"
#include "ui/gui/GuiSynthInlinePromptEvents.h"

namespace arachno {

GuiSynthKeyResult handleSynthWindowKeyPress(
    GuiSynthKeyContext& context,
    KeySym key,
    const XKeyEvent& keyEvent) {
    GuiSynthKeyResult result;
    result.consumed = true;
    result.needsRedraw = true;
    result.synthWindowNeedsRedraw = true;

    const bool ctrlDown = (keyEvent.state & ControlMask) != 0;
    const bool altDown = (keyEvent.state & Mod1Mask) != 0;
    char lookupBuffer[16];
    XKeyEvent lookupEvent = keyEvent;
    const int lookupCount = XLookupString(
        &lookupEvent,
        lookupBuffer,
        static_cast<int>(sizeof(lookupBuffer)),
        nullptr,
        nullptr);

    if (context.inlinePrompt.active && context.isSynthInlinePromptKind(context.inlinePrompt.kind)) {
        GuiSynthInlinePromptEventContext promptEventContext {
            context.inlinePrompt,
            context.inlinePromptUsesFileBrowser,
            context.synthInlinePromptButtonsVisible,
            context.synthInlinePromptAcceptButton,
            context.synthInlinePromptCancelButton,
            context.fileBrowserHits,
            context.fileBrowserListRect,
            context.fileBrowserScroll,
            context.fileBrowserSelected,
            context.fileBrowserEntries,
            context.fileBrowserDirectory,
            context.refreshFileBrowserEntries,
            context.executeInlinePrompt,
            context.cancelInlinePrompt};
        if (handleSynthInlinePromptKeyPress(
                promptEventContext,
                key,
                ctrlDown,
                altDown,
                lookupBuffer,
                lookupCount)) {
            return result;
        }
    }

    auto setSynthOctaveFromShortcut = [&](int octave) {
        context.setArmedOctave(octave);
        context.synthPreviewMidi = std::clamp(((context.armedOctave + 1) * 12) + (context.synthPreviewMidi % 12), 0, 127);
        context.paintNoteMidi = context.synthPreviewMidi;
        context.ensureSynthKeyboardShowsMidi(context.synthPreviewMidi);
    };

    if (key == XK_Escape) {
        context.setSynthWindowVisible(false);
    } else if (key == XK_Left || key == XK_Up) {
        context.cycleInstrument(-1);
    } else if (key == XK_Right || key == XK_Down) {
        context.cycleInstrument(1);
    } else if (key == XK_space || key == XK_Return) {
        if (context.claimSynthPreviewKey(keyEvent.keycode, context.synthPreviewMidi)) {
            context.auditionSynthPreviewMidi(context.synthPreviewMidi);
        }
    } else {
        int midiNote = 0;
        if (!ctrlDown && trackerKeyToMidi(key, context.armedOctave, midiNote)) {
            if (context.claimSynthPreviewKey(keyEvent.keycode, midiNote)) {
                context.auditionSynthPreviewMidi(midiNote);
            }
        } else if (ctrlDown) {
            const int octaveDigit = digitKeyToInt(key);
            if (octaveDigit >= 0 && octaveDigit <= 8) {
                setSynthOctaveFromShortcut(octaveDigit);
            }
        } else if (key == XK_minus || key == XK_KP_Subtract
            || key == XK_bracketleft
            || key == XK_Page_Down
            || key == XK_KP_Page_Down) {
            setSynthOctaveFromShortcut(context.armedOctave - 1);
        } else if (key == XK_equal || key == XK_plus || key == XK_KP_Add
            || key == XK_bracketright
            || key == XK_Page_Up
            || key == XK_KP_Page_Up) {
            setSynthOctaveFromShortcut(context.armedOctave + 1);
        }
    }
    return result;
}

GuiSynthKeyResult handleSynthWindowKeyRelease(
    const GuiSynthKeyContext& context,
    unsigned int keycode,
    bool autoRepeatRelease) {
    GuiSynthKeyResult result;
    result.consumed = true;
    if (!autoRepeatRelease) {
        context.releaseSynthPreviewKey(keycode);
    }
    return result;
}

} // namespace arachno
