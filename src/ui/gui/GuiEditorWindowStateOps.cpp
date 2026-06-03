#include "ui/gui/GuiEditorWindowStateOps.h"

#include <algorithm>

#include "ui/gui/GuiArrangementOps.h"
#include "ui/gui/GuiSynthPreviewOps.h"

namespace arachno {

GuiEditorWindowStateBindings makeEditorWindowStateBindingsFromWindowState(
    const GuiEditorWindowStateBindingsInput& input) {
    const auto state = input;
    GuiEditorWindowStateBindings bindings;

    bindings.resizePatternRows = [state](int rows) {
        resizePatternRowsFromWindowState(
            rows,
            state.activePatternRows,
            [state](const AppActionRequest& request) { return state.runAction(request); });
    };

    bindings.setArmedOctave = [state](int octave) {
        setArmedOctaveFromWindowState(octave, state.armedOctave, state.paintNoteMidi);
    };

    bindings.applyArmedOctaveToSelection = [state]() {
        applyArmedOctaveToSelectionFromWindowState(
            state.armedOctave,
            [state](const AppActionRequest& request) { return state.runAction(request); });
    };

    bindings.ensureSynthKeyboardShowsMidi = [state](int midiNote) {
        state.synthKeyboardBaseOctave = synthKeyboardBaseForMidi(
            state.synthKeyboardBaseOctave,
            state.synthKeyboardVisibleOctaves,
            midiNote);
    };

    bindings.ensurePatternRowsForRow = [state](int row) {
        ensurePatternRowsForRowFromWindowState(
            row,
            state.activePatternRows,
            [state](int rows) {
                resizePatternRowsFromWindowState(
                    rows,
                    state.activePatternRows,
                    [state](const AppActionRequest& request) { return state.runAction(request); });
            });
    };

    bindings.applyLastActionState = [state](const GuiLastActionState& actionState) {
        state.lastAction.ok = actionState.ok;
        state.lastAction.actionId = actionState.actionId;
        state.lastAction.message = actionState.message;
        state.lastAction.error = actionState.error;
    };

    bindings.applyActionResultStatus = [state](const AppActionResult& result) {
        state.lastAction.ok = result.ok;
        state.lastAction.actionId = result.actionId;
        state.lastAction.message = result.message;
        state.lastAction.error = result.error;
    };

    return bindings;
}

void resizePatternRowsFromWindowState(
    int rows,
    int& activePatternRows,
    const std::function<AppActionResult(const AppActionRequest&)>& runAction) {
    rows = std::clamp(rows, 8, 8192);
    if (rows == activePatternRows) {
        return;
    }
    AppActionRequest resize;
    resize.actionId = "editor.pattern.resize";
    resize.parameters = {{"rows", std::to_string(rows)}};
    (void)runAction(resize);
}

void setArmedOctaveFromWindowState(int octave, int& armedOctave, int& paintNoteMidi) {
    armedOctave = std::clamp(octave, 0, 8);
    const int pitchClass = std::clamp(paintNoteMidi, 0, 127) % 12;
    paintNoteMidi = std::clamp((armedOctave * 12) + pitchClass, 0, 127);
}

void applyArmedOctaveToSelectionFromWindowState(
    int armedOctave,
    const std::function<AppActionResult(const AppActionRequest&)>& runAction) {
    AppActionRequest octaveRequest;
    octaveRequest.actionId = "editor.step.octave";
    octaveRequest.parameters = {{"octave", std::to_string(armedOctave)}};
    (void)runAction(octaveRequest);
}

void ensurePatternRowsForRowFromWindowState(
    int row,
    int activePatternRows,
    const std::function<void(int)>& resizePatternRows) {
    if (row < activePatternRows) {
        return;
    }
    const int targetRows = expandedPatternRowsForRow(activePatternRows, row);
    resizePatternRows(targetRows);
}

} // namespace arachno
