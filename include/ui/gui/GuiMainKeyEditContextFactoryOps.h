#pragma once

#include <functional>

#include <X11/Xlib.h>

#include "AppActions.h"
#include "ui/gui/GuiMainKeyEditOps.h"

namespace arachno {

struct GuiMainKeyEditContextFactoryInput {
    KeySym key = NoSymbol;
    bool ctrlDown = false;
    bool shiftDown = false;
    bool altDown = false;
    bool synthWindowVisible = false;

    bool& stepAdvance;
    float& defaultVelocity;
    int& armedOctave;
    int& synthPreviewMidi;
    int& paintNoteMidi;
    int activePatternRows = 64;
    int requestedRowCount = 16;
    int& viewStartRow;
    bool& keyboardSelectionActive;
    int& keyboardSelectionAnchorRow;
    int& keyboardSelectionAnchorTrack;

    std::function<bool(KeySym)> keyMatches;
    std::function<int()> resolvedDigit;
    std::function<void(int)> setArmedOctave;
    std::function<void()> applyArmedOctaveToSelection;
    std::function<void(int)> ensureSynthKeyboardShowsMidi;
    std::function<void(int)> cycleInstrumentBy;
    std::function<void(int)> selectInstrument;
    std::function<void()> auditionArmedInstrument;
    std::function<void(int, int)> moveCursor;
    std::function<void(int, int, int)> paintNoteAt;
    std::function<void()> refreshSnapshot;
    std::function<void(int)> ensurePatternRowsForRow;
    std::function<void(const std::string&)> runActionById;
    std::function<void(int, int, int, int)> applySelectionRange;
    std::function<void()> lockManualScroll;
    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<AppActionResult(const AppActionRequest&)> runAction;
};

GuiMainKeyEditContext makeMainKeyEditContextFromState(const GuiMainKeyEditContextFactoryInput& input);

} // namespace arachno
