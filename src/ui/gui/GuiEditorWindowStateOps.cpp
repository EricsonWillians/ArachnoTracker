#include "ui/gui/GuiEditorWindowStateOps.h"

#include <algorithm>

#include "ui/gui/GuiArrangementOps.h"
#include "ui/gui/GuiSynthPreviewOps.h"

namespace arachno {

GuiEditorWindowStateBindings makeEditorWindowStateBindingsFromWindowState(
    const GuiEditorWindowStateBindingsInput& input) {
    GuiEditorWindowStateBindings bindings;

    bindings.resizePatternRows = [&](int rows) {
        resizePatternRowsFromWindowState(
            rows,
            input.activePatternRows,
            [&](const AppActionRequest& request) { return input.runAction(request); });
    };

    bindings.setArmedOctave = [&](int octave) {
        setArmedOctaveFromWindowState(octave, input.armedOctave, input.paintNoteMidi);
    };

    bindings.applyArmedOctaveToSelection = [&]() {
        applyArmedOctaveToSelectionFromWindowState(
            input.armedOctave,
            [&](const AppActionRequest& request) { return input.runAction(request); });
    };

    bindings.ensureSynthKeyboardShowsMidi = [&](int midiNote) {
        input.synthKeyboardBaseOctave = synthKeyboardBaseForMidi(
            input.synthKeyboardBaseOctave,
            input.synthKeyboardVisibleOctaves,
            midiNote);
    };

    bindings.ensurePatternRowsForRow = [&](int row) {
        ensurePatternRowsForRowFromWindowState(
            row,
            input.activePatternRows,
            [&](int rows) { bindings.resizePatternRows(rows); });
    };

    bindings.applyLastActionState = [&](const GuiLastActionState& state) {
        input.lastAction.ok = state.ok;
        input.lastAction.actionId = state.actionId;
        input.lastAction.message = state.message;
        input.lastAction.error = state.error;
    };

    bindings.applyActionResultStatus = [&](const AppActionResult& result) {
        input.lastAction.ok = result.ok;
        input.lastAction.actionId = result.actionId;
        input.lastAction.message = result.message;
        input.lastAction.error = result.error;
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
