#include "AudioEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
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
};

struct MixBusState {
    double gain = 1.0;
    double dcInputLeft = 0.0;
    double dcOutputLeft = 0.0;
    double dcInputRight = 0.0;
    double dcOutputRight = 0.0;
};

float safetySaturate(double value) {
    const double threshold = 0.98;
    const double magnitude = std::abs(value);
    if (magnitude <= threshold) {
        return static_cast<float>(value);
    }
    const double excess = magnitude - threshold;
    const double softened = threshold + std::tanh(excess * 8.0) * 0.02;
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
    constexpr double limiterThreshold = 0.93;
    const double attackCoeff = std::exp(-1.0 / (sampleRate * 0.0006));
    const double releaseCoeff = std::exp(-1.0 / (sampleRate * 0.12));

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

    Synthesizer synth(static_cast<double>(sampleRate));
    std::size_t nextEvent = 0;
    constexpr int blockSize = 128;
    std::vector<float> left(blockSize, 0.0f);
    std::vector<float> right(blockSize, 0.0f);
    MixBusState mixBus;
    const double inputTrim = playbackHeadroomGain(renderableTrackCount);

    for (int frame = 0; frame < totalFrames; frame += blockSize) {
        const int framesThisBlock = std::min(blockSize, totalFrames - frame);
        std::fill(left.begin(), left.begin() + framesThisBlock, 0.0f);
        std::fill(right.begin(), right.begin() + framesThisBlock, 0.0f);

        int cursor = 0;
        while (cursor < framesThisBlock) {
            const int absoluteFrame = frame + cursor;
            while (nextEvent < events.size() && events[nextEvent].frame <= absoluteFrame) {
                synth.noteOn(
                    events[nextEvent].note,
                    events[nextEvent].patch,
                    events[nextEvent].pan,
                    events[nextEvent].gateSeconds);
                ++nextEvent;
            }

            int segmentEnd = framesThisBlock;
            if (nextEvent < events.size()) {
                segmentEnd = std::min(segmentEnd, std::max(cursor + 1, events[nextEvent].frame - frame));
            }

            synth.render(left.data() + cursor, right.data() + cursor, segmentEnd - cursor);
            cursor = segmentEnd;
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
