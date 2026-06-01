#include "ui/gui/GuiMainWindowEventDispatchOps.h"

namespace arachno {

bool dispatchMainWindowEvent(const GuiMainWindowEventDispatchContext& context, const XEvent& event) {
    const GuiSynthWindowEventResult synthEventResult = context.dispatchSynthWindowEvent(event);
    if (synthEventResult.consumed) {
        if (synthEventResult.needsRedraw) {
            context.needsRedraw = true;
        }
        if (synthEventResult.synthWindowNeedsRedraw) {
            context.synthWindowNeedsRedraw = true;
        }
        return true;
    }

    if (event.xany.window != context.mainWindow) {
        return true;
    }

    const GuiMainEventScaffoldDispatchResult scaffoldResult = context.dispatchScaffoldEvent(event);
    if (scaffoldResult.consumed) {
        if (scaffoldResult.needsRedraw) {
            context.needsRedraw = true;
        }
        if (scaffoldResult.shouldQuit) {
            context.running = false;
        }
        return true;
    }

    if (event.type == ButtonPress) {
        const GuiMainButtonPressDispatchResult buttonPressResult = context.dispatchButtonPressEvent(event.xbutton);
        if (buttonPressResult.consumed) {
            if (buttonPressResult.needsRedraw) {
                context.needsRedraw = true;
            }
            return true;
        }
    }

    if (event.type != KeyPress) {
        return true;
    }

    const GuiMainKeyPressDispatchResult keyPressResult = context.dispatchKeyPressEvent(event.xkey);
    if (keyPressResult.consumed) {
        if (keyPressResult.needsRedraw) {
            context.needsRedraw = true;
        }
        if (keyPressResult.synthWindowNeedsRedraw) {
            context.synthWindowNeedsRedraw = true;
        }
        if (keyPressResult.shouldQuit) {
            context.running = false;
        }
        return true;
    }

    return false;
}

} // namespace arachno
