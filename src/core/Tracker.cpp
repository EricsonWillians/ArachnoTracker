#include "Tracker.h"

#include <algorithm>
#include <iterator>
#include <stdexcept>

namespace arachno {

double Song::secondsPerRow() const {
    return 60.0 / bpm / static_cast<double>(rowsPerBeat);
}

int Song::totalRows() const {
    int rows = 0;
    for (int patternIndex : order) {
        if (patternIndex >= 0 && patternIndex < static_cast<int>(patterns.size())) {
            rows += patterns[static_cast<std::size_t>(patternIndex)].rowCount();
        }
    }
    return rows;
}

double Song::durationSeconds() const {
    return static_cast<double>(totalRows()) * secondsPerRow();
}

int Tracker::addTrack(const std::string& name) {
    Track track;
    track.name = name;
    song_.tracks.push_back(track);
    return static_cast<int>(song_.tracks.size() - 1);
}

int Tracker::addInstrument(const SynthPatch& patch) {
    Instrument instrument;
    instrument.id = static_cast<int>(song_.instruments.size());
    instrument.patch = patch;
    song_.instruments.push_back(instrument);
    return instrument.id;
}

int Tracker::addPattern(const Pattern& pattern) {
    song_.patterns.push_back(pattern);
    return static_cast<int>(song_.patterns.size() - 1);
}

void Tracker::appendPatternToOrder(int patternIndex) {
    if (patternIndex < 0 || patternIndex >= static_cast<int>(song_.patterns.size())) {
        throw std::out_of_range("pattern index is out of range");
    }
    song_.order.push_back(patternIndex);
}

Song makeDemoSong() {
    Tracker tracker;
    tracker.song().title = "Arachno Demo";
    tracker.song().bpm = 132.0;
    tracker.song().rowsPerBeat = 4;
    tracker.song().sampleRate = 48000;

    const int bassTrack = tracker.addTrack("Bass");
    const int leadTrack = tracker.addTrack("Lead");
    const int padTrack = tracker.addTrack("Pad");

    tracker.song().tracks[static_cast<std::size_t>(bassTrack)].volume = 0.9;
    tracker.song().tracks[static_cast<std::size_t>(leadTrack)].volume = 0.75;
    tracker.song().tracks[static_cast<std::size_t>(leadTrack)].pan = -0.18;
    tracker.song().tracks[static_cast<std::size_t>(padTrack)].volume = 0.55;
    tracker.song().tracks[static_cast<std::size_t>(padTrack)].pan = 0.2;

    SynthPatch bass;
    bass.name = "Low Saw";
    bass.oscillatorA = Waveform::Saw;
    bass.oscillatorB = Waveform::Square;
    bass.oscillatorMix = 0.22;
    bass.subOscillator = 0.35;
    bass.cutoff = 0.36;
    bass.filterEnvelopeAmount = 0.22;
    bass.drive = 0.22;
    bass.ampEnvelope.attack = 0.002;
    bass.ampEnvelope.decay = 0.07;
    bass.ampEnvelope.sustain = 0.55;
    bass.ampEnvelope.release = 0.08;

    SynthPatch lead;
    lead.name = "Bright Twin";
    lead.oscillatorA = Waveform::Saw;
    lead.oscillatorB = Waveform::Triangle;
    lead.oscillatorMix = 0.42;
    lead.detuneCents = 11.0;
    lead.cutoff = 0.86;
    lead.resonance = 0.18;
    lead.lfoRate = 5.8;
    lead.vibratoCents = 4.0;
    lead.tremoloDepth = 0.08;
    lead.drive = 0.06;
    lead.ampEnvelope.attack = 0.004;
    lead.ampEnvelope.decay = 0.1;
    lead.ampEnvelope.sustain = 0.7;
    lead.ampEnvelope.release = 0.16;

    SynthPatch pad;
    pad.name = "Soft Wide";
    pad.oscillatorA = Waveform::Sine;
    pad.oscillatorB = Waveform::Triangle;
    pad.oscillatorMix = 0.5;
    pad.detuneCents = -8.0;
    pad.cutoff = 0.55;
    pad.lfoRate = 0.45;
    pad.vibratoCents = 2.5;
    pad.tremoloDepth = 0.12;
    pad.ampEnvelope.attack = 0.08;
    pad.ampEnvelope.decay = 0.3;
    pad.ampEnvelope.sustain = 0.78;
    pad.ampEnvelope.release = 0.45;
    pad.gain = 0.42;

    const int bassInstrument = tracker.addInstrument(bass);
    const int leadInstrument = tracker.addInstrument(lead);
    const int padInstrument = tracker.addInstrument(pad);

    Pattern pattern("Opening", 64, static_cast<int>(tracker.song().tracks.size()));
    const int bassNotes[] = {36, 36, 43, 41, 36, 48, 43, 34};
    for (int row = 0; row < 64; row += 8) {
        PatternStep& step = pattern.step(row, bassTrack);
        step.note = Note(bassNotes[(row / 8) % 8], 0.92f);
        step.instrument = bassInstrument;
        step.gate = 0.72;
    }

    const int leadRows[] = {0, 6, 12, 18, 24, 30, 36, 42, 48, 54, 60};
    const int leadNotes[] = {60, 63, 67, 70, 72, 70, 67, 63, 65, 67, 72};
    for (std::size_t i = 0; i < std::size(leadRows); ++i) {
        PatternStep& step = pattern.step(leadRows[i], leadTrack);
        step.note = Note(leadNotes[i], 0.68f);
        step.instrument = leadInstrument;
        step.gate = 0.58;
        step.microOffsetRows = (i % 2 == 0) ? 0.0 : -0.08;
    }

    for (int row = 0; row < 64; row += 16) {
        PatternStep& root = pattern.step(row, padTrack);
        root.note = Note(row == 32 ? 41 : 48, 0.5f);
        root.instrument = padInstrument;
        root.gate = 3.6;

        PatternStep& fifth = pattern.step(row + 1, padTrack);
        fifth.note = Note(row == 32 ? 48 : 55, 0.38f);
        fifth.instrument = padInstrument;
        fifth.gate = 3.4;
        fifth.microOffsetRows = -0.96;
    }

    const int patternIndex = tracker.addPattern(pattern);
    tracker.appendPatternToOrder(patternIndex);
    tracker.appendPatternToOrder(patternIndex);
    return tracker.song();
}

const char* waveformName(Waveform waveform) {
    switch (waveform) {
        case Waveform::Sine: return "sine";
        case Waveform::Square: return "square";
        case Waveform::Saw: return "saw";
        case Waveform::Triangle: return "triangle";
        case Waveform::Noise: return "noise";
    }
    return "sine";
}

Waveform waveformFromName(const std::string& name) {
    if (name == "square") return Waveform::Square;
    if (name == "saw") return Waveform::Saw;
    if (name == "triangle") return Waveform::Triangle;
    if (name == "noise") return Waveform::Noise;
    return Waveform::Sine;
}

} // namespace arachno
