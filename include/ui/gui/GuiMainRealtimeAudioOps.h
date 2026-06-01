#pragma once

#include <chrono>
#include <functional>
#include <vector>

#include "AppActions.h"
#include "ApplicationSession.h"
#include "ui/gui/GuiAudioRuntime.h"

namespace arachno {

struct GuiMainRealtimeAudioContext {
    ApplicationSession& session;
    GuiAudioRuntime& audioRuntime;
    std::vector<float>& audioLeft;
    std::vector<float>& audioRight;
    std::chrono::steady_clock::time_point& lastPlaybackTick;
    double& audioPendingFrames;
    bool& previousAudioStreamActive;
    AppActionResult& lastAction;

    std::function<bool(int)> openAudioOutput;
    std::function<void()> closeAudioOutput;
    std::function<bool(const float*, const float*, int)> writeAudioOutput;
    std::function<void(int)> tuneRealtimeAudioForLoad;
};

void processMainRealtimeAudio(const GuiMainRealtimeAudioContext& context);

} // namespace arachno
