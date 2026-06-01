#include "ui/gui/GuiMainLoopTickOps.h"

#include <thread>

namespace arachno {

void runMainLoopTick(const GuiMainLoopTickContext& context) {
    if (context.pollMidiInput() && context.synthWindowVisible) {
        context.needsRedraw = true;
    }

    context.processRealtimeAudio();

    const auto now = std::chrono::steady_clock::now();
    if (now - context.lastRefresh >= std::chrono::milliseconds(80)) {
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

    const int sleepMs = context.previousAudioStreamActive ? 1 : 16;
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
}

} // namespace arachno
