#pragma once

#include <functional>

#include <X11/Xlib.h>

#include "ui/gui/GuiMainButtonPressContextFactoryOps.h"
#include "ui/gui/GuiMainButtonPressDispatchOps.h"
#include "ui/gui/GuiMainGridClickOps.h"
#include "ui/gui/GuiMainKeyPressDispatchOps.h"
#include "ui/gui/GuiMainKeyCommandOps.h"
#include "ui/gui/GuiMainKeyEditOps.h"
#include "ui/gui/GuiMainKeyModalOps.h"
#include "ui/gui/GuiMainKeyNoteOps.h"
#include "ui/gui/GuiMainModalMouseOps.h"
#include "ui/gui/GuiMainWheelOps.h"

namespace arachno {

struct GuiMainButtonXEventDispatchContext {
    std::function<int()> playbackSampleRate;
    std::function<GuiMainButtonPressContextFactoryInput()> makeButtonFactoryInput;

    UiRect sidebarViewport;
    int verticalSplitterX = 0;
    int horizontalSplitterY = 0;
    int& pointerX;
    int& pointerY;
    bool& resizingSidebar;
    bool& resizingTopPanel;

    std::function<GuiMainModalMouseContext(int)> makeModalMouseContext;
    std::function<GuiMainModalMouseResult(const GuiMainModalMouseContext&, int, int, int, unsigned int)> runModalMouse;
    std::function<GuiMainWheelContext(int, int, int, unsigned int)> makeWheelContext;
    std::function<GuiMainWheelResult(const GuiMainWheelContext&)> runWheel;
    std::function<GuiMainPrimaryClickResult(const GuiMainPrimaryClickContext&)> dispatchPrimaryLeft;
    std::function<GuiMainSidebarClickResult(const GuiMainSidebarClickContext&)> dispatchSidebarLeft;
    std::function<GuiMainGridClickContext(int, int, int, bool, bool)> makeGridClickContext;
    std::function<GuiMainGridClickResult(const GuiMainGridClickContext&)> runGridClick;
};

GuiMainButtonPressDispatchResult dispatchMainButtonXEvent(
    const GuiMainButtonXEventDispatchContext& context,
    const XButtonEvent& event);

struct GuiMainKeyXEventDispatchContext {
    std::function<int()> playbackSampleRate;
    std::function<GuiMainKeyModalContext(
        KeySym,
        bool,
        bool,
        bool,
        int,
        const char*,
        const std::function<bool(KeySym)>&,
        const std::function<int()>&,
        int)>
        makeModalContext;
    std::function<GuiMainKeyModalResult(const GuiMainKeyModalContext&)> runModal;
    std::function<GuiMainKeyNoteContext(
        KeySym,
        unsigned int,
        bool,
        bool,
        bool,
        const std::function<bool(KeySym)>&,
        const std::function<int()>&)>
        makeNoteContext;
    std::function<GuiMainKeyNoteResult(const GuiMainKeyNoteContext&)> runNote;
    std::function<GuiMainKeyCommandContext(KeySym, bool, bool, bool, const std::function<bool(KeySym)>&)>
        makeCommandContext;
    std::function<GuiMainKeyCommandResult(const GuiMainKeyCommandContext&)> runCommand;
    std::function<GuiMainKeyEditContext(
        KeySym,
        bool,
        bool,
        bool,
        const std::function<bool(KeySym)>&,
        const std::function<int()>&)>
        makeEditContext;
    std::function<GuiMainKeyEditResult(const GuiMainKeyEditContext&)> runEdit;
};

GuiMainKeyPressDispatchResult dispatchMainKeyXEvent(
    const GuiMainKeyXEventDispatchContext& context,
    const XKeyEvent& event);

} // namespace arachno
