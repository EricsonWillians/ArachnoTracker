#pragma once

#include <chrono>
#include <functional>
#include <string>

#include <X11/Xlib.h>

#include "ui/gui/GuiMainWindowInteractionAdapterOps.h"
#include "ui/gui/GuiSynthWindowInteractionAdapterOps.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiWindowRunLoopAdapterContext {
    Display* display = nullptr;
    Window window = 0;
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
    TrackerWindowLayout& layout;
    int& pointerX;
    int& pointerY;

    GuiMainWindowInteractionAdapterContext& mainInteractionContext;
    GuiSynthWindowInteractionAdapterContext& synthInteractionContext;

    bool& synthWindowVisible;
    bool& previousAudioStreamActive;
    std::string& synthTooltipParam;
    std::chrono::steady_clock::time_point& synthTooltipHoverSince;
    std::chrono::steady_clock::time_point& lastRefresh;
    std::chrono::steady_clock::time_point& lastPlayheadPoll;

    std::function<bool(const XEvent&)> isAutoRepeatRelease;
    std::function<void(unsigned int)> releaseSynthPreviewKey;
    std::function<void()> ensureTrackerBackbuffer;
    std::function<int()> playbackSampleRate;
    std::function<void()> processRealtimeAudio;
    std::function<void()> refreshSnapshot;
    std::function<int()> pollPlayhead;
    std::function<void()> drawMainWindow;
    std::function<void()> drawSynthWindow;
    std::function<void()> syncArmedInstrument;
    // True when the dedicated audio producer thread owns block rendering; the
    // GUI loop may sleep instead of hot-spinning to service inline audio.
    bool audioProducerActive = false;
};

void runMainLoopFromAdapter(const GuiWindowRunLoopAdapterContext& context);

} // namespace arachno
