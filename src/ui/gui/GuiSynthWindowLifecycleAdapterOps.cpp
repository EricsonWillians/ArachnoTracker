#include "ui/gui/GuiSynthWindowLifecycleAdapterOps.h"

namespace arachno {

GuiSynthWindowLifecycleContext makeSynthWindowLifecycleContextFromWindowState(
    const GuiSynthWindowLifecycleAdapterInput& input) {
    return GuiSynthWindowLifecycleContext {
        input.display,
        input.screen,
        input.wmDelete,
        input.uiFont,
        input.synthWindow,
        input.synthGc,
        input.synthBackbuffer,
        input.synthBackbufferWidth,
        input.synthBackbufferHeight,
        input.synthWindowWidth,
        input.synthWindowHeight,
        input.synthWindowVisible,
        input.synthPreviewKeyHeld,
        input.synthPreviewKeyMidi,
        input.synthMidiPreviewHeld,
        input.synthPointerDown,
        input.synthLastPointerMidi,
        input.synthTooltipParam,
        input.synthTooltipHoverSince,
        input.synthScopeRetriggerRequested,
        input.synthKeyboardBaseOctave,
        input.synthKeyboardVisibleOctaves,
        input.synthPreviewMidi,
        input.paintNoteMidi,
        input.armedOctave,
        input.synthWindowNeedsRedraw,
        input.synthScopeLaneActive,
        input.synthScopeLaneSynth,
        input.lastAction};
}

void releaseSynthBackbufferFromWindowState(const GuiSynthWindowLifecycleAdapterInput& input) {
    releaseSynthBackbufferResources(makeSynthWindowLifecycleContextFromWindowState(input));
}

void ensureSynthBackbufferFromWindowState(const GuiSynthWindowLifecycleAdapterInput& input) {
    ensureSynthBackbufferResources(makeSynthWindowLifecycleContextFromWindowState(input));
}

bool claimSynthPreviewKeyFromWindowState(
    const GuiSynthWindowLifecycleAdapterInput& input,
    unsigned int keycode,
    int midiNote) {
    return claimSynthPreviewKey(input.synthPreviewKeyHeld, input.synthPreviewKeyMidi, keycode, midiNote);
}

void releaseSynthPreviewKeyFromWindowState(const GuiSynthWindowLifecycleAdapterInput& input, unsigned int keycode) {
    releaseSynthPreviewKey(input.synthPreviewKeyHeld, input.synthPreviewKeyMidi, keycode);
}

void setSynthWindowVisibleFromWindowState(const GuiSynthWindowLifecycleAdapterInput& input, bool visible) {
    setSynthWindowVisible(makeSynthWindowLifecycleContextFromWindowState(input), visible);
}

} // namespace arachno
