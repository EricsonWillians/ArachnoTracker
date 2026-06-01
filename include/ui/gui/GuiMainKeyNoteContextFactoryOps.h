#pragma once

#include <functional>

#include <X11/Xlib.h>

#include "AppActions.h"
#include "ui/gui/GuiMainKeyNoteOps.h"

namespace arachno {

struct GuiMainKeyNoteContextFactoryInput {
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
    float defaultVelocity = 0.8f;

    std::function<bool(KeySym)> keyMatches;
    std::function<int()> resolvedDigit;
    std::function<void(int)> setArmedOctave;
    std::function<void(int)> ensureSynthKeyboardShowsMidi;
    std::function<bool(unsigned int, int)> claimSynthPreviewKey;
    std::function<void(int)> auditionSynthPreviewMidi;
    std::function<void(int)> selectInstrument;
    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<void(int)> ensurePatternRowsForRow;
    std::function<AppActionResult(const AppActionRequest&, bool)> runActionWithRefresh;
};

GuiMainKeyNoteContext makeMainKeyNoteContextFromState(const GuiMainKeyNoteContextFactoryInput& input);

} // namespace arachno
