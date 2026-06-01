#include "ui/gui/GuiMainRealtimeAudioOps.h"

#include <algorithm>
#include <cmath>

namespace arachno {

void processMainRealtimeAudio(const GuiMainRealtimeAudioContext& context) {
    const PlaybackSnapshot playback = context.session.playback().snapshot();
    const bool shouldStreamAudio = playback.state == TransportState::Playing || playback.previewActive;
    bool hasLiveOutput = context.audioRuntime.hasOutput();
    if (shouldStreamAudio && (!context.previousAudioStreamActive || !hasLiveOutput)) {
        if (!hasLiveOutput) {
            if (!context.openAudioOutput(playback.sampleRate)) {
                context.lastAction.ok = false;
                context.lastAction.actionId = "audio.output.open";
                context.lastAction.error = "failed to open live audio output (ALSA/aplay)";
            } else {
                hasLiveOutput = true;
            }
        }
    }
    if (!shouldStreamAudio && context.previousAudioStreamActive) {
        context.closeAudioOutput();
    }

    if (shouldStreamAudio) {
        context.tuneRealtimeAudioForLoad(playback.sampleRate);
        const auto nowTick = std::chrono::steady_clock::now();
        const double elapsedSeconds = std::chrono::duration<double>(nowTick - context.lastPlaybackTick).count();
        context.audioPendingFrames += elapsedSeconds * static_cast<double>(playback.sampleRate);
        const double clampCeiling = static_cast<double>(context.audioRuntime.frameMax() * 12);
        if (context.audioPendingFrames > clampCeiling) {
            context.audioPendingFrames = clampCeiling;
        }
        int blocksRendered = 0;
        const int minFrames = std::max(1, context.audioRuntime.frameMin());
        while (((context.audioPendingFrames >= static_cast<double>(minFrames))
                   || (blocksRendered == 0 && context.audioPendingFrames >= 1.0))
            && blocksRendered < 12) {
            const double frameTarget = std::max(context.audioPendingFrames, static_cast<double>(minFrames));
            int frames = std::clamp(
                static_cast<int>(std::llround(frameTarget)),
                minFrames,
                context.audioRuntime.frameMax());
            if (context.audioLeft.size() < static_cast<std::size_t>(frames)) {
                context.audioLeft.resize(static_cast<std::size_t>(frames), 0.0f);
                context.audioRight.resize(static_cast<std::size_t>(frames), 0.0f);
            }
            std::fill(context.audioLeft.begin(), context.audioLeft.begin() + frames, 0.0f);
            std::fill(context.audioRight.begin(), context.audioRight.begin() + frames, 0.0f);
            if (context.session.audioRuntimeHealth().active) {
                const AudioRuntimeProcessResult processed = context.session.renderAudioRuntimeBlock(
                    context.audioLeft.data(),
                    context.audioRight.data(),
                    frames);
                if (!processed.ok) {
                    context.lastAction.ok = false;
                    context.lastAction.actionId = "audio.runtime.render";
                    context.lastAction.error = processed.error;
                    break;
                }
            } else {
                context.session.playback().render(context.audioLeft.data(), context.audioRight.data(), frames);
            }
            if (hasLiveOutput
                && !context.writeAudioOutput(context.audioLeft.data(), context.audioRight.data(), frames)) {
                context.lastAction.ok = false;
                context.lastAction.actionId = "audio.output.write";
                context.lastAction.error = "live audio output stream failed";
                break;
            }
            context.audioPendingFrames -= static_cast<double>(frames);
            if (context.audioPendingFrames < 0.0) {
                context.audioPendingFrames = 0.0;
            }
            ++blocksRendered;
        }
        context.lastPlaybackTick = nowTick;
    } else {
        context.lastPlaybackTick = std::chrono::steady_clock::now();
        context.audioPendingFrames = 0.0;
    }
    context.previousAudioStreamActive = shouldStreamAudio;
}

} // namespace arachno
