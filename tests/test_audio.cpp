#include <cassert>

#include "AudioEngine.h"
#include "Exporter.h"
#include "Tracker.h"

void testRenderDemoSong() {
    const arachno::Song song = arachno::makeDemoSong();
    arachno::AudioEngine engine(song.sampleRate);
    const arachno::RenderedAudio audio = engine.renderSong(song);

    assert(audio.sampleRate == 48000);
    assert(audio.frameCount() > 0);
    assert(audio.interleavedStereo.size() % 2 == 0);

    bool hasSignal = false;
    for (float sample : audio.interleavedStereo) {
        if (sample > 0.001f || sample < -0.001f) {
            hasSignal = true;
            break;
        }
    }
    assert(hasSignal);
}
