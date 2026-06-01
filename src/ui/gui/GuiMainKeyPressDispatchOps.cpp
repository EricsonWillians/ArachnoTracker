#include "ui/gui/GuiMainKeyPressDispatchOps.h"

#include <X11/keysym.h>
#include <X11/Xutil.h>

#include "GuiInput.h"

namespace arachno {

GuiMainKeyPressDispatchResult dispatchMainKeyPress(const GuiMainKeyPressDispatchContext& context) {
    XKeyEvent keyEvent = context.event;
    KeySym key0 = XLookupKeysym(&keyEvent, 0);
    KeySym key1 = XLookupKeysym(&keyEvent, 1);
    KeySym key = NoSymbol;
    char lookupBuffer[16];
    const int lookupCount = XLookupString(
        &keyEvent,
        lookupBuffer,
        static_cast<int>(sizeof(lookupBuffer)),
        &key,
        nullptr);
    if (key == NoSymbol) {
        key = key0;
    }
    if (key == NoSymbol) {
        key = key1;
    }
    auto keyMatches = [key, key0, key1](KeySym target) {
        return key == target || key0 == target || key1 == target;
    };
    auto resolvedDigit = [key, key0, key1]() -> int {
        int digit = digitKeyToInt(key);
        if (digit < 0) {
            digit = digitKeyToInt(key0);
        }
        if (digit < 0) {
            digit = digitKeyToInt(key1);
        }
        return digit;
    };
    const bool ctrlDown = (context.event.state & ControlMask) != 0;
    const bool shiftDown = (context.event.state & ShiftMask) != 0;
    const bool altDown = (context.event.state & Mod1Mask) != 0;

    const GuiMainKeyModalResult keyModalResult = context.dispatchModal(
        key,
        ctrlDown,
        shiftDown,
        altDown,
        lookupCount,
        lookupBuffer,
        keyMatches,
        resolvedDigit,
        context.playbackSampleRate());
    if (keyModalResult.consumed) {
        return GuiMainKeyPressDispatchResult {
            true,
            keyModalResult.needsRedraw,
            false,
            false};
    }

    if (key == XK_Escape || (ctrlDown && normalizeLetterKey(key) == XK_q)) {
        return GuiMainKeyPressDispatchResult {true, false, false, true};
    }

    const GuiMainKeyNoteResult keyNoteResult = context.dispatchNote(
        key,
        context.event.keycode,
        ctrlDown,
        shiftDown,
        altDown,
        keyMatches,
        resolvedDigit);
    if (keyNoteResult.consumed) {
        return GuiMainKeyPressDispatchResult {
            true,
            keyNoteResult.needsRedraw,
            keyNoteResult.synthWindowNeedsRedraw,
            false};
    }

    const GuiMainKeyCommandResult keyCommandResult = context.dispatchCommand(
        key,
        ctrlDown,
        shiftDown,
        altDown,
        keyMatches);
    if (keyCommandResult.consumed) {
        return GuiMainKeyPressDispatchResult {true, keyCommandResult.needsRedraw, false, false};
    }

    const GuiMainKeyEditResult keyEditResult = context.dispatchEdit(
        key,
        ctrlDown,
        shiftDown,
        altDown,
        keyMatches,
        resolvedDigit);
    if (keyEditResult.consumed) {
        return GuiMainKeyPressDispatchResult {
            true,
            keyEditResult.needsRedraw,
            keyEditResult.synthWindowNeedsRedraw,
            false};
    }

    return GuiMainKeyPressDispatchResult {};
}

} // namespace arachno
