#pragma once

#include <functional>
#include <utility>

#include <X11/Xlib.h>

namespace arachno {

struct GuiMainKeyNoteResult {
    bool consumed = false;
    bool needsRedraw = false;
    bool synthWindowNeedsRedraw = false;
};

struct GuiMainKeyNoteContext {
    KeySym key = NoSymbol;
    unsigned int keycode = 0;
    bool ctrlDown = false;
    bool shiftDown = false;
    bool altDown = false;

    bool synthWindowVisible = false;
    int& armedOctave;
    int& armedInstrument;
    int& synthPreviewMidi;
    int& paintNoteMidi;
    bool stepAdvance = true;

    std::function<bool(KeySym)> keyMatches;
    std::function<int()> resolvedDigit;
    std::function<void(int)> setArmedOctave;
    std::function<void(int)> ensureSynthKeyboardShowsMidi;
    std::function<bool(unsigned int, int)> claimSynthPreviewKey;
    std::function<void(int)> auditionSynthPreviewMidi;
    std::function<void(int)> selectInstrument;
    std::function<int()> instrumentCount;
    std::function<bool(int, int, bool)> applyTrackerNoteAtCursor;
};

GuiMainKeyNoteResult handleMainKeyNoteOps(const GuiMainKeyNoteContext& context);

} // namespace arachno
