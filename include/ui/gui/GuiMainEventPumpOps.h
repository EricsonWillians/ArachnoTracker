#pragma once

#include <functional>

#include <X11/Xlib.h>

#include "ui/gui/GuiMainButtonPressContextFactoryOps.h"
#include "ui/gui/GuiMainGridClickOps.h"
#include "ui/gui/GuiMainKeyCommandOps.h"
#include "ui/gui/GuiMainKeyEditOps.h"
#include "ui/gui/GuiMainKeyModalOps.h"
#include "ui/gui/GuiMainKeyNoteOps.h"
#include "ui/gui/GuiMainModalMouseOps.h"
#include "ui/gui/GuiMainMotionOps.h"
#include "ui/gui/GuiMainPrimaryClickOps.h"
#include "ui/gui/GuiMainSidebarClickOps.h"
#include "ui/gui/GuiMainWheelOps.h"
#include "ui/gui/GuiSynthWindowEventOps.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiMainEventPumpContext {
    Display* display = nullptr;
    Window mainWindow = 0;
    Atom wmDelete = 0;

    bool& running;
    bool& needsRedraw;
    bool& synthWindowNeedsRedraw;

    int& windowWidth;
    int& windowHeight;
    bool& resizingSidebar;
    bool& resizingTopPanel;
    bool& draggingSelection;
    bool& draggingPatternRows;
    bool& paintingNotes;
    int& lastPaintRow;
    int& lastPaintTrack;
    UiRect& sidebarViewport;
    const TrackerWindowLayout& layout;
    int& pointerX;
    int& pointerY;

    std::function<GuiSynthWindowEventContext()> makeSynthWindowEventContext;
    std::function<bool(const XEvent&)> isAutoRepeatRelease;
    std::function<void(unsigned int)> releaseSynthPreviewKey;
    std::function<void()> ensureTrackerBackbuffer;

    std::function<GuiMainMotionContext(int, int, unsigned int)> makeMainMotionContext;
    std::function<GuiMainMotionResult(const GuiMainMotionContext&)> handleMainMotionNotify;
    std::function<void()> serviceRealtimeAudio;

    std::function<int()> playbackSampleRate;
    std::function<GuiMainButtonPressContextFactoryInput()> makeButtonPressFactoryInput;
    std::function<GuiMainModalMouseContext(int)> makeMainModalMouseContext;
    std::function<GuiMainModalMouseResult(const GuiMainModalMouseContext&, int, int, int, unsigned int)>
        handleMainModalButtonPress;
    std::function<GuiMainWheelContext(int, int, int, unsigned int)> makeMainWheelContext;
    std::function<GuiMainWheelResult(const GuiMainWheelContext&)> handleMainWheel;
    std::function<GuiMainPrimaryClickResult(const GuiMainPrimaryClickContext&)> handleMainPrimaryLeftClick;
    std::function<GuiMainSidebarClickResult(const GuiMainSidebarClickContext&)> handleMainSidebarLeftClick;
    std::function<GuiMainGridClickContext(int, int, int, bool, bool)> makeMainGridClickContext;
    std::function<GuiMainGridClickResult(const GuiMainGridClickContext&)> handleMainGridClick;

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
        makeMainKeyModalContext;
    std::function<GuiMainKeyModalResult(const GuiMainKeyModalContext&)> handleMainKeyModal;
    std::function<GuiMainKeyNoteContext(
        KeySym,
        unsigned int,
        bool,
        bool,
        bool,
        const std::function<bool(KeySym)>&,
        const std::function<int()>&)>
        makeMainKeyNoteContext;
    std::function<GuiMainKeyNoteResult(const GuiMainKeyNoteContext&)> handleMainKeyNoteOps;
    std::function<GuiMainKeyCommandContext(KeySym, bool, bool, bool, const std::function<bool(KeySym)>&)>
        makeMainKeyCommandContext;
    std::function<GuiMainKeyCommandResult(const GuiMainKeyCommandContext&)> handleMainKeyCommands;
    std::function<GuiMainKeyEditContext(
        KeySym,
        bool,
        bool,
        bool,
        const std::function<bool(KeySym)>&,
        const std::function<int()>&)>
        makeMainKeyEditContext;
    std::function<GuiMainKeyEditResult(const GuiMainKeyEditContext&)> handleMainKeyEditOps;
};

void pumpPendingMainEvents(const GuiMainEventPumpContext& context);

} // namespace arachno
