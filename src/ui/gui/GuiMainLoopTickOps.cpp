#include "ui/gui/GuiMainLoopTickOps.h"

#include <thread>

#include "SpikeLog.h"

namespace arachno {

void runMainLoopTick(const GuiMainLoopTickContext& context) {
    const bool streamWasActive = context.previousAudioStreamActive;
    {
        const SpikeProbe probe("gui-midi-poll", 20.0, "MIDI input drain");
        if (context.pollMidiInput() && context.synthWindowVisible) {
            context.needsRedraw = true;
        }
    }

    context.processRealtimeAudio();
    const bool streamActive = context.previousAudioStreamActive;

    const auto now = std::chrono::steady_clock::now();
    // Fast playhead path: keeps the playhead highlight, follow-scroll, and order
    // sync moving at ~30 Hz during playback without rebuilding the view model.
    if (context.pollPlayhead && now - context.lastPlayheadPoll >= std::chrono::milliseconds(33)) {
        const int pollResult = context.pollPlayhead();
        if (pollResult == 2) {
            const SpikeProbe probe("gui-refresh", 40.0, "view-model rebuild (playhead)");
            context.refreshSnapshot();
            context.needsRedraw = true;
            context.lastRefresh = now;
        } else if (pollResult == 1) {
            context.needsRedraw = true;
        }
        context.lastPlayheadPoll = now;
    }
    const auto refreshPeriod = streamActive ? std::chrono::milliseconds(240) : std::chrono::milliseconds(80);
    if (now - context.lastRefresh >= refreshPeriod) {
        const SpikeProbe probe("gui-refresh", 40.0, "view-model rebuild (timer)");
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
        const SpikeProbe probe("gui-draw", 60.0, "main window repaint");
        context.drawMainWindow();
        if (context.synthWindowVisible) {
            context.synthWindowNeedsRedraw = true;
        }
        context.needsRedraw = false;
    }
    if (context.synthWindowVisible && context.synthWindowNeedsRedraw) {
        const SpikeProbe probe("gui-draw-synth", 60.0, "synth window repaint");
        context.drawSynthWindow();
        context.synthWindowNeedsRedraw = false;
    }

    if (streamWasActive || streamActive) {
        // Service audio again after potentially expensive draw work.
        context.processRealtimeAudio();
    }

    if (streamActive && !context.audioProducerActive) {
        // Inline fallback (P-GUI): the GUI thread still feeds the device, so
        // yield instead of sleeping to keep refill latency minimal.
        std::this_thread::yield();
    } else {
        // Producer thread owns block rendering (or nothing is streaming): a
        // short sleep keeps input latency low without pegging a core. Before
        // the producer thread existed this loop hot-spun with yield() during
        // streaming, starving event dispatch on loaded machines.
        std::this_thread::sleep_for(std::chrono::milliseconds(streamActive ? 4 : 16));
    }
}

} // namespace arachno
