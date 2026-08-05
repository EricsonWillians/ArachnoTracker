#include "AudioEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

#include "StepEffects.h"
#include "Synthesizer.h"

namespace arachno {

namespace {
struct ScheduledNote {
    int frame = 0;
    int track = 0;
    Note note;
    SynthPatch patch;
    double pan = 0.0;
    double gateSeconds = 0.2;
    // Note-off event: releases the last note started on this track.
    bool noteOff = false;
};

struct MixBusState {
    double gain = 1.0;
    double dcInputLeft = 0.0;
    double dcOutputLeft = 0.0;
    double dcInputRight = 0.0;
    double dcOutputRight = 0.0;
};

float safetySaturate(double value) {
    const double threshold = 0.992;
    const double magnitude = std::abs(value);
    if (magnitude <= threshold) {
        return static_cast<float>(value);
    }
    const double excess = magnitude - threshold;
    const double softened = threshold + std::tanh(excess * 10.0) * 0.018;
    return static_cast<float>(std::copysign(std::min(0.999, softened), value));
}

double approach(double current, double target, double coefficient) {
    return target + (current - target) * coefficient;
}

double dcBlock(double sample, double& previousInput, double& previousOutput) {
    constexpr double coefficient = 0.995;
    const double output = sample - previousInput + coefficient * previousOutput;
    previousInput = sample;
    previousOutput = output;
    if (std::abs(output) < 1e-20) {
        return 0.0;
    }
    return output;
}

int countRenderableTracks(const Song& song, const RenderOptions& options, bool hasSoloTrack) {
    int count = 0;
    for (int track = 0; track < static_cast<int>(song.tracks.size()); ++track) {
        if (options.soloTrack >= 0) {
            if (track == options.soloTrack) {
                ++count;
            }
            continue;
        }
        const Track& trackInfo = song.tracks[static_cast<std::size_t>(track)];
        if (!options.includeMutedTracks && trackInfo.muted) {
            continue;
        }
        if (!hasSoloTrack || trackInfo.solo) {
            ++count;
        }
    }
    return count;
}

double playbackHeadroomGain(int renderableTrackCount) {
    const double trackCount = std::max(1.0, static_cast<double>(renderableTrackCount));
    const double effectiveTracks = 1.0 + (trackCount - 1.0) * 0.18;
    return 1.0 / std::sqrt(effectiveTracks);
}

void applyMixBus(
    float* left,
    float* right,
    int sampleCount,
    int sampleRate,
    double inputTrim,
    MixBusState& state) {
    if (sampleCount <= 0) {
        return;
    }
    constexpr double limiterThreshold = 0.96;
    const double attackCoeff = std::exp(-1.0 / (sampleRate * 0.0010));
    const double releaseCoeff = std::exp(-1.0 / (sampleRate * 0.16));

    for (int sample = 0; sample < sampleCount; ++sample) {
        double inLeft = static_cast<double>(left[sample]) * inputTrim;
        double inRight = static_cast<double>(right[sample]) * inputTrim;
        const double peak = std::max(std::abs(inLeft), std::abs(inRight));
        const double targetGain = peak > limiterThreshold
            ? limiterThreshold / std::max(peak, 1e-9)
            : 1.0;
        const double coeff = targetGain < state.gain ? attackCoeff : releaseCoeff;
        state.gain = approach(state.gain, targetGain, coeff);
        inLeft *= state.gain;
        inRight *= state.gain;
        inLeft = dcBlock(inLeft, state.dcInputLeft, state.dcOutputLeft);
        inRight = dcBlock(inRight, state.dcInputRight, state.dcOutputRight);
        left[sample] = safetySaturate(inLeft);
        right[sample] = safetySaturate(inRight);
    }
}

bool songHasSoloTrack(const Song& song) {
    return std::any_of(song.tracks.begin(), song.tracks.end(), [](const Track& track) {
        return track.solo;
    });
}

bool shouldRenderTrack(const Song& song, int track, const RenderOptions& options, bool hasSoloTrack) {
    if (track < 0 || track >= static_cast<int>(song.tracks.size())) {
        return false;
    }

    if (options.soloTrack >= 0) {
        return track == options.soloTrack;
    }

    const Track& trackInfo = song.tracks[static_cast<std::size_t>(track)];
    if (!options.includeMutedTracks && trackInfo.muted) {
        return false;
    }

    return !hasSoloTrack || trackInfo.solo;
}

double deterministicUnitValue(int patternIndex, int globalRow, int row, int track) {
    std::uint32_t value = 2166136261u;
    value = (value ^ static_cast<std::uint32_t>(patternIndex + 4099)) * 16777619u;
    value = (value ^ static_cast<std::uint32_t>(globalRow + 8191)) * 16777619u;
    value = (value ^ static_cast<std::uint32_t>(row + 131071)) * 16777619u;
    value = (value ^ static_cast<std::uint32_t>(track + 524287)) * 16777619u;
    return static_cast<double>(value & 0x00ffffffu) / static_cast<double>(0x01000000u);
}

bool shouldTriggerStep(const PatternStep& step, int patternIndex, int globalRow, int row, int track) {
    if (!step.probability.has_value()) {
        return true;
    }
    const double probability = std::clamp(step.probability.value(), 0.0, 1.0);
    if (probability <= 0.0) {
        return false;
    }
    if (probability >= 1.0) {
        return true;
    }
    return deterministicUnitValue(patternIndex, globalRow, row, track) < probability;
}
} // namespace

AudioEngine::AudioEngine(int sampleRate) : sampleRate_(sampleRate) {
    if (sampleRate_ <= 0) {
        throw std::invalid_argument("sample rate must be positive");
    }
}

RenderedAudio AudioEngine::renderSong(const Song& song) const {
    return renderSong(song, RenderOptions {});
}

RenderedAudio AudioEngine::renderSong(const Song& song, const RenderOptions& options) const {
    const int sampleRate = song.sampleRate > 0 ? song.sampleRate : sampleRate_;
    const double secondsPerRow = song.secondsPerRow();
    const int tailFrames = static_cast<int>(sampleRate * 1.2);
    const int totalFrames = static_cast<int>(std::ceil(song.durationSeconds() * sampleRate)) + tailFrames;

    RenderedAudio rendered;
    rendered.sampleRate = sampleRate;
    rendered.interleavedStereo.assign(static_cast<std::size_t>(totalFrames) * 2, 0.0f);

    std::vector<ScheduledNote> events;
    int globalRow = 0;
    const bool hasSoloTrack = songHasSoloTrack(song);
    const int renderableTrackCount = countRenderableTracks(song, options, hasSoloTrack);

    for (int patternIndex : song.order) {
        if (patternIndex < 0 || patternIndex >= static_cast<int>(song.patterns.size())) {
            continue;
        }

        const Pattern& pattern = song.patterns[static_cast<std::size_t>(patternIndex)];
        for (int row = 0; row < pattern.rowCount(); ++row) {
            for (int track = 0; track < pattern.trackCount(); ++track) {
                if (!shouldRenderTrack(song, track, options, hasSoloTrack)) {
                    continue;
                }

                const PatternStep& step = pattern.step(row, track);
                if (!step.note.has_value()) {
                    if (step.noteOff) {
                        const int frame = std::max(0, static_cast<int>(std::llround(
                            (static_cast<double>(globalRow + row) + step.microOffsetRows)
                                * secondsPerRow * sampleRate)));
                        ScheduledNote event;
                        event.frame = frame;
                        event.track = track;
                        event.noteOff = true;
                        events.push_back(event);
                    }
                    continue;
                }

                const int instrumentIndex = step.instrument;
                if (instrumentIndex < 0 || instrumentIndex >= static_cast<int>(song.instruments.size())) {
                    continue;
                }

                if (!shouldTriggerStep(step, patternIndex, globalRow, row, track)) {
                    continue;
                }

                const Track* trackInfo = track < static_cast<int>(song.tracks.size())
                    ? &song.tracks[static_cast<std::size_t>(track)]
                    : nullptr;

                StepSynthesisState state;
                state.note = *step.note;
                state.patch = song.instruments[static_cast<std::size_t>(instrumentIndex)].patch;
                state.gateRows = step.gate;
                state.microOffsetRows = step.microOffsetRows;
                state.pan = trackInfo != nullptr ? trackInfo->pan : 0.0;
                if (!applyStepSynthesisState(step, state) || state.muted) {
                    continue;
                }
                if (trackInfo != nullptr) {
                    state.patch.gain *= trackInfo->volume;
                }

                const int retriggerCount = std::max(1, step.retriggerCount);
                const double spacingRows = std::max(0.001, step.retriggerSpacingRows);
                double velocityScale = 1.0;
                for (int repeat = 0; repeat < retriggerCount; ++repeat) {
                    const double rowPosition = static_cast<double>(globalRow + row)
                        + state.microOffsetRows
                        + static_cast<double>(repeat) * spacingRows;
                    const int frame = std::max(0, static_cast<int>(std::llround(rowPosition * secondsPerRow * sampleRate)));
                    Note note = state.note;
                    note.velocity = static_cast<float>(std::clamp(
                        static_cast<double>(note.velocity) * velocityScale,
                        0.0,
                        1.0));
                    const double gateRows = retriggerCount > 1
                        ? std::min(state.gateRows, spacingRows * 0.85)
                        : state.gateRows;
                    events.push_back({
                        frame,
                        track,
                        note,
                        state.patch,
                        state.pan,
                        gateRows * secondsPerRow
                    });
                    velocityScale *= std::clamp(step.retriggerVelocityDecay, 0.0, 1.0);
                }
            }
        }
        globalRow += pattern.rowCount();
    }

    std::sort(events.begin(), events.end(), [](const ScheduledNote& lhs, const ScheduledNote& rhs) {
        return lhs.frame < rhs.frame;
    });

    const unsigned int hwConcurrency = std::max(1u, std::thread::hardware_concurrency());
    const int maxParallelBuses = std::max(1, std::min(4, static_cast<int>(hwConcurrency)));
    const int desiredBuses = std::max(1, std::min(maxParallelBuses, std::max(1, renderableTrackCount)));
    const bool parallelBusRender = desiredBuses > 1 && events.size() > 96;
    const int busCount = parallelBusRender ? desiredBuses : 1;

    std::vector<std::vector<ScheduledNote>> busEvents(static_cast<std::size_t>(busCount));
    int maxEventTrack = -1;
    for (const ScheduledNote& event : events) {
        const int bus = busCount <= 1 ? 0 : std::clamp(event.track, 0, std::numeric_limits<int>::max()) % busCount;
        busEvents[static_cast<std::size_t>(bus)].push_back(event);
        maxEventTrack = std::max(maxEventTrack, event.track);
    }
    // Last started note per track, maintained independently per bus (each track maps to one bus).
    std::vector<std::vector<int>> busLastNoteForTrack(
        static_cast<std::size_t>(busCount),
        std::vector<int>(static_cast<std::size_t>(maxEventTrack + 1), -1));
    std::vector<Synthesizer> busSynths;
    busSynths.reserve(static_cast<std::size_t>(busCount));
    for (int bus = 0; bus < busCount; ++bus) {
        busSynths.emplace_back(static_cast<double>(sampleRate));
        // Offline mixdown: always render at full quality (no load-adaptive degradation).
        busSynths.back().setOfflineRendering(true);
    }
    std::vector<std::size_t> busNextEvent(static_cast<std::size_t>(busCount), 0);

    constexpr int blockSize = 128;
    std::vector<float> left(blockSize, 0.0f);
    std::vector<float> right(blockSize, 0.0f);
    std::vector<std::vector<float>> busLeft(
        static_cast<std::size_t>(busCount),
        std::vector<float>(static_cast<std::size_t>(blockSize), 0.0f));
    std::vector<std::vector<float>> busRight(
        static_cast<std::size_t>(busCount),
        std::vector<float>(static_cast<std::size_t>(blockSize), 0.0f));
    MixBusState mixBus;
    const double inputTrim = playbackHeadroomGain(renderableTrackCount);

    for (int frame = 0; frame < totalFrames; frame += blockSize) {
        const int framesThisBlock = std::min(blockSize, totalFrames - frame);
        std::fill(left.begin(), left.begin() + framesThisBlock, 0.0f);
        std::fill(right.begin(), right.begin() + framesThisBlock, 0.0f);
        auto renderBus = [&](int bus) {
            std::vector<float>& busL = busLeft[static_cast<std::size_t>(bus)];
            std::vector<float>& busR = busRight[static_cast<std::size_t>(bus)];
            std::fill(busL.begin(), busL.begin() + framesThisBlock, 0.0f);
            std::fill(busR.begin(), busR.begin() + framesThisBlock, 0.0f);
            Synthesizer& synth = busSynths[static_cast<std::size_t>(bus)];
            std::size_t& nextEvent = busNextEvent[static_cast<std::size_t>(bus)];
            const std::vector<ScheduledNote>& eventsForBus = busEvents[static_cast<std::size_t>(bus)];
            std::vector<int>& lastNoteForTrack = busLastNoteForTrack[static_cast<std::size_t>(bus)];

            int cursor = 0;
            while (cursor < framesThisBlock) {
                const int absoluteFrame = frame + cursor;
                while (nextEvent < eventsForBus.size() && eventsForBus[nextEvent].frame <= absoluteFrame) {
                    const ScheduledNote& event = eventsForBus[nextEvent];
                    if (event.noteOff) {
                        int& lastNote = lastNoteForTrack[static_cast<std::size_t>(event.track)];
                        if (lastNote >= 0) {
                            synth.noteOff(lastNote, event.track);
                            lastNote = -1;
                        }
                    } else {
                        synth.noteOn(
                            event.note,
                            event.patch,
                            event.pan,
                            event.gateSeconds,
                            -1,
                            false,
                            event.track);
                        lastNoteForTrack[static_cast<std::size_t>(event.track)] = event.note.midi;
                    }
                    ++nextEvent;
                }

                int segmentEnd = framesThisBlock;
                if (nextEvent < eventsForBus.size()) {
                    segmentEnd = std::min(segmentEnd, std::max(cursor + 1, eventsForBus[nextEvent].frame - frame));
                }

                synth.render(busL.data() + cursor, busR.data() + cursor, segmentEnd - cursor);
                cursor = segmentEnd;
            }
        };

        if (parallelBusRender) {
            std::vector<std::thread> workers;
            workers.reserve(static_cast<std::size_t>(busCount));
            for (int bus = 0; bus < busCount; ++bus) {
                workers.emplace_back([&, bus]() {
                    renderBus(bus);
                });
            }
            for (std::thread& worker : workers) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
        } else {
            renderBus(0);
        }

        for (int bus = 0; bus < busCount; ++bus) {
            const std::vector<float>& busL = busLeft[static_cast<std::size_t>(bus)];
            const std::vector<float>& busR = busRight[static_cast<std::size_t>(bus)];
            for (int index = 0; index < framesThisBlock; ++index) {
                left[static_cast<std::size_t>(index)] += busL[static_cast<std::size_t>(index)];
                right[static_cast<std::size_t>(index)] += busR[static_cast<std::size_t>(index)];
            }
        }

        applyMixBus(left.data(), right.data(), framesThisBlock, sampleRate, inputTrim, mixBus);
        for (int i = 0; i < framesThisBlock; ++i) {
            const std::size_t output = static_cast<std::size_t>(frame + i) * 2;
            rendered.interleavedStereo[output] = left[static_cast<std::size_t>(i)];
            rendered.interleavedStereo[output + 1] = right[static_cast<std::size_t>(i)];
        }
    }

    return rendered;
}

RenderedAudio AudioEngine::renderTrackStem(const Song& song, int trackIndex) const {
    if (trackIndex < 0 || trackIndex >= static_cast<int>(song.tracks.size())) {
        throw std::out_of_range("track index is out of range");
    }

    RenderOptions options;
    options.soloTrack = trackIndex;
    options.includeMutedTracks = true;
    return renderSong(song, options);
}

} // namespace arachno
