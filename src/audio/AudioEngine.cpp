#include "AudioEngine.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

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

float softLimit(float value) {
    return std::tanh(value);
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

                const double rowPosition = static_cast<double>(globalRow + row) + step.microOffsetRows;
                const int frame = std::max(0, static_cast<int>(std::llround(rowPosition * secondsPerRow * sampleRate)));
                const Track* trackInfo = track < static_cast<int>(song.tracks.size())
                    ? &song.tracks[static_cast<std::size_t>(track)]
                    : nullptr;

                SynthPatch patch = song.instruments[static_cast<std::size_t>(instrumentIndex)].patch;
                for (const auto& [parameter, value] : step.automation) {
                    setSynthPatchParameter(patch, parameter, value);
                }
                if (trackInfo != nullptr) {
                    patch.gain *= trackInfo->volume;
                }

                events.push_back({
                    frame,
                    track,
                    *step.note,
                    patch,
                    trackInfo != nullptr ? trackInfo->pan : 0.0,
                    step.gate * secondsPerRow
                });
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

        for (int i = 0; i < framesThisBlock; ++i) {
            const std::size_t output = static_cast<std::size_t>(frame + i) * 2;
            rendered.interleavedStereo[output] = softLimit(left[static_cast<std::size_t>(i)]);
            rendered.interleavedStereo[output + 1] = softLimit(right[static_cast<std::size_t>(i)]);
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
