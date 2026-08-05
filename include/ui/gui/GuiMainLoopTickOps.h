#pragma once

#include <chrono>
#include <functional>
#include <string>

namespace arachno {

struct GuiMainLoopTickContext {
    bool synthWindowVisible = false;
    bool& synthWindowNeedsRedraw;
    bool& needsRedraw;
    bool& previousAudioStreamActive;
    std::string& synthTooltipParam;
    std::chrono::steady_clock::time_point& synthTooltipHoverSince;
    std::chrono::steady_clock::time_point& lastRefresh;
    std::chrono::steady_clock::time_point& lastPlayheadPoll;

    std::function<bool()> pollMidiInput;
    std::function<void()> processRealtimeAudio;
    std::function<void()> refreshSnapshot;
    // Cheap playhead-only poll (~30 Hz during playback); returns
    // GuiPlayheadPollResult as int (0=unchanged, 1=redraw, 2=full refresh).
    std::function<int()> pollPlayhead;
    std::function<void()> drawMainWindow;
    std::function<void()> drawSynthWindow;
    // True when the dedicated audio producer thread owns block rendering; the
    // GUI loop may sleep instead of hot-spinning to service inline audio.
    bool audioProducerActive = false;
};

void runMainLoopTick(const GuiMainLoopTickContext& context);

} // namespace arachno
