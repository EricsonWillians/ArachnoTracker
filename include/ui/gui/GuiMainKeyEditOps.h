#pragma once

#include <functional>
#include <string>
#include <utility>

#include <X11/Xlib.h>

namespace arachno {

struct GuiMainKeyEditResult {
    bool consumed = false;
    bool needsRedraw = false;
    bool synthWindowNeedsRedraw = false;
};

struct GuiMainKeyEditContext {
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
    std::function<void(int)> cycleInstrument;
    std::function<void(int)> selectInstrumentIndex;
    std::function<void()> auditionArmedInstrument;
    std::function<std::pair<int, int>()> currentCursor;
    std::function<void(int, int)> moveCursor;
    std::function<void(int, int, int)> paintNoteAt;
    std::function<void()> refreshSnapshot;
    std::function<void(int)> ensurePatternRowsForRow;
    std::function<void(const std::string&)> runActionById;
    std::function<void(int, int, int, int)> applySelectionRange;
    std::function<void()> lockManualScroll;
    std::function<int()> currentInstrumentCount;
    std::function<void()> selectAll;
};

GuiMainKeyEditResult handleMainKeyEditOps(const GuiMainKeyEditContext& context);

} // namespace arachno
