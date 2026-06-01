#include "Tracker.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <iterator>
#include <string>
#include <stdexcept>
#include <vector>

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

namespace {

std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

Pattern buildOpeningPattern(
    int trackCount,
    int bassTrack,
    int drumTrack,
    int leadTrack,
    int padTrack,
    int bassInstrument,
    int kickInstrument,
    int snareInstrument,
    int hatInstrument,
    int leadInstrument,
    int padInstrument,
    int arpInstrument,
    int clapInstrument,
    int tomInstrument,
    int rideInstrument,
    int acidInstrument,
    int bellInstrument,
    int choirInstrument,
    int reeseInstrument,
    int rimInstrument) {
    Pattern pattern("Opening", 64, trackCount);
    const int bassNotes[] = {36, 36, 43, 41, 36, 48, 43, 34};
    for (int row = 0; row < 64; row += 8) {
        PatternStep& step = pattern.step(row, bassTrack);
        step.note = Note(bassNotes[(row / 8) % 8], 0.92f);
        step.instrument = bassInstrument;
        step.gate = 0.72;
        if (row % 16 == 8) {
            step.automation["cutoff"] = 0.42;
        }
    }

    for (int row = 0; row < 64; row += 4) {
        PatternStep& step = pattern.step(row, drumTrack);
        step.note = Note(36, row % 16 == 0 ? 1.0f : 0.82f);
        step.instrument = kickInstrument;
        step.gate = 0.35;
    }
    for (int row = 8; row < 64; row += 16) {
        PatternStep& step = pattern.step(row, drumTrack);
        step.note = Note(40, 0.8f);
        step.instrument = snareInstrument;
        step.gate = 0.55;
        step.retriggerCount = 2;
        step.retriggerSpacingRows = 0.18;
        step.retriggerVelocityDecay = 0.55;
    }
    for (int row = 2; row < 64; row += 4) {
        PatternStep& step = pattern.step(row, drumTrack);
        step.note = Note(72, row % 8 == 2 ? 0.46f : 0.34f);
        step.instrument = hatInstrument;
        step.gate = 0.18;
        step.microOffsetRows = row % 8 == 6 ? -0.04 : 0.0;
        if (row % 16 == 14) {
            step.probability = 0.62;
            step.retriggerCount = 3;
            step.retriggerSpacingRows = 0.12;
            step.retriggerVelocityDecay = 0.72;
        }
    }

    for (int row = 12; row < 64; row += 16) {
        PatternStep& clap = pattern.step(row, drumTrack);
        clap.note = Note(70, 0.72f);
        clap.instrument = clapInstrument;
        clap.gate = 0.44;
    }
    for (int row = 30; row < 64; row += 32) {
        PatternStep& tomA = pattern.step(row, drumTrack);
        tomA.note = Note(52, 0.76f);
        tomA.instrument = tomInstrument;
        tomA.gate = 0.42;
        tomA.probability = 0.78;

        PatternStep& tomB = pattern.step(row + 1, drumTrack);
        tomB.note = Note(48, 0.68f);
        tomB.instrument = tomInstrument;
        tomB.gate = 0.38;
        tomB.retriggerCount = 2;
        tomB.retriggerSpacingRows = 0.16;
    }
    for (int row = 15; row < 64; row += 16) {
        PatternStep& ride = pattern.step(row, drumTrack);
        ride.note = Note(78, 0.36f);
        ride.instrument = rideInstrument;
        ride.gate = 0.28;
        ride.probability = 0.55;
    }
    for (int row = 7; row < 64; row += 16) {
        PatternStep& rim = pattern.step(row, drumTrack);
        rim.note = Note(62, 0.52f);
        rim.instrument = rimInstrument;
        rim.gate = 0.2;
        rim.probability = 0.74;
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

    for (int row = 4; row < 64; row += 8) {
        PatternStep& arp = pattern.step(row, leadTrack);
        arp.note = Note(72 + ((row / 8) % 4), 0.42f);
        arp.instrument = arpInstrument;
        arp.gate = 0.22;
        arp.automation["vibrato"] = 8.0;
    }
    const int acidRows[] = {2, 10, 26, 34, 50, 58};
    const int acidNotes[] = {48, 51, 55, 58, 55, 51};
    for (std::size_t i = 0; i < std::size(acidRows); ++i) {
        PatternStep& step = pattern.step(acidRows[i], leadTrack);
        step.note = Note(acidNotes[i], 0.5f);
        step.instrument = acidInstrument;
        step.gate = 0.36;
        step.automation["cutoff"] = 0.46 + static_cast<double>(i % 3) * 0.08;
    }
    const int reeseRows[] = {14, 38};
    const int reeseNotes[] = {39, 41};
    for (std::size_t i = 0; i < std::size(reeseRows); ++i) {
        PatternStep& step = pattern.step(reeseRows[i], leadTrack);
        step.note = Note(reeseNotes[i], 0.56f);
        step.instrument = reeseInstrument;
        step.gate = 0.82;
        step.automation["drive"] = 0.28;
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
    for (int row = 8; row < 64; row += 16) {
        PatternStep& bell = pattern.step(row, padTrack);
        bell.note = Note((row == 40) ? 79 : 76, 0.44f);
        bell.instrument = bellInstrument;
        bell.gate = 1.8;
    }
    for (int row = 12; row < 64; row += 16) {
        PatternStep& choir = pattern.step(row, padTrack);
        choir.note = Note((row == 44) ? 60 : 55, 0.38f);
        choir.instrument = choirInstrument;
        choir.gate = 2.6;
    }

    return pattern;
}

Song buildEbmTemplate() {
    Song song = makeDemoSong();
    song.title = "Factory Pulse";
    song.author = "ArachnoTracker";
    song.description = "EBM-oriented template with tighter low-end and mechanical percussion.";
    song.notes = "Template: hard kick pulse, sync bassline, and gated stabs for arrangement sketching.";
    song.bpm = 126.0;
    song.rowsPerBeat = 4;
    if (!song.patterns.empty()) {
        Pattern pattern("Pulse Grid", 64, static_cast<int>(song.tracks.size()));
        const int bassInst = 0;
        const int kickInst = 1;
        const int snareInst = 2;
        const int hatInst = 3;
        const int leadInst = 4;
        const int stabInst = song.instruments.size() > 7 ? 7 : 4;
        const int clapInst = song.instruments.size() > 9 ? 9 : 2;

        const int bassSequence[] = {36, 36, 38, 36, 34, 36, 41, 43};
        for (int row = 0; row < 64; row += 4) {
            PatternStep& kick = pattern.step(row, 1);
            kick.note = Note(36, row % 16 == 0 ? 1.0f : (row % 8 == 4 ? 0.9f : 0.84f));
            kick.instrument = kickInst;
            kick.gate = row % 8 == 4 ? 0.36 : 0.3;
            if (row % 16 == 12) {
                kick.retriggerCount = 2;
                kick.retriggerSpacingRows = 0.13;
                kick.retriggerVelocityDecay = 0.68;
            }
        }
        for (int row = 8; row < 64; row += 16) {
            PatternStep& snare = pattern.step(row, 1);
            snare.note = Note(40, 0.9f);
            snare.instrument = snareInst;
            snare.gate = 0.48;
            snare.retriggerCount = row % 32 == 24 ? 2 : 1;
            snare.retriggerSpacingRows = 0.16;
            snare.retriggerVelocityDecay = 0.62;
        }
        for (int row = 2; row < 64; row += 2) {
            PatternStep& hat = pattern.step(row, 1);
            hat.note = Note(74, row % 8 == 2 ? 0.56f : 0.34f);
            hat.instrument = hatInst;
            hat.gate = row % 8 == 6 ? 0.1 : 0.16;
            hat.probability = row % 8 == 6 ? 0.78 : 0.94;
            if (row % 16 == 14) {
                hat.retriggerCount = 3;
                hat.retriggerSpacingRows = 0.1;
                hat.retriggerVelocityDecay = 0.74;
            }
        }
        for (int row = 12; row < 64; row += 16) {
            PatternStep& clap = pattern.step(row, 1);
            clap.note = Note(70, 0.74f);
            clap.instrument = clapInst;
            clap.gate = 0.4;
            clap.retriggerCount = 2;
            clap.retriggerSpacingRows = 0.12;
            clap.retriggerVelocityDecay = 0.72;
        }
        for (int row = 0; row < 64; row += 2) {
            PatternStep& bass = pattern.step(row, 0);
            bass.note = Note(bassSequence[(row / 2) % 8], row % 8 == 0 ? 1.0f : 0.88f);
            bass.instrument = bassInst;
            bass.gate = row % 8 == 6 ? 0.4 : 0.6;
            if (row % 8 == 6) {
                bass.automation["cutoff"] = 0.34;
            } else if (row % 8 == 2) {
                bass.automation["drive"] = 0.56;
            }
        }
        for (int row = 0; row < 64; row += 8) {
            PatternStep& stab = pattern.step(row + 4, 2);
            stab.note = Note(row % 16 == 0 ? 63 : 67, 0.62f);
            stab.instrument = stabInst;
            stab.gate = 0.62;
            stab.automation["drive"] = 0.22;

            PatternStep& lead = pattern.step(row + 6, 2);
            lead.note = Note((row % 16 == 0) ? 70 : 72, 0.58f);
            lead.instrument = leadInst;
            lead.gate = 0.3;
        }
        for (int row = 0; row < 64; row += 16) {
            PatternStep& pad = pattern.step(row, 3);
            pad.note = Note((row == 32) ? 43 : 48, 0.42f);
            pad.instrument = 5;
            pad.gate = 3.2;
        }
        song.patterns[0] = pattern;
    }
    song.order = {0, 0, 0};
    return song;
}

Song buildNightDriveTemplate() {
    Song song = makeDemoSong();
    song.title = "Night Drive";
    song.author = "ArachnoTracker";
    song.description = "Melodic darkwave template with wide pads, arps, and evolving atmosphere.";
    song.notes = "Template: long pad motion and lead/arp call-response for cinematic arrangements.";
    song.bpm = 118.0;
    if (!song.patterns.empty()) {
        Pattern pattern("Night Intro", 64, static_cast<int>(song.tracks.size()));
        const int bassInst = 0;
        const int hatInst = 3;
        const int leadInst = 4;
        const int padInst = 5;
        const int arpInst = song.instruments.size() > 6 ? 6 : 4;
        const int droneInst = song.instruments.size() > 8 ? 8 : 5;

        const std::array<int, 8> bassLine {{36, 36, 39, 41, 36, 34, 31, 34}};
        for (int row = 0; row < 64; row += 8) {
            PatternStep& bass = pattern.step(row, 0);
            bass.note = Note(bassLine[static_cast<std::size_t>((row / 8) % bassLine.size())], 0.86f);
            bass.instrument = bassInst;
            bass.gate = 0.74;
        }

        for (int row = 0; row < 64; row += 4) {
            PatternStep& hat = pattern.step(row + 2, 1);
            hat.note = Note(76, row % 16 == 0 ? 0.38f : 0.26f);
            hat.instrument = hatInst;
            hat.gate = 0.2;
            if (row % 16 == 12) {
                hat.probability = 0.66;
            }
        }

        const std::array<int, 8> melody {{60, 63, 67, 70, 72, 70, 67, 63}};
        for (int index = 0; index < 16; ++index) {
            const int row = index * 4;
            PatternStep& arp = pattern.step(row, 2);
            arp.note = Note(melody[static_cast<std::size_t>(index % melody.size())] + (index >= 8 ? 12 : 0), 0.46f);
            arp.instrument = arpInst;
            arp.gate = 0.28;
            arp.microOffsetRows = (index % 2 == 0) ? 0.0 : -0.05;
        }
        for (int row = 0; row < 64; row += 16) {
            PatternStep& lead = pattern.step(row + 8, 2);
            lead.note = Note((row == 32) ? 75 : 72, 0.62f);
            lead.instrument = leadInst;
            lead.gate = 1.2;
            lead.automation["vibrato"] = 10.0;
        }
        for (int row = 0; row < 64; row += 16) {
            PatternStep& padRoot = pattern.step(row, 3);
            padRoot.note = Note((row == 16) ? 46 : (row == 32 ? 43 : 48), 0.48f);
            padRoot.instrument = padInst;
            padRoot.gate = 3.6;

            PatternStep& drone = pattern.step(row + 2, 3);
            drone.note = Note((row == 16) ? 53 : (row == 32 ? 50 : 55), 0.32f);
            drone.instrument = droneInst;
            drone.gate = 3.0;
            drone.automation["cutoff"] = row == 48 ? 0.62 : 0.48;
        }
        song.patterns[0] = pattern;
    }
    song.order = {0, 0, 0, 0};
    return song;
}

} // namespace

Song makeDemoSong() {
    Tracker tracker;
    tracker.song().title = "Arachno Demo";
    tracker.song().author = "ArachnoTracker";
    tracker.song().description = "Built-in darkwave/EBM demo with a broader patch library.";
    tracker.song().notes = "Use --show, --arrangement, --instruments, --list-demo-templates, and --write-demo <path> <template>.";
    tracker.song().bpm = 132.0;
    tracker.song().rowsPerBeat = 4;
    tracker.song().sampleRate = 48000;

    const int bassTrack = tracker.addTrack("Bass");
    const int drumTrack = tracker.addTrack("Drums");
    const int leadTrack = tracker.addTrack("Lead");
    const int padTrack = tracker.addTrack("Pad");

    tracker.song().tracks[static_cast<std::size_t>(bassTrack)].volume = 1.0;
    tracker.song().tracks[static_cast<std::size_t>(drumTrack)].volume = 1.0;
    tracker.song().tracks[static_cast<std::size_t>(leadTrack)].volume = 0.75;
    tracker.song().tracks[static_cast<std::size_t>(leadTrack)].pan = -0.18;
    tracker.song().tracks[static_cast<std::size_t>(padTrack)].volume = 0.55;
    tracker.song().tracks[static_cast<std::size_t>(padTrack)].pan = 0.2;

    SynthPatch bass;
    bass.name = "EBM Monolith";
    bass.oscillatorA = Waveform::SuperSaw;
    bass.oscillatorB = Waveform::Square;
    bass.oscillatorC = Waveform::Triangle;
    bass.oscillatorCEnabled = true;
    bass.oscillatorCMix = 0.14;
    bass.oscillatorMix = 0.36;
    bass.pulseWidth = 0.42;
    bass.pwmDepth = 0.06;
    bass.unisonVoices = 2;
    bass.unisonDetuneCents = 3.5;
    bass.stereoSpread = 0.08;
    bass.subOscillator = 0.66;
    bass.cutoff = 0.28;
    bass.resonance = 0.2;
    bass.filterMode = 1;
    bass.filterDrive = 0.72;
    bass.filterKeytrack = 0.42;
    bass.filterEnvelopeAmount = 0.42;
    bass.lfoFilterDepth = 0.05;
    bass.hardSyncEnabled = true;
    bass.hardSync = 0.08;
    bass.drive = 0.36;
    bass.wavefold = 0.06;
    bass.analogColor = 0.74;
    bass.toneTilt = -0.5;
    bass.bitCrushEnabled = true;
    bass.bitCrush = 0.02;
    bass.combMix = 0.06;
    bass.combTime = 0.052;
    bass.combFeedback = 0.18;
    bass.gain = 0.62;
    bass.ampEnvelope.attack = 0.002;
    bass.ampEnvelope.decay = 0.09;
    bass.ampEnvelope.sustain = 0.58;
    bass.ampEnvelope.release = 0.1;
    bass.filterEnvelope.attack = 0.001;
    bass.filterEnvelope.decay = 0.12;
    bass.filterEnvelope.sustain = 0.28;
    bass.filterEnvelope.release = 0.1;

    SynthPatch lead;
    lead.name = "Bright Twin";
    lead.oscillatorA = Waveform::Saw;
    lead.oscillatorB = Waveform::Triangle;
    lead.oscillatorC = Waveform::Square;
    lead.oscillatorCEnabled = true;
    lead.oscillatorCMix = 0.22;
    lead.oscillatorMix = 0.42;
    lead.detuneCents = 11.0;
    lead.detuneCCents = -15.0;
    lead.pulseWidth = 0.46;
    lead.pwmDepth = 0.12;
    lead.fmEnabled = true;
    lead.fmAmount = 0.08;
    lead.fmRatio = 2.0;
    lead.fmFeedback = 0.12;
    lead.chorusEnabled = true;
    lead.chorusMix = 0.18;
    lead.chorusRate = 0.42;
    lead.chorusDepth = 0.35;
    lead.unisonVoices = 4;
    lead.unisonDetuneCents = 10.5;
    lead.stereoSpread = 0.38;
    lead.cutoff = 0.82;
    lead.resonance = 0.24;
    lead.filterEnvelopeAmount = 0.22;
    lead.lfoFilterDepth = 0.08;
    lead.lfoRate = 5.8;
    lead.vibratoCents = 5.5;
    lead.tremoloDepth = 0.08;
    lead.ringEnabled = true;
    lead.ringMod = 0.08;
    lead.hardSyncEnabled = true;
    lead.hardSync = 0.12;
    lead.drive = 0.1;
    lead.wavefold = 0.05;
    lead.gain = 0.42;
    lead.ampEnvelope.attack = 0.004;
    lead.ampEnvelope.decay = 0.11;
    lead.ampEnvelope.sustain = 0.72;
    lead.ampEnvelope.release = 0.18;
    lead.filterEnvelope.attack = 0.002;
    lead.filterEnvelope.decay = 0.08;
    lead.filterEnvelope.sustain = 0.5;
    lead.filterEnvelope.release = 0.12;

    SynthPatch pad;
    pad.name = "Soft Wide";
    pad.oscillatorA = Waveform::Sine;
    pad.oscillatorB = Waveform::Triangle;
    pad.oscillatorC = Waveform::Saw;
    pad.oscillatorCEnabled = true;
    pad.oscillatorCMix = 0.2;
    pad.oscillatorMix = 0.5;
    pad.detuneCents = -8.0;
    pad.detuneCCents = 9.0;
    pad.unisonVoices = 5;
    pad.unisonDetuneCents = 14.0;
    pad.stereoSpread = 0.62;
    pad.chorusEnabled = true;
    pad.chorusMix = 0.32;
    pad.chorusRate = 0.24;
    pad.chorusDepth = 0.58;
    pad.cutoff = 0.5;
    pad.resonance = 0.14;
    pad.filterEnvelopeAmount = 0.18;
    pad.lfoFilterDepth = 0.12;
    pad.lfoPanDepth = 0.08;
    pad.lfoRate = 0.45;
    pad.vibratoCents = 2.5;
    pad.tremoloDepth = 0.1;
    pad.combMix = 0.1;
    pad.combTime = 0.22;
    pad.combFeedback = 0.22;
    pad.gain = 0.38;
    pad.ampEnvelope.attack = 0.08;
    pad.ampEnvelope.decay = 0.34;
    pad.ampEnvelope.sustain = 0.8;
    pad.ampEnvelope.release = 0.58;
    pad.filterEnvelope.attack = 0.06;
    pad.filterEnvelope.decay = 0.24;
    pad.filterEnvelope.sustain = 0.52;
    pad.filterEnvelope.release = 0.44;

    SynthPatch kick;
    kick.name = "EBM Iron Kick";
    kick.oscillatorA = Waveform::Sine;
    kick.oscillatorB = Waveform::Triangle;
    kick.oscillatorMix = 0.14;
    kick.pitchEnvelopeSemitones = 40.0;
    kick.pitchEnvelopeDecay = 0.05;
    kick.subOscillator = 0.34;
    kick.noise = 0.03;
    kick.noiseTone = 0.36;
    kick.click = 0.52;
    kick.transientShape = 0.62;
    kick.transientNoise = 0.12;
    kick.transientPitchSemitones = 14.0;
    kick.transientPitchDecay = 0.01;
    kick.transientBurstCount = 2;
    kick.transientBurstSpacing = 0.0028;
    kick.transientBurstDecay = 0.72;
    kick.transientTone = 0.34;
    kick.transientDecay = 0.014;
    kick.cutoff = 0.56;
    kick.filterMode = 2;
    kick.filterDrive = 0.72;
    kick.filterKeytrack = 0.18;
    kick.filterEnvelopeAmount = 0.22;
    kick.fmEnabled = true;
    kick.fmAmount = 0.11;
    kick.fmRatio = 1.5;
    kick.fmFeedback = 0.26;
    kick.fmAlgorithm = 1;
    kick.drive = 0.52;
    kick.wavefold = 0.08;
    kick.analogColor = 0.62;
    kick.toneTilt = -0.36;
    kick.gain = 0.98;
    kick.ampEnvelope.attack = 0.001;
    kick.ampEnvelope.decay = 0.12;
    kick.ampEnvelope.sustain = 0.0;
    kick.ampEnvelope.release = 0.035;

    SynthPatch snare;
    snare.name = "Steel Snare";
    snare.oscillatorA = Waveform::Noise;
    snare.oscillatorB = Waveform::Square;
    snare.oscillatorMix = 0.24;
    snare.fmAmount = 0.22;
    snare.fmRatio = 3.6;
    snare.fmEnabled = true;
    snare.fmFeedback = 0.3;
    snare.fmAlgorithm = 3;
    snare.noise = 0.82;
    snare.noiseTone = 0.84;
    snare.click = 0.3;
    snare.transientShape = 0.72;
    snare.transientNoise = 0.66;
    snare.transientPitchSemitones = 22.0;
    snare.transientPitchDecay = 0.009;
    snare.transientBurstCount = 3;
    snare.transientBurstSpacing = 0.0027;
    snare.transientBurstDecay = 0.56;
    snare.transientTone = 0.86;
    snare.transientDecay = 0.03;
    snare.pitchEnvelopeSemitones = 9.0;
    snare.pitchEnvelopeDecay = 0.035;
    snare.cutoff = 0.8;
    snare.filterMode = 2;
    snare.filterDrive = 0.66;
    snare.filterKeytrack = 0.28;
    snare.highPass = 0.42;
    snare.ringEnabled = true;
    snare.ringMod = 0.22;
    snare.drive = 0.44;
    snare.wavefold = 0.2;
    snare.analogColor = 0.58;
    snare.toneTilt = 0.22;
    snare.bitCrushEnabled = true;
    snare.bitCrush = 0.08;
    snare.gain = 0.64;
    snare.ampEnvelope.attack = 0.001;
    snare.ampEnvelope.decay = 0.082;
    snare.ampEnvelope.sustain = 0.0;
    snare.ampEnvelope.release = 0.16;

    SynthPatch hat;
    hat.name = "EBM Razor Hat";
    hat.oscillatorA = Waveform::Noise;
    hat.oscillatorB = Waveform::Square;
    hat.oscillatorMix = 0.65;
    hat.pulseWidth = 0.28;
    hat.fmEnabled = true;
    hat.fmAmount = 0.28;
    hat.fmRatio = 6.0;
    hat.fmFeedback = 0.25;
    hat.fmAlgorithm = 3;
    hat.chorusEnabled = true;
    hat.chorusMix = 0.08;
    hat.chorusRate = 0.9;
    hat.chorusDepth = 0.18;
    hat.noise = 0.92;
    hat.noiseTone = 0.95;
    hat.click = 0.18;
    hat.transientShape = 0.62;
    hat.transientNoise = 0.72;
    hat.transientPitchSemitones = 24.0;
    hat.transientPitchDecay = 0.007;
    hat.transientBurstCount = 4;
    hat.transientBurstSpacing = 0.0025;
    hat.transientBurstDecay = 0.5;
    hat.transientTone = 0.98;
    hat.transientDecay = 0.008;
    hat.cutoff = 0.9;
    hat.filterMode = 2;
    hat.filterDrive = 0.48;
    hat.filterKeytrack = 0.2;
    hat.highPass = 0.78;
    hat.ringEnabled = true;
    hat.ringMod = 0.35;
    hat.hardSyncEnabled = true;
    hat.hardSync = 0.44;
    hat.bitCrushEnabled = true;
    hat.bitCrush = 0.22;
    hat.sampleRateReduction = 0.18;
    hat.analogColor = 0.52;
    hat.toneTilt = 0.44;
    hat.gain = 0.32;
    hat.ampEnvelope.attack = 0.001;
    hat.ampEnvelope.decay = 0.035;
    hat.ampEnvelope.sustain = 0.0;
    hat.ampEnvelope.release = 0.025;

    SynthPatch arp = lead;
    arp.name = "Neon Arp";
    arp.oscillatorA = Waveform::Square;
    arp.oscillatorB = Waveform::Saw;
    arp.oscillatorC = Waveform::Triangle;
    arp.oscillatorCEnabled = true;
    arp.oscillatorCMix = 0.26;
    arp.oscillatorD = Waveform::Sine;
    arp.oscillatorDEnabled = true;
    arp.oscillatorDMix = 0.18;
    arp.detuneDCents = 17.0;
    arp.fmEnabled = true;
    arp.fmAmount = 0.16;
    arp.fmRatio = 3.0;
    arp.fmFeedback = 0.14;
    arp.ringEnabled = true;
    arp.ringMod = 0.16;
    arp.hardSyncEnabled = true;
    arp.hardSync = 0.2;
    arp.gain = 0.32;
    arp.ampEnvelope.attack = 0.002;
    arp.ampEnvelope.decay = 0.08;
    arp.ampEnvelope.sustain = 0.45;
    arp.ampEnvelope.release = 0.09;

    SynthPatch stab = bass;
    stab.name = "Body Stab";
    stab.oscillatorA = Waveform::Saw;
    stab.oscillatorB = Waveform::Square;
    stab.oscillatorC = Waveform::Triangle;
    stab.oscillatorCEnabled = true;
    stab.oscillatorCMix = 0.33;
    stab.oscillatorD = Waveform::Noise;
    stab.oscillatorDEnabled = true;
    stab.oscillatorDMix = 0.16;
    stab.detuneDCents = 21.0;
    stab.hardSyncEnabled = true;
    stab.hardSync = 0.26;
    stab.ringEnabled = true;
    stab.ringMod = 0.2;
    stab.cutoff = 0.48;
    stab.drive = 0.36;
    stab.wavefold = 0.12;
    stab.gain = 0.43;
    stab.ampEnvelope.attack = 0.002;
    stab.ampEnvelope.decay = 0.16;
    stab.ampEnvelope.sustain = 0.0;
    stab.ampEnvelope.release = 0.12;

    SynthPatch drone = pad;
    drone.name = "Rust Drone";
    drone.oscillatorA = Waveform::Triangle;
    drone.oscillatorB = Waveform::Saw;
    drone.oscillatorC = Waveform::Noise;
    drone.oscillatorCEnabled = true;
    drone.oscillatorCMix = 0.2;
    drone.oscillatorD = Waveform::Square;
    drone.oscillatorDEnabled = true;
    drone.oscillatorDMix = 0.12;
    drone.unisonVoices = 6;
    drone.unisonDetuneCents = 18.0;
    drone.chorusEnabled = true;
    drone.chorusMix = 0.38;
    drone.chorusRate = 0.16;
    drone.chorusDepth = 0.62;
    drone.cutoff = 0.42;
    drone.highPass = 0.12;
    drone.ringEnabled = true;
    drone.ringMod = 0.12;
    drone.drive = 0.18;
    drone.combMix = 0.14;
    drone.combTime = 0.26;
    drone.combFeedback = 0.38;
    drone.gain = 0.26;
    drone.ampEnvelope.attack = 0.22;
    drone.ampEnvelope.decay = 0.4;
    drone.ampEnvelope.sustain = 0.74;
    drone.ampEnvelope.release = 0.65;

    SynthPatch clap = snare;
    clap.name = "Factory Slap Clap";
    clap.oscillatorA = Waveform::Noise;
    clap.oscillatorB = Waveform::Square;
    clap.oscillatorC = Waveform::Noise;
    clap.oscillatorCEnabled = true;
    clap.oscillatorCMix = 0.48;
    clap.cutoff = 0.88;
    clap.highPass = 0.6;
    clap.click = 0.38;
    clap.transientShape = 0.74;
    clap.transientNoise = 0.7;
    clap.transientPitchSemitones = 14.0;
    clap.transientPitchDecay = 0.01;
    clap.transientBurstCount = 5;
    clap.transientBurstSpacing = 0.0042;
    clap.transientBurstDecay = 0.64;
    clap.transientTone = 0.9;
    clap.transientDecay = 0.015;
    clap.filterMode = 2;
    clap.filterDrive = 0.54;
    clap.fmAlgorithm = 3;
    clap.bitCrushEnabled = true;
    clap.bitCrush = 0.06;
    clap.analogColor = 0.44;
    clap.toneTilt = 0.3;
    clap.gain = 0.46;
    clap.ampEnvelope.decay = 0.055;
    clap.ampEnvelope.release = 0.08;

    SynthPatch tom = kick;
    tom.name = "Tunnel Assault Tom";
    tom.pitchEnvelopeSemitones = 12.0;
    tom.pitchEnvelopeDecay = 0.045;
    tom.cutoff = 0.52;
    tom.noiseTone = 0.48;
    tom.transientShape = 0.42;
    tom.transientPitchSemitones = 9.0;
    tom.transientPitchDecay = 0.012;
    tom.transientBurstCount = 3;
    tom.transientBurstSpacing = 0.0034;
    tom.transientBurstDecay = 0.66;
    tom.transientTone = 0.58;
    tom.filterMode = 1;
    tom.filterDrive = 0.58;
    tom.filterKeytrack = 0.35;
    tom.drive = 0.42;
    tom.wavefold = 0.06;
    tom.analogColor = 0.56;
    tom.toneTilt = -0.18;
    tom.gain = 0.58;
    tom.ampEnvelope.decay = 0.12;
    tom.ampEnvelope.release = 0.1;

    SynthPatch ride = hat;
    ride.name = "Noise Ride";
    ride.oscillatorC = Waveform::Noise;
    ride.oscillatorCEnabled = true;
    ride.oscillatorCMix = 0.58;
    ride.highPass = 0.86;
    ride.noiseTone = 0.98;
    ride.chorusMix = 0.04;
    ride.ringMod = 0.42;
    ride.transientShape = 0.48;
    ride.transientPitchSemitones = 8.0;
    ride.transientPitchDecay = 0.007;
    ride.transientBurstCount = 3;
    ride.transientBurstSpacing = 0.0032;
    ride.transientBurstDecay = 0.62;
    ride.transientTone = 0.99;
    ride.sampleRateReduction = 0.22;
    ride.gain = 0.2;
    ride.ampEnvelope.decay = 0.07;
    ride.ampEnvelope.release = 0.06;

    SynthPatch acid = bass;
    acid.name = "Acid Razor";
    acid.oscillatorA = Waveform::Saw;
    acid.oscillatorB = Waveform::Square;
    acid.oscillatorC = Waveform::Saw;
    acid.oscillatorCEnabled = true;
    acid.oscillatorCMix = 0.16;
    acid.oscillatorDEnabled = false;
    acid.oscillatorMix = 0.62;
    acid.detuneCents = 4.0;
    acid.detuneCCents = -10.0;
    acid.fmEnabled = false;
    acid.chorusEnabled = false;
    acid.subOscillator = 0.25;
    acid.cutoff = 0.28;
    acid.resonance = 0.42;
    acid.filterEnvelopeAmount = 0.62;
    acid.lfoFilterDepth = 0.14;
    acid.hardSyncEnabled = true;
    acid.hardSync = 0.26;
    acid.drive = 0.44;
    acid.wavefold = 0.22;
    acid.combMix = 0.18;
    acid.combTime = 0.03;
    acid.combFeedback = 0.35;
    acid.gain = 0.34;
    acid.ampEnvelope.attack = 0.001;
    acid.ampEnvelope.decay = 0.08;
    acid.ampEnvelope.sustain = 0.34;
    acid.ampEnvelope.release = 0.07;
    acid.filterEnvelope.attack = 0.001;
    acid.filterEnvelope.decay = 0.09;
    acid.filterEnvelope.sustain = 0.2;
    acid.filterEnvelope.release = 0.08;

    SynthPatch bell = lead;
    bell.name = "FM Glass Bell";
    bell.oscillatorA = Waveform::Sine;
    bell.oscillatorB = Waveform::Triangle;
    bell.oscillatorC = Waveform::Sine;
    bell.oscillatorCEnabled = true;
    bell.oscillatorD = Waveform::Noise;
    bell.oscillatorDEnabled = true;
    bell.oscillatorMix = 0.22;
    bell.oscillatorCMix = 0.4;
    bell.oscillatorDMix = 0.08;
    bell.fmEnabled = true;
    bell.fmAmount = 0.44;
    bell.fmRatio = 5.0;
    bell.fmFeedback = 0.28;
    bell.chorusEnabled = true;
    bell.chorusMix = 0.18;
    bell.chorusRate = 0.18;
    bell.chorusDepth = 0.42;
    bell.unisonVoices = 2;
    bell.unisonDetuneCents = 3.0;
    bell.cutoff = 0.84;
    bell.resonance = 0.36;
    bell.filterEnvelopeAmount = 0.08;
    bell.ringEnabled = true;
    bell.ringMod = 0.2;
    bell.drive = 0.12;
    bell.wavefold = 0.18;
    bell.combMix = 0.22;
    bell.combTime = 0.12;
    bell.combFeedback = 0.28;
    bell.noise = 0.06;
    bell.gain = 0.31;
    bell.ampEnvelope.attack = 0.001;
    bell.ampEnvelope.decay = 0.38;
    bell.ampEnvelope.sustain = 0.0;
    bell.ampEnvelope.release = 0.36;
    bell.filterEnvelope.attack = 0.001;
    bell.filterEnvelope.decay = 0.22;
    bell.filterEnvelope.sustain = 0.24;
    bell.filterEnvelope.release = 0.24;

    SynthPatch choir = pad;
    choir.name = "Cathedral Choir";
    choir.oscillatorA = Waveform::Triangle;
    choir.oscillatorB = Waveform::Saw;
    choir.oscillatorC = Waveform::Sine;
    choir.oscillatorCEnabled = true;
    choir.oscillatorD = Waveform::Noise;
    choir.oscillatorDEnabled = true;
    choir.oscillatorMix = 0.58;
    choir.oscillatorCMix = 0.28;
    choir.oscillatorDMix = 0.08;
    choir.unisonVoices = 7;
    choir.unisonDetuneCents = 16.0;
    choir.stereoSpread = 0.72;
    choir.fmEnabled = true;
    choir.fmAmount = 0.06;
    choir.fmRatio = 1.5;
    choir.chorusEnabled = true;
    choir.chorusMix = 0.46;
    choir.chorusRate = 0.11;
    choir.chorusDepth = 0.74;
    choir.cutoff = 0.48;
    choir.resonance = 0.22;
    choir.filterEnvelopeAmount = 0.26;
    choir.lfoFilterDepth = 0.16;
    choir.lfoPanDepth = 0.22;
    choir.vibratoCents = 3.0;
    choir.tremoloDepth = 0.14;
    choir.ringEnabled = true;
    choir.ringMod = 0.08;
    choir.combMix = 0.18;
    choir.combTime = 0.34;
    choir.combFeedback = 0.4;
    choir.drive = 0.12;
    choir.highPass = 0.04;
    choir.noise = 0.08;
    choir.gain = 0.24;
    choir.ampEnvelope.attack = 0.16;
    choir.ampEnvelope.decay = 0.42;
    choir.ampEnvelope.sustain = 0.82;
    choir.ampEnvelope.release = 0.92;

    SynthPatch reese = bass;
    reese.name = "Reese Pressure XL";
    reese.oscillatorA = Waveform::Saw;
    reese.oscillatorB = Waveform::Saw;
    reese.oscillatorC = Waveform::Square;
    reese.oscillatorCEnabled = true;
    reese.oscillatorD = Waveform::Noise;
    reese.oscillatorDEnabled = true;
    reese.oscillatorMix = 0.5;
    reese.oscillatorCMix = 0.22;
    reese.oscillatorDMix = 0.1;
    reese.unisonVoices = 5;
    reese.unisonDetuneCents = 16.0;
    reese.stereoSpread = 0.36;
    reese.detuneCents = 18.0;
    reese.detuneCCents = -22.0;
    reese.detuneDCents = 9.0;
    reese.subOscillator = 0.48;
    reese.cutoff = 0.24;
    reese.resonance = 0.28;
    reese.filterMode = 1;
    reese.filterDrive = 0.64;
    reese.filterKeytrack = 0.38;
    reese.filterEnvelopeAmount = 0.18;
    reese.lfoFilterDepth = 0.18;
    reese.hardSyncEnabled = true;
    reese.hardSync = 0.1;
    reese.drive = 0.56;
    reese.wavefold = 0.14;
    reese.analogColor = 0.7;
    reese.toneTilt = -0.34;
    reese.bitCrushEnabled = true;
    reese.bitCrush = 0.06;
    reese.combMix = 0.22;
    reese.combTime = 0.046;
    reese.combFeedback = 0.32;
    reese.highPass = 0.06;
    reese.gain = 0.48;
    reese.ampEnvelope.attack = 0.003;
    reese.ampEnvelope.decay = 0.16;
    reese.ampEnvelope.sustain = 0.64;
    reese.ampEnvelope.release = 0.22;

    SynthPatch rim = snare;
    rim.name = "Anvil Rim";
    rim.oscillatorA = Waveform::Square;
    rim.oscillatorB = Waveform::Noise;
    rim.oscillatorC = Waveform::Triangle;
    rim.oscillatorCEnabled = true;
    rim.oscillatorCMix = 0.24;
    rim.oscillatorDEnabled = false;
    rim.oscillatorMix = 0.18;
    rim.fmEnabled = true;
    rim.fmAmount = 0.28;
    rim.fmRatio = 8.0;
    rim.fmFeedback = 0.22;
    rim.noise = 0.46;
    rim.noiseTone = 0.9;
    rim.cutoff = 0.82;
    rim.highPass = 0.62;
    rim.ringEnabled = true;
    rim.ringMod = 0.34;
    rim.hardSyncEnabled = true;
    rim.hardSync = 0.22;
    rim.drive = 0.4;
    rim.wavefold = 0.22;
    rim.bitCrushEnabled = true;
    rim.bitCrush = 0.18;
    rim.sampleRateReduction = 0.12;
    rim.click = 0.48;
    rim.transientShape = 0.84;
    rim.transientNoise = 0.44;
    rim.transientPitchSemitones = 22.0;
    rim.transientPitchDecay = 0.008;
    rim.transientBurstCount = 2;
    rim.transientBurstSpacing = 0.0026;
    rim.transientBurstDecay = 0.52;
    rim.transientTone = 0.95;
    rim.transientDecay = 0.012;
    rim.gain = 0.28;
    rim.ampEnvelope.attack = 0.001;
    rim.ampEnvelope.decay = 0.05;
    rim.ampEnvelope.sustain = 0.0;
    rim.ampEnvelope.release = 0.08;

    const int bassInstrument = tracker.addInstrument(bass);
    const int kickInstrument = tracker.addInstrument(kick);
    const int snareInstrument = tracker.addInstrument(snare);
    const int hatInstrument = tracker.addInstrument(hat);
    const int leadInstrument = tracker.addInstrument(lead);
    const int padInstrument = tracker.addInstrument(pad);
    const int arpInstrument = tracker.addInstrument(arp);
    tracker.addInstrument(stab);
    tracker.addInstrument(drone);
    const int clapInstrument = tracker.addInstrument(clap);
    const int tomInstrument = tracker.addInstrument(tom);
    const int rideInstrument = tracker.addInstrument(ride);
    const int acidInstrument = tracker.addInstrument(acid);
    const int bellInstrument = tracker.addInstrument(bell);
    const int choirInstrument = tracker.addInstrument(choir);
    const int reeseInstrument = tracker.addInstrument(reese);
    const int rimInstrument = tracker.addInstrument(rim);

    Pattern pattern = buildOpeningPattern(
        static_cast<int>(tracker.song().tracks.size()),
        bassTrack,
        drumTrack,
        leadTrack,
        padTrack,
        bassInstrument,
        kickInstrument,
        snareInstrument,
        hatInstrument,
        leadInstrument,
        padInstrument,
        arpInstrument,
        clapInstrument,
        tomInstrument,
        rideInstrument,
        acidInstrument,
        bellInstrument,
        choirInstrument,
        reeseInstrument,
        rimInstrument);

    const int patternIndex = tracker.addPattern(pattern);
    tracker.appendPatternToOrder(patternIndex);
    tracker.appendPatternToOrder(patternIndex);
    return tracker.song();
}

Song makeBlankSong() {
    Tracker tracker;
    tracker.song().title = "Untitled";
    tracker.song().author.clear();
    tracker.song().description = "Blank project";
    tracker.song().notes.clear();
    tracker.song().bpm = 128.0;
    tracker.song().rowsPerBeat = 4;
    tracker.song().sampleRate = 48000;

    const int bassTrack = tracker.addTrack("Bass");
    const int drumTrack = tracker.addTrack("Drums");
    const int leadTrack = tracker.addTrack("Lead");
    const int padTrack = tracker.addTrack("Pad");
    (void)bassTrack;
    (void)drumTrack;
    (void)leadTrack;
    (void)padTrack;

    SynthPatch initPatch;
    initPatch.name = "Init";
    initPatch.gain = 0.72;
    initPatch.cutoff = 0.58;
    initPatch.resonance = 0.12;
    initPatch.drive = 0.08;
    initPatch.ampEnvelope.attack = 0.002;
    initPatch.ampEnvelope.decay = 0.09;
    initPatch.ampEnvelope.sustain = 0.76;
    initPatch.ampEnvelope.release = 0.14;
    tracker.addInstrument(initPatch);

    Pattern pattern("Pattern1", 64, static_cast<int>(tracker.song().tracks.size()));
    const int patternIndex = tracker.addPattern(pattern);
    tracker.appendPatternToOrder(patternIndex);

    return tracker.song();
}

Song makeTemplateSong(const std::string& templateName) {
    const std::string normalized = lowerCopy(templateName);
    if (normalized.empty() || normalized == "default" || normalized == "demo" || normalized == "darkwave_foundation") {
        return makeDemoSong();
    }
    if (normalized == "factory_pulse" || normalized == "ebm" || normalized == "ebm_factory") {
        return buildEbmTemplate();
    }
    if (normalized == "night_drive" || normalized == "cinematic_darkwave") {
        return buildNightDriveTemplate();
    }
    throw std::invalid_argument("unknown template: " + templateName);
}

std::vector<std::string> demoTemplateNames() {
    return {
        "darkwave_foundation",
        "factory_pulse",
        "night_drive",
    };
}

const char* waveformName(Waveform waveform) {
    switch (waveform) {
        case Waveform::Sine: return "sine";
        case Waveform::Square: return "square";
        case Waveform::Saw: return "saw";
        case Waveform::Triangle: return "triangle";
        case Waveform::Noise: return "noise";
        case Waveform::SuperSaw: return "supersaw";
    }
    return "sine";
}

Waveform waveformFromName(const std::string& name) {
    const std::string normalized = lowerCopy(name);
    if (normalized == "square" || normalized == "sqr") return Waveform::Square;
    if (normalized == "saw") return Waveform::Saw;
    if (normalized == "triangle" || normalized == "tri") return Waveform::Triangle;
    if (normalized == "noise" || normalized == "noi") return Waveform::Noise;
    if (normalized == "supersaw" || normalized == "super_saw" || normalized == "ssaw" || normalized == "sup") return Waveform::SuperSaw;
    return Waveform::Sine;
}

} // namespace arachno
