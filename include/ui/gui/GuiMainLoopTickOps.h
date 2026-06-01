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

    std::function<bool()> pollMidiInput;
    std::function<void()> processRealtimeAudio;
    std::function<void()> refreshSnapshot;
    std::function<void()> drawMainWindow;
    std::function<void()> drawSynthWindow;
};

void runMainLoopTick(const GuiMainLoopTickContext& context);

} // namespace arachno
