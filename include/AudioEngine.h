#pragma once

#include <vector>

#include "Tracker.h"

namespace arachno {

struct RenderedAudio {
    int sampleRate = 48000;
    std::vector<float> interleavedStereo;

    int frameCount() const { return static_cast<int>(interleavedStereo.size() / 2); }
};

struct RenderOptions {
    int soloTrack = -1;
    bool includeMutedTracks = false;
};

class AudioEngine {
public:
    explicit AudioEngine(int sampleRate = 48000);

    RenderedAudio renderSong(const Song& song) const;
    RenderedAudio renderSong(const Song& song, const RenderOptions& options) const;
    RenderedAudio renderTrackStem(const Song& song, int trackIndex) const;

private:
    int sampleRate_ = 48000;
};

} // namespace arachno
