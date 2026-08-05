#include "ui/gui/GuiMainRealtimeAudioOps.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace arachno {

void processMainRealtimeAudio(const GuiMainRealtimeAudioContext& context) {
    const PlaybackSnapshot playback = context.session.playback().snapshot();
    const bool shouldStreamAudio = playback.state == TransportState::Playing || playback.previewActive;
    bool hasLiveOutput = context.audioRuntime.hasOutput();
    const bool streamStarting = shouldStreamAudio && !context.previousAudioStreamActive;
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
        if (!context.audioProducerActive) {
            // Inline fallback: render on the GUI thread only when no dedicated
            // producer thread is running (producer startup failed).
            (void)renderMainRealtimeAudioBlocks(context);
        }
    } else {
        context.lastPlaybackTick = std::chrono::steady_clock::now();
        context.audioPendingFrames = 0.0;
    }
    context.previousAudioStreamActive = shouldStreamAudio;
}

int renderMainRealtimeAudioBlocks(const GuiMainRealtimeAudioContext& context) {
    const PlaybackSnapshot playback = context.session.playback().snapshot();
    const bool streamStarting = !context.previousAudioStreamActive;
    const bool hasLiveOutput = context.audioRuntime.hasOutput();
    if (streamStarting) {
        context.audioPendingFrames = 0.0;
    }
    context.tuneRealtimeAudioForLoad(playback.sampleRate);
        if (context.audioRuntime.performanceMode() == AudioPerformanceMode::Auto) {
            const int outputPressure = hasLiveOutput ? context.audioRuntime.outputPressureLevel() : 0;
            int pressureTrackEstimate = 0;
            if (playback.synth.underrunRisk >= 0.78
                || playback.synth.dspLoadPercent >= 88.0
                || playback.synth.activeVoices >= 28
                || outputPressure >= 48) {
                pressureTrackEstimate = 40;
            } else if (playback.synth.underrunRisk >= 0.60
                || playback.synth.dspLoadPercent >= 72.0
                || playback.synth.activeVoices >= 20
                || outputPressure >= 28) {
                pressureTrackEstimate = 24;
            } else if (playback.synth.underrunRisk >= 0.46
                || playback.synth.dspLoadPercent >= 60.0
                || playback.synth.activeVoices >= 14
                || outputPressure >= 16) {
                pressureTrackEstimate = 14;
            }
            if (pressureTrackEstimate > 0) {
                context.audioRuntime.tuneForLoad(playback.sampleRate, pressureTrackEstimate);
            }
        }
        const auto nowTick = std::chrono::steady_clock::now();
        const auto renderBudgetStart = nowTick;
        const int outputPressureLevel = hasLiveOutput ? context.audioRuntime.outputPressureLevel() : 0;
        const double elapsedSeconds = std::chrono::duration<double>(nowTick - context.lastPlaybackTick).count();
        const bool highPressure = playback.synth.underrunRisk >= 0.58
            || playback.synth.dspLoadPercent >= 78.0
            || context.audioRuntime.loadClass() >= 2
            || outputPressureLevel >= 24;
        const bool criticalPressure = playback.synth.underrunRisk >= 0.80
            || playback.synth.dspLoadPercent >= 90.0
            || playback.synth.activeVoices >= 30
            || outputPressureLevel >= 52;
        const double maxElapsedSeconds = criticalPressure ? 0.060 : (highPressure ? 0.080 : 0.100);
        const bool schedulingStall = elapsedSeconds > maxElapsedSeconds;
        const double elapsedFrames = elapsedSeconds * static_cast<double>(playback.sampleRate);
        const int minFrames = std::max(1, context.audioRuntime.frameMin());
        const int maxFrames = std::max(minFrames, context.audioRuntime.frameMax());
        const int startupBoostFrames = maxFrames * (highPressure ? 2 : 1);
        int alsaQueuedFrames = 0;
        int alsaCapacityFrames = 0;
        int pipeQueuedFrames = 0;
        int pipeCapacityFrames = 0;
        int dynamicTargetQueueFrames = std::clamp(
            static_cast<int>(std::lround(static_cast<double>(playback.sampleRate)
                * (criticalPressure ? 0.18 : (highPressure ? 0.13 : 0.09)))),
            minFrames * 2,
            maxFrames * 4);
        if (outputPressureLevel >= 40) {
            dynamicTargetQueueFrames = std::max(dynamicTargetQueueFrames, minFrames * 3);
        }
        if (context.audioRuntime.usesAlsa()) {
            const auto [queuedSamples, queueCapacitySamples] = context.audioRuntime.alsaQueueUsage();
            alsaQueuedFrames = static_cast<int>(queuedSamples / 2);
            alsaCapacityFrames = static_cast<int>(queueCapacitySamples / 2);
        } else if (hasLiveOutput) {
            const auto [queuedSamples, queueCapacitySamples] = context.audioRuntime.pipeQueueUsage();
            pipeQueuedFrames = static_cast<int>(queuedSamples / 2);
            pipeCapacityFrames = static_cast<int>(queueCapacitySamples / 2);
        }
        const int queueCapacityFrames = context.audioRuntime.usesAlsa() ? alsaCapacityFrames : pipeCapacityFrames;
        if (queueCapacityFrames > 0) {
            const int targetCapacityFraction = criticalPressure ? 4 : 3;
            const int targetCapacityDivisor = criticalPressure ? 5 : 5;
            dynamicTargetQueueFrames = std::min(
                dynamicTargetQueueFrames,
                std::max(minFrames * 2, (queueCapacityFrames * targetCapacityFraction) / targetCapacityDivisor));
        }
        const double clampCeiling = static_cast<double>(std::max(
            dynamicTargetQueueFrames + maxFrames * 2,
            minFrames * 4));

        if (context.audioRuntime.usesAlsa()) {
            const int queuedFrames = alsaQueuedFrames;
            const int capacityFrames = alsaCapacityFrames;
            if (capacityFrames > 0) {
                int queueDeficitFrames = std::max(0, dynamicTargetQueueFrames - queuedFrames);
                if (streamStarting) {
                    queueDeficitFrames += startupBoostFrames;
                }
                const double desiredPending = static_cast<double>(queueDeficitFrames);
                context.audioPendingFrames = (context.audioPendingFrames * 0.4) + (desiredPending * 0.6);
                if (schedulingStall) {
                    context.audioPendingFrames = std::min(
                        context.audioPendingFrames,
                        static_cast<double>(dynamicTargetQueueFrames + maxFrames));
                }
            } else {
                context.audioPendingFrames += elapsedFrames;
                if (streamStarting) {
                    context.audioPendingFrames += static_cast<double>(startupBoostFrames);
                }
            }
        } else if (hasLiveOutput) {
            const int queuedFrames = pipeQueuedFrames;
            const int capacityFrames = pipeCapacityFrames;
            if (capacityFrames > 0) {
                int queueDeficitFrames = std::max(0, dynamicTargetQueueFrames - queuedFrames);
                if (streamStarting) {
                    queueDeficitFrames += startupBoostFrames;
                }
                const double desiredPending = static_cast<double>(queueDeficitFrames);
                context.audioPendingFrames = (context.audioPendingFrames * 0.4) + (desiredPending * 0.6);
                if (schedulingStall) {
                    context.audioPendingFrames = std::min(
                        context.audioPendingFrames,
                        static_cast<double>(dynamicTargetQueueFrames + maxFrames));
                }
            } else {
                context.audioPendingFrames += elapsedFrames;
                if (streamStarting) {
                    context.audioPendingFrames += static_cast<double>(startupBoostFrames);
                }
            }
        } else {
            context.audioPendingFrames += elapsedFrames;
            if (streamStarting) {
                context.audioPendingFrames += static_cast<double>(startupBoostFrames);
            }
        }
        if (context.audioRuntime.usesAlsa()) {
            const int queuedFrames = alsaQueuedFrames;
            const int capacityFrames = alsaCapacityFrames;
            if (capacityFrames > 0) {
                const int floorTarget = std::max(
                    minFrames,
                    dynamicTargetQueueFrames / 2);
                if (queuedFrames < floorTarget) {
                    context.audioPendingFrames += static_cast<double>(floorTarget - queuedFrames);
                }
            }
        } else if (hasLiveOutput) {
            const int queuedFrames = pipeQueuedFrames;
            const int capacityFrames = pipeCapacityFrames;
            if (capacityFrames > 0) {
                const int floorTarget = std::max(
                    minFrames,
                    dynamicTargetQueueFrames / 2);
                if (queuedFrames < floorTarget) {
                    context.audioPendingFrames += static_cast<double>(floorTarget - queuedFrames);
                }
            }
        }
        if (context.audioPendingFrames > clampCeiling) {
            context.audioPendingFrames = clampCeiling;
        }
        if (context.audioPendingFrames < 0.0) {
            context.audioPendingFrames = 0.0;
        }
        int blocksRendered = 0;
        int framesRenderedThisTick = 0;
        const int queueLevelFrames = context.audioRuntime.usesAlsa() ? alsaQueuedFrames : pipeQueuedFrames;
        const bool queueStarved = hasLiveOutput && queueCapacityFrames > 0
            && queueLevelFrames < std::max(
                minFrames * 2,
                dynamicTargetQueueFrames / 2);
        const int maxBlocksPerTick = queueStarved ? (criticalPressure ? 64 : 48) : (highPressure ? 24 : 12);
        const int maxFramesPerTick = maxFrames
            * (queueStarved ? (criticalPressure ? 8 : 6) : (highPressure ? 4 : 3));
        const int preferredChunkFrames = std::clamp(
            minFrames * (queueStarved ? (criticalPressure ? 8 : 6) : (highPressure ? 4 : 3)),
            minFrames,
            // Cap per-block frames so a single render call (which holds the
            // audio-state mutex for its whole duration) stays bounded —
            // 4096-frame blocks meant up to ~85 ms of note-on latency for
            // keyboard/MIDI audition under load.
            std::min(maxFrames, queueStarved ? 2048 : 1024));
        const auto maxTickRenderBudget = queueStarved
            ? std::chrono::milliseconds(criticalPressure ? 16 : 12)
            : std::chrono::milliseconds(highPressure ? 8 : 6);
        int queueHeadroomFramesHint = std::numeric_limits<int>::max();
        const bool hasQueueCapacity = hasLiveOutput && queueCapacityFrames > 0;
        if (hasQueueCapacity) {
            const int queueGuardFrames = std::clamp(
                minFrames * 2,
                minFrames,
                std::max(minFrames, queueCapacityFrames / 6));
            queueHeadroomFramesHint = std::max(0, queueCapacityFrames - queueLevelFrames - queueGuardFrames);
        }
        while (((context.audioPendingFrames >= static_cast<double>(minFrames))
                   || (blocksRendered == 0 && context.audioPendingFrames >= 1.0))
            && blocksRendered < maxBlocksPerTick
            && framesRenderedThisTick < maxFramesPerTick) {
            int frames = 0;
            const double frameTarget = std::max(context.audioPendingFrames, static_cast<double>(minFrames));
            frames = std::clamp(
                static_cast<int>(std::llround(frameTarget)),
                minFrames,
                preferredChunkFrames);
            const int remainingBudget = std::max(0, maxFramesPerTick - framesRenderedThisTick);
            if (remainingBudget > 0) {
                frames = std::min(frames, remainingBudget);
            }
            if (frames < minFrames && blocksRendered > 0) {
                break;
            }
            frames = std::max(1, frames);
            if (hasQueueCapacity) {
                const bool refreshHeadroom = blocksRendered == 0
                    || queueHeadroomFramesHint < minFrames
                    || (blocksRendered % 3) == 0;
                if (refreshHeadroom) {
                    const auto [queuedSamplesNow, capacitySamplesNow] = context.audioRuntime.usesAlsa()
                        ? context.audioRuntime.alsaQueueUsage()
                        : context.audioRuntime.pipeQueueUsage();
                    const int queuedFramesNow = static_cast<int>(queuedSamplesNow / 2);
                    const int capacityFramesNow = static_cast<int>(capacitySamplesNow / 2);
                    const int queueGuardFrames = std::clamp(
                        minFrames * 2,
                        minFrames,
                        std::max(minFrames, capacityFramesNow / 6));
                    queueHeadroomFramesHint = std::max(0, capacityFramesNow - queuedFramesNow - queueGuardFrames);
                }
                if (queueHeadroomFramesHint <= 0) {
                    break;
                }
                frames = std::min(frames, queueHeadroomFramesHint);
                if (frames < minFrames && blocksRendered > 0) {
                    break;
                }
            }
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
            if (hasQueueCapacity && queueHeadroomFramesHint != std::numeric_limits<int>::max()) {
                queueHeadroomFramesHint = std::max(0, queueHeadroomFramesHint - frames);
            }
            framesRenderedThisTick += frames;
            ++blocksRendered;
            if (std::chrono::steady_clock::now() - renderBudgetStart >= maxTickRenderBudget) {
                break;
            }
        }
    context.lastPlaybackTick = nowTick;
    context.previousAudioStreamActive = true;
    return framesRenderedThisTick;
}

} // namespace arachno
