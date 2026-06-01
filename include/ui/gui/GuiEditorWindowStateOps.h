#pragma once

#include <functional>

#include "AppActions.h"
#include "ui/gui/GuiArrangementOps.h"

namespace arachno {

struct GuiEditorWindowStateBindingsInput {
    int& activePatternRows;
    int& armedOctave;
    int& paintNoteMidi;
    int& synthKeyboardBaseOctave;
    int& synthKeyboardVisibleOctaves;
    AppActionResult& lastAction;
    std::function<AppActionResult(const AppActionRequest&)> runAction;
};

struct GuiEditorWindowStateBindings {
    std::function<void(int)> resizePatternRows;
    std::function<void(int)> setArmedOctave;
    std::function<void()> applyArmedOctaveToSelection;
    std::function<void(int)> ensureSynthKeyboardShowsMidi;
    std::function<void(int)> ensurePatternRowsForRow;
    std::function<void(const GuiLastActionState&)> applyLastActionState;
    std::function<void(const AppActionResult&)> applyActionResultStatus;
};

GuiEditorWindowStateBindings makeEditorWindowStateBindingsFromWindowState(
    const GuiEditorWindowStateBindingsInput& input);

void resizePatternRowsFromWindowState(
    int rows,
    int& activePatternRows,
    const std::function<AppActionResult(const AppActionRequest&)>& runAction);

void setArmedOctaveFromWindowState(int octave, int& armedOctave, int& paintNoteMidi);

void applyArmedOctaveToSelectionFromWindowState(
    int armedOctave,
    const std::function<AppActionResult(const AppActionRequest&)>& runAction);

void ensurePatternRowsForRowFromWindowState(
    int row,
    int activePatternRows,
    const std::function<void(int)>& resizePatternRows);

} // namespace arachno
