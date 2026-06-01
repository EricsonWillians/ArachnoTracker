#pragma once

#include <functional>

#include <X11/Xlib.h>

#include "ui/gui/GuiMainKeyCommandOps.h"
#include "ui/gui/GuiMainKeyEditOps.h"
#include "ui/gui/GuiMainKeyModalOps.h"
#include "ui/gui/GuiMainKeyNoteOps.h"

namespace arachno {

struct GuiMainKeyPressDispatchResult {
    bool consumed = false;
    bool needsRedraw = false;
    bool synthWindowNeedsRedraw = false;
    bool shouldQuit = false;
};

struct GuiMainKeyPressDispatchContext {
    XKeyEvent event {};

    std::function<int()> playbackSampleRate;
    std::function<GuiMainKeyModalResult(
        KeySym,
        bool,
        bool,
        bool,
        int,
        const char*,
        const std::function<bool(KeySym)>&,
        const std::function<int()>&,
        int)>
        dispatchModal;
    std::function<GuiMainKeyNoteResult(
        KeySym,
        unsigned int,
        bool,
        bool,
        bool,
        const std::function<bool(KeySym)>&,
        const std::function<int()>&)>
        dispatchNote;
    std::function<GuiMainKeyCommandResult(KeySym, bool, bool, bool, const std::function<bool(KeySym)>&)>
        dispatchCommand;
    std::function<GuiMainKeyEditResult(
        KeySym,
        bool,
        bool,
        bool,
        const std::function<bool(KeySym)>&,
        const std::function<int()>&)>
        dispatchEdit;
};

GuiMainKeyPressDispatchResult dispatchMainKeyPress(const GuiMainKeyPressDispatchContext& context);

} // namespace arachno
