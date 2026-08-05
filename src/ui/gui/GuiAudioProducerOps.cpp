#include "ui/gui/GuiAudioProducerOps.h"

#include <chrono>
#include <exception>

#include "SpikeLog.h"
#include "ui/gui/GuiMainRealtimeAudioOps.h"

namespace arachno {

GuiAudioProducer::GuiAudioProducer(
    ApplicationSession& session,
    GuiAudioRuntime& audioRuntime,
    std::function<bool(const float*, const float*, int)> writeAudioOutput,
    std::function<void(int)> tuneRealtimeAudioForLoad)
    : session_(session)
    , audioRuntime_(audioRuntime)
    , writeAudioOutput_(std::move(writeAudioOutput))
    , tuneRealtimeAudioForLoad_(std::move(tuneRealtimeAudioForLoad)) {}

GuiAudioProducer::~GuiAudioProducer() {
    stop();
}

bool GuiAudioProducer::start() {
    if (running_.load(std::memory_order_relaxed)) {
        return true;
    }
    stopRequested_.store(false, std::memory_order_relaxed);
    try {
        thread_ = std::thread([this] { threadMain(); });
    } catch (const std::exception&) {
        return false;
    } catch (...) {
        return false;
    }
    running_.store(true, std::memory_order_relaxed);
    return true;
}

void GuiAudioProducer::stop() {
    stopRequested_.store(true, std::memory_order_relaxed);
    if (thread_.joinable()) {
        thread_.join();
    }
    running_.store(false, std::memory_order_relaxed);
}

void GuiAudioProducer::threadMain() {
    while (!stopRequested_.load(std::memory_order_relaxed)) {
        const PlaybackSnapshot playback = session_.playback().snapshot();
        const bool streaming = (playback.state == TransportState::Playing || playback.previewActive)
            && audioRuntime_.hasOutput();
        if (!streaming) {
            // Keep clocks fresh so the next stream start behaves like the
            // inline path (prime burst instead of a huge elapsed-time catch-up).
            lastPlaybackTick_ = std::chrono::steady_clock::now();
            audioPendingFrames_ = 0.0;
            previousAudioStreamActive_ = false;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }
        const int framesRendered = [&] {
            const SpikeProbe probe("producer-render", 30.0, "block batch exceeded");
            return renderMainRealtimeAudioBlocks(
                GuiMainRealtimeAudioContext {
                    session_,
                    audioRuntime_,
                    audioLeft_,
                    audioRight_,
                    lastPlaybackTick_,
                    audioPendingFrames_,
                    previousAudioStreamActive_,
                    lastAction_,
                    {}, // openAudioOutput: the GUI thread owns output lifecycle
                    {}, // closeAudioOutput
                    writeAudioOutput_,
                    tuneRealtimeAudioForLoad_,
                    false});
        }();
        if (framesRendered <= 0) {
            // Caught up with the queue target (or the queue is full): do not
            // hot-spin waiting for work/space.
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    running_.store(false, std::memory_order_relaxed);
}

} // namespace arachno
