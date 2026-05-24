#pragma once

#include <vector>

#include "Tracker.h"

namespace arachno {

struct RenderedAudio {
    int sampleRate = 48000;
    std::vector<float> interleavedStereo;

    int frameCount() const { return static_cast<int>(interleavedStereo.size() / 2); }
};

class AudioEngine {
public:
    explicit AudioEngine(int sampleRate = 48000);

    RenderedAudio renderSong(const Song& song) const;

private:
    int sampleRate_ = 48000;
};

} // namespace arachno
