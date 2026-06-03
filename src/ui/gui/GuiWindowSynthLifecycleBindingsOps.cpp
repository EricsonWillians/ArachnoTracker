#include "ui/gui/GuiWindowSynthLifecycleBindingsOps.h"

#include "ui/gui/GuiFileButtonOps.h"

namespace arachno {

GuiWindowSynthLifecycleBindings makeSynthLifecycleBindingsFromWindowState(
    const GuiWindowSynthLifecycleBindingsInput& input) {
    GuiWindowSynthLifecycleBindings bindings {
        GuiSynthWindowLifecycleAdapterInput {
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
            input.lastAction},
        {},
        {},
        {},
        {},
        {},
        {}};

    GuiSynthWindowLifecycleAdapterInput lifecycleInput = bindings.lifecycleInput;
    auto synthWindowVisibleRef = std::ref(input.synthWindowVisible);
    auto activeSnapshot = input.activeSnapshot;
    auto runLifecycleAction = input.runLifecycleAction;
    auto runAction = input.runAction;
    auto beginInlinePrompt = input.beginInlinePrompt;
    auto audioTuningDialogActiveRef = std::ref(input.audioTuningDialogActive);

    bindings.releaseSynthBackbuffer = [lifecycleInput]() {
        releaseSynthBackbufferFromWindowState(lifecycleInput);
    };
    bindings.ensureSynthBackbuffer = [lifecycleInput]() {
        ensureSynthBackbufferFromWindowState(lifecycleInput);
    };
    bindings.claimSynthPreviewKey = [lifecycleInput](unsigned int keycode, int midiNote) {
        return claimSynthPreviewKeyFromWindowState(lifecycleInput, keycode, midiNote);
    };
    bindings.releaseSynthPreviewKey = [lifecycleInput](unsigned int keycode) {
        releaseSynthPreviewKeyFromWindowState(lifecycleInput, keycode);
    };
    bindings.setSynthWindowVisible = [lifecycleInput](bool visible) {
        setSynthWindowVisibleFromWindowState(lifecycleInput, visible);
    };
    bindings.runFileButtonAction = [=](const std::string& actionId) {
        arachno::runFileButtonAction(
            GuiFileButtonContext {
                audioTuningDialogActiveRef.get(),
                activeSnapshot,
                [lifecycleInput, synthWindowVisibleRef]() {
                    setSynthWindowVisibleFromWindowState(lifecycleInput, !synthWindowVisibleRef.get());
                },
                [runLifecycleAction](const AppActionRequest& request) { runLifecycleAction(request); },
                [runAction](const AppActionRequest& request) { (void)runAction(request); },
                beginInlinePrompt},
            actionId);
    };

    return bindings;
}

} // namespace arachno
