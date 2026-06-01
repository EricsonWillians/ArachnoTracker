#include "ui/gui/GuiSynthWindowLifecycleOps.h"

#include <algorithm>

namespace arachno {

bool ensureSynthWindowResources(const GuiSynthWindowLifecycleContext& context) {
    if (context.synthWindow != 0 && context.synthGc != nullptr) {
        return true;
    }
    if (context.synthWindow == 0) {
        context.synthWindow = XCreateSimpleWindow(
            context.display,
            RootWindow(context.display, context.screen),
            140,
            120,
            static_cast<unsigned int>(context.synthWindowWidth),
            static_cast<unsigned int>(context.synthWindowHeight),
            1,
            BlackPixel(context.display, context.screen),
            WhitePixel(context.display, context.screen));
        XStoreName(context.display, context.synthWindow, "ArachnoTracker Synth Designer");
        XSelectInput(
            context.display,
            context.synthWindow,
            ExposureMask
                | KeyPressMask
                | KeyReleaseMask
                | StructureNotifyMask
                | ButtonPressMask
                | ButtonReleaseMask
                | PointerMotionMask);
        Atom wmDelete = context.wmDelete;
        XSetWMProtocols(context.display, context.synthWindow, &wmDelete, 1);
    }
    if (context.synthGc == nullptr) {
        context.synthGc = XCreateGC(context.display, context.synthWindow, 0, nullptr);
        if (context.synthGc != nullptr) {
            XSetForeground(context.display, context.synthGc, BlackPixel(context.display, context.screen));
            XSetBackground(context.display, context.synthGc, WhitePixel(context.display, context.screen));
            if (context.uiFont != nullptr) {
                XSetFont(context.display, context.synthGc, context.uiFont->fid);
            }
        }
    }
    return context.synthWindow != 0 && context.synthGc != nullptr;
}

void releaseSynthBackbufferResources(const GuiSynthWindowLifecycleContext& context) {
    if (context.synthBackbuffer != 0) {
        XFreePixmap(context.display, context.synthBackbuffer);
        context.synthBackbuffer = 0;
    }
    context.synthBackbufferWidth = 0;
    context.synthBackbufferHeight = 0;
}

void ensureSynthBackbufferResources(const GuiSynthWindowLifecycleContext& context) {
    if (context.synthWindow == 0) {
        releaseSynthBackbufferResources(context);
        return;
    }
    const int targetWidth = std::max(1, context.synthWindowWidth);
    const int targetHeight = std::max(1, context.synthWindowHeight);
    if (context.synthBackbuffer != 0
        && context.synthBackbufferWidth == targetWidth
        && context.synthBackbufferHeight == targetHeight) {
        return;
    }
    releaseSynthBackbufferResources(context);
    context.synthBackbuffer = XCreatePixmap(
        context.display,
        context.synthWindow,
        static_cast<unsigned int>(targetWidth),
        static_cast<unsigned int>(targetHeight),
        static_cast<unsigned int>(DefaultDepth(context.display, context.screen)));
    if (context.synthBackbuffer != 0) {
        context.synthBackbufferWidth = targetWidth;
        context.synthBackbufferHeight = targetHeight;
    }
}

bool claimSynthPreviewKey(
    std::array<bool, 256>& synthPreviewKeyHeld,
    std::array<int, 256>& synthPreviewKeyMidi,
    unsigned int keycode,
    int midiNote) {
    if (keycode < synthPreviewKeyHeld.size()) {
        if (synthPreviewKeyHeld[keycode]) {
            return false;
        }
        synthPreviewKeyHeld[keycode] = true;
        synthPreviewKeyMidi[keycode] = std::clamp(midiNote, 0, 127);
    }
    return true;
}

void releaseSynthPreviewKey(
    std::array<bool, 256>& synthPreviewKeyHeld,
    std::array<int, 256>& synthPreviewKeyMidi,
    unsigned int keycode) {
    if (keycode < synthPreviewKeyHeld.size()) {
        synthPreviewKeyHeld[keycode] = false;
        synthPreviewKeyMidi[keycode] = -1;
    }
}

void clearSynthPreviewKeys(
    std::array<bool, 256>& synthPreviewKeyHeld,
    std::array<int, 256>& synthPreviewKeyMidi) {
    synthPreviewKeyHeld.fill(false);
    synthPreviewKeyMidi.fill(-1);
}

void clearSynthMidiPreviewNotes(std::array<bool, 128>& synthMidiPreviewHeld) {
    synthMidiPreviewHeld.fill(false);
}

void setSynthWindowVisible(const GuiSynthWindowLifecycleContext& context, bool visible) {
    if (visible) {
        if (!ensureSynthWindowResources(context)) {
            context.lastAction.ok = false;
            context.lastAction.actionId = "synth.window.open";
            context.lastAction.error = "failed to open synth designer window";
            return;
        }
        ensureSynthBackbufferResources(context);
        XMapRaised(context.display, context.synthWindow);
        context.synthWindowVisible = true;
        clearSynthPreviewKeys(context.synthPreviewKeyHeld, context.synthPreviewKeyMidi);
        clearSynthMidiPreviewNotes(context.synthMidiPreviewHeld);
        context.synthTooltipParam.clear();
        context.synthTooltipHoverSince = std::chrono::steady_clock::time_point {};
        context.synthScopeRetriggerRequested = true;
        context.synthKeyboardBaseOctave = std::clamp(
            context.armedOctave - 1,
            0,
            std::max(0, 10 - context.synthKeyboardVisibleOctaves));
        context.synthPreviewMidi =
            std::clamp((context.armedOctave * 12) + (context.synthPreviewMidi % 12), 0, 127);
        context.paintNoteMidi = context.synthPreviewMidi;
        context.synthWindowNeedsRedraw = true;
        return;
    }

    if (context.synthWindow != 0) {
        XUnmapWindow(context.display, context.synthWindow);
    }
    context.synthWindowVisible = false;
    context.synthPointerDown = false;
    context.synthLastPointerMidi = -1;
    clearSynthPreviewKeys(context.synthPreviewKeyHeld, context.synthPreviewKeyMidi);
    clearSynthMidiPreviewNotes(context.synthMidiPreviewHeld);
    context.synthTooltipParam.clear();
    context.synthTooltipHoverSince = std::chrono::steady_clock::time_point {};
    context.synthScopeLaneActive = false;
    for (Synthesizer& laneSynth : context.synthScopeLaneSynth) {
        laneSynth.reset();
    }
}

} // namespace arachno
