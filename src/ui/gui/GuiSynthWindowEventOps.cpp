#include "ui/gui/GuiSynthWindowEventOps.h"

#include <algorithm>

#include <X11/X.h>

#include "ui/gui/GuiSynthInlinePromptEvents.h"
#include "ui/gui/GuiSynthMotionOps.h"

namespace arachno {

GuiSynthWindowEventResult handleSynthWindowEvent(
    const GuiSynthWindowEventContext& context,
    const XEvent& event) {
    GuiSynthWindowEventResult result;

    if (event.xany.window != context.synthWindow || context.synthWindow == 0) {
        return result;
    }
    result.consumed = true;

    if (event.type == Expose) {
        result.synthWindowNeedsRedraw = true;
        return result;
    }
    if (event.type == ConfigureNotify) {
        context.synthWindowWidth = std::max(context.synthWindowMinWidth, event.xconfigure.width);
        context.synthWindowHeight = std::max(context.synthWindowMinHeight, event.xconfigure.height);
        if (event.xconfigure.width < context.synthWindowMinWidth
            || event.xconfigure.height < context.synthWindowMinHeight) {
            XResizeWindow(
                context.display,
                context.synthWindow,
                static_cast<unsigned int>(context.synthWindowWidth),
                static_cast<unsigned int>(context.synthWindowHeight));
        }
        context.ensureSynthBackbuffer();
        result.synthWindowNeedsRedraw = true;
        return result;
    }
    if (event.type == ClientMessage) {
        if (static_cast<Atom>(event.xclient.data.l[0]) == context.wmDelete) {
            context.setSynthWindowVisible(false);
        }
        return result;
    }
    if (event.type == ButtonPress) {
        const bool shiftDown = (event.xbutton.state & ShiftMask) != 0;
        context.synthPointerX = event.xbutton.x;
        context.synthPointerY = event.xbutton.y;
        if (event.xbutton.button == Button1) {
            context.synthParamDragActive = false;
            context.synthParamDragKnob = false;
            context.synthParamDragDirty = false;
            context.synthTooltipParam.clear();
        }
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
            if (handleSynthInlinePromptButtonPress(
                    promptEventContext,
                    event.xbutton.button,
                    event.xbutton.x,
                    event.xbutton.y)) {
                result.synthWindowNeedsRedraw = true;
                result.needsRedraw = true;
                return result;
            }
        }
        if (event.xbutton.button == Button4 || event.xbutton.button == Button5) {
            if (context.synthParamViewport.contains(event.xbutton.x, event.xbutton.y)) {
                const int deltaRows = shiftDown ? 8 : 3;
                const int direction = event.xbutton.button == Button4 ? -1 : 1;
                const int delta = direction * deltaRows * 24;
                const int maxScroll = std::max(0, context.synthParamContentHeight - std::max(1, context.synthParamViewport.height));
                context.synthParamScroll = std::clamp(context.synthParamScroll + delta, 0, maxScroll);
                result.synthWindowNeedsRedraw = true;
                result.needsRedraw = true;
                return result;
            }
        }
        if (event.xbutton.button == Button1) {
            context.synthPointerDown = false;
            context.synthLastPointerMidi = -1;
            if (context.triggerSynthKeyboardPointer(event.xbutton.x, event.xbutton.y, false)) {
                context.synthPointerDown = true;
                result.needsRedraw = true;
                result.synthWindowNeedsRedraw = true;
                return result;
            }
            if (context.handleSynthWindowClick(event.xbutton.x, event.xbutton.y)) {
                result.needsRedraw = true;
                result.synthWindowNeedsRedraw = true;
            }
        }
        return result;
    }
    if (event.type == ButtonRelease) {
        if (event.xbutton.button == Button1) {
            // Releasing the pointer ends the sustained virtual-piano note.
            if (context.synthLastPointerMidi >= 0 && context.noteOffSynthPreviewMidi) {
                context.noteOffSynthPreviewMidi(context.synthLastPointerMidi);
            }
            context.synthPointerDown = false;
            context.synthLastPointerMidi = -1;
            if (context.synthParamDragDirty) {
                context.refreshSnapshot();
                context.synthParamDragDirty = false;
            }
            context.synthParamDragActive = false;
            context.synthParamDragKnob = false;
        }
        return result;
    }
    if (event.type == MotionNotify) {
        GuiSynthMotionContext motionContext {
            context.synthPointerX,
            context.synthPointerY,
            context.synthWindowHits,
            context.synthTooltipParam,
            context.synthTooltipHoverSince,
            context.inlinePrompt.active && context.isSynthInlinePromptKind(context.inlinePrompt.kind),
            context.synthParamDragActive,
            context.synthParamDragKnob,
            context.synthParamDragName,
            context.synthParamDragRect,
            context.synthParamDragStartX,
            context.synthParamDragStartY,
            context.synthParamDragStartValue,
            context.synthParamDragLastValue,
            context.synthParamDragDirty,
            context.synthPointerDown,
            context.clampInstrumentIndex,
            context.findSynthParamDef,
            context.setSynthParameter,
            context.triggerSynthKeyboardPointer};
        const GuiSynthMotionResult motionResult = handleSynthWindowMotion(
            motionContext,
            event.xmotion.x,
            event.xmotion.y,
            static_cast<unsigned int>(event.xmotion.state));
        result.needsRedraw = motionResult.needsRedraw;
        result.synthWindowNeedsRedraw = motionResult.synthWindowNeedsRedraw;
        return result;
    }
    if (event.type == LeaveNotify) {
        context.synthTooltipParam.clear();
        context.synthTooltipHoverSince = std::chrono::steady_clock::time_point {};
        result.synthWindowNeedsRedraw = true;
        result.needsRedraw = true;
        return result;
    }
    if (event.type == KeyRelease) {
        GuiSynthKeyContext keyContext = context.makeSynthKeyContext();
        const GuiSynthKeyResult keyResult = handleSynthWindowKeyRelease(
            keyContext,
            event.xkey.keycode,
            context.isAutoRepeatRelease(event));
        result.needsRedraw = keyResult.needsRedraw;
        result.synthWindowNeedsRedraw = keyResult.synthWindowNeedsRedraw;
        return result;
    }
    if (event.type == KeyPress) {
        GuiSynthKeyContext keyContext = context.makeSynthKeyContext();
        XKeyEvent keyEvent = event.xkey;
        KeySym key = XLookupKeysym(&keyEvent, 0);
        const GuiSynthKeyResult keyResult = handleSynthWindowKeyPress(keyContext, key, event.xkey);
        result.needsRedraw = keyResult.needsRedraw;
        result.synthWindowNeedsRedraw = keyResult.synthWindowNeedsRedraw;
        return result;
    }
    return result;
}

} // namespace arachno
