#include "ui/gui/GuiMainLoopTickOps.h"

#include <thread>

namespace arachno {

void runMainLoopTick(const GuiMainLoopTickContext& context) {
    const bool streamWasActive = context.previousAudioStreamActive;
    if (context.pollMidiInput() && context.synthWindowVisible) {
        context.needsRedraw = true;
    }

    context.processRealtimeAudio();
    const bool streamActive = context.previousAudioStreamActive;

    const auto now = std::chrono::steady_clock::now();
    const auto refreshPeriod = streamActive ? std::chrono::milliseconds(240) : std::chrono::milliseconds(80);
    if (now - context.lastRefresh >= refreshPeriod) {
        context.refreshSnapshot();
        context.needsRedraw = true;
        context.lastRefresh = now;
    }
    if (context.synthWindowVisible
        && !context.synthTooltipParam.empty()
        && context.synthTooltipHoverSince != std::chrono::steady_clock::time_point {}) {
        const auto hoverMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - context.synthTooltipHoverSince);
        if (hoverMs.count() >= 520) {
            context.synthWindowNeedsRedraw = true;
        }
    }

    if (context.needsRedraw) {
        context.drawMainWindow();
        if (context.synthWindowVisible) {
            context.synthWindowNeedsRedraw = true;
        }
        context.needsRedraw = false;
    }
    if (context.synthWindowVisible && context.synthWindowNeedsRedraw) {
        context.drawSynthWindow();
        context.synthWindowNeedsRedraw = false;
    }

    if (streamWasActive || streamActive) {
        // Service audio again after potentially expensive draw work.
        context.processRealtimeAudio();
    }

    if (streamActive) {
        // Yield instead of sleeping so audio refill can run again with minimal delay.
        std::this_thread::yield();
    } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

} // namespace arachno
