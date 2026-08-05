#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <vector>

#include "AppActions.h"
#include "ApplicationSession.h"
#include "ui/gui/GuiAudioRuntime.h"

namespace arachno {

// Dedicated audio producer thread. Renders realtime blocks into the (already
// thread-safe) output queue so GUI redraws, event storms, and snapshot rebuilds
// can no longer starve realtime playback — the pre-2026-08 design rendered all
// audio synchronously on the GUI main thread, which hard-stuttered on dense
// songs. The GUI keeps output open/close and stream-state handling; if start()
// fails the GUI falls back to its inline render loop.
//
// Thread-safety contract: block renders are serialized against transport,
// audition, and song mutations via ApplicationSession::audioStateMutex()
// (RealtimePlaybackSession locks it internally; never hold it across waits).
// The output queue is protected by GuiAudioRuntime's own mutexes.
class GuiAudioProducer {
public:
    GuiAudioProducer(
        ApplicationSession& session,
        GuiAudioRuntime& audioRuntime,
        std::function<bool(const float*, const float*, int)> writeAudioOutput,
        std::function<void(int)> tuneRealtimeAudioForLoad);
    ~GuiAudioProducer();

    GuiAudioProducer(const GuiAudioProducer&) = delete;
    GuiAudioProducer& operator=(const GuiAudioProducer&) = delete;

    bool start(); // false on failure -> GUI uses the inline render fallback
    void stop();  // idempotent; signals and joins
    bool active() const { return running_.load(std::memory_order_relaxed); }

private:
    void threadMain();

    ApplicationSession& session_;
    GuiAudioRuntime& audioRuntime_;
    std::function<bool(const float*, const float*, int)> writeAudioOutput_;
    std::function<void(int)> tuneRealtimeAudioForLoad_;

    // Render-loop state owned by the producer thread (mirrors the GUI-side
    // context state, kept separate so the two paths never share mutable state).
    std::vector<float> audioLeft_;
    std::vector<float> audioRight_;
    std::chrono::steady_clock::time_point lastPlaybackTick_ {};
    double audioPendingFrames_ = 0.0;
    bool previousAudioStreamActive_ = false;
    AppActionResult lastAction_ {};

    std::thread thread_;
    std::atomic<bool> stopRequested_ {false};
    std::atomic<bool> running_ {false};
};

} // namespace arachno
