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

void applyVintageHiFiPolish(SynthPatch& patch, double intensity, bool percussive) {
    const double t = std::clamp(intensity, 0.0, 1.0);
    patch.analogColor = std::clamp(std::max(patch.analogColor, 0.44 + t * 0.34), 0.0, 1.0);
    patch.analogWarmth = std::clamp(std::max(patch.analogWarmth, 0.38 + t * 0.44), 0.0, 1.0);
    patch.voiceSlop = std::clamp(std::max(patch.voiceSlop, 0.20 + t * 0.36), 0.0, 1.0);
    patch.phaseScatter = std::clamp(std::max(patch.phaseScatter, 0.22 + t * 0.44), 0.0, 1.0);
    patch.unisonWarp = std::clamp(std::max(patch.unisonWarp, 0.10 + t * 0.52), 0.0, 1.0);
    patch.unisonHumanize = std::clamp(std::max(patch.unisonHumanize, 0.16 + t * 0.58), 0.0, 1.0);
    patch.fmColor = std::clamp(std::max(patch.fmColor, 0.34 + t * 0.46), 0.0, 1.0);
    patch.fmSpread = std::clamp(std::max(patch.fmSpread, 0.08 + t * 0.42), 0.0, 1.0);
    patch.chorusTone = std::clamp(std::max(patch.chorusTone, 0.46 + t * 0.34), 0.0, 1.0);
    patch.chorusJitter = std::clamp(std::max(patch.chorusJitter, 0.14 + t * 0.52), 0.0, 1.0);
    patch.chorusSaturation = std::clamp(std::max(patch.chorusSaturation, 0.12 + t * 0.56), 0.0, 1.0);
    patch.delayDiffusion = std::clamp(std::max(patch.delayDiffusion, 0.18 + t * 0.48), 0.0, 1.0);
    patch.delayWow = std::clamp(std::max(patch.delayWow, 0.12 + t * 0.56), 0.0, 1.0);
    patch.delayCrossfeed = std::clamp(std::max(patch.delayCrossfeed, 0.20 + t * 0.52), 0.0, 1.0);
    patch.reverbDecay = std::clamp(std::max(patch.reverbDecay, 0.40 + t * 0.52), 0.0, 1.0);
    patch.reverbEarlyMix = std::clamp(std::max(patch.reverbEarlyMix, 0.16 + t * 0.36), 0.0, 1.0);
    patch.reverbTone = std::clamp(std::max(patch.reverbTone, 0.30 + t * 0.52), 0.0, 1.0);
    patch.reverbChorus = std::clamp(std::max(patch.reverbChorus, 0.08 + t * 0.46), 0.0, 1.0);
    patch.reverbBloom = std::clamp(std::max(patch.reverbBloom, 0.12 + t * 0.54), 0.0, 1.0);
    patch.consoleCrosstalk = std::clamp(std::max(patch.consoleCrosstalk, 0.04 + t * 0.16), 0.0, 1.0);
    patch.stereoDepth = std::clamp(std::max(patch.stereoDepth, 0.12 + t * 0.56), 0.0, 1.0);
    patch.hifiExciter = std::clamp(std::max(patch.hifiExciter, 0.12 + t * 0.58), 0.0, 1.0);
    patch.outputTransformer = std::clamp(std::max(patch.outputTransformer, 0.12 + t * 0.52), 0.0, 1.0);
    patch.outputSoftClip = std::clamp(std::max(patch.outputSoftClip, 0.14 + t * 0.56), 0.0, 1.0);
    patch.outputGlue = std::clamp(std::max(patch.outputGlue, 0.22 + t * 0.52), 0.0, 1.0);
    patch.tapeColor = std::clamp(std::max(patch.tapeColor, 0.10 + t * 0.44), 0.0, 1.0);
    patch.airBoost = std::clamp(std::max(patch.airBoost, 0.10 + t * 0.44), 0.0, 1.0);
    patch.lowPunch = std::clamp(std::max(patch.lowPunch, 0.12 + t * 0.42), 0.0, 1.0);
    patch.wowFlutter = std::clamp(std::max(patch.wowFlutter, 0.05 + t * 0.36), 0.0, 1.0);
    patch.vintageDrift = std::clamp(std::max(patch.vintageDrift, 0.24 + t * 0.52), 0.0, 1.0);
    patch.chorusEnsemble = std::clamp(std::max(patch.chorusEnsemble, 0.14 + t * 0.58), 0.0, 1.0);
    patch.chorusFeedback = std::clamp(std::max(patch.chorusFeedback, 0.08 + t * 0.52), 0.0, 0.98);
    patch.chorusDelay = std::clamp(std::max(patch.chorusDelay, 0.14 + t * 0.44), 0.0, 1.0);
    patch.chorusWidth = std::clamp(std::max(patch.chorusWidth, 0.32 + t * 0.52), 0.0, 1.0);
    patch.delayDrive = std::clamp(std::max(patch.delayDrive, 0.08 + t * 0.52), 0.0, 1.0);
    patch.delayModDepth = std::clamp(std::max(patch.delayModDepth, 0.12 + t * 0.54), 0.0, 1.0);
    patch.delayStereo = std::clamp(std::max(patch.delayStereo, 0.16 + t * 0.56), 0.0, 1.0);
    patch.delayDucking = std::clamp(std::max(patch.delayDucking, 0.08 + t * 0.42), 0.0, 1.0);
    patch.reverbDiffusion = std::clamp(std::max(patch.reverbDiffusion, 0.36 + t * 0.52), 0.0, 1.0);
    patch.reverbWidth = std::clamp(std::max(patch.reverbWidth, 0.18 + t * 0.64), 0.0, 1.0);
    patch.reverbModDepth = std::clamp(std::max(patch.reverbModDepth, 0.08 + t * 0.46), 0.0, 1.0);
    patch.reverbShimmer = std::clamp(std::max(patch.reverbShimmer, 0.02 + t * 0.36), 0.0, 1.0);

    if (percussive) {
        patch.delayMix = std::clamp(std::max(patch.delayMix, 0.02 + t * 0.18), 0.0, 1.0);
        patch.reverbMix = std::clamp(std::max(patch.reverbMix, 0.05 + t * 0.26), 0.0, 1.0);
        patch.combMix = std::clamp(std::max(patch.combMix, 0.04 + t * 0.18), 0.0, 1.0);
        patch.transientShape = std::clamp(std::max(patch.transientShape, 0.44 + t * 0.46), 0.0, 1.0);
    } else {
        patch.delayMix = std::clamp(std::max(patch.delayMix, 0.08 + t * 0.34), 0.0, 1.0);
        patch.reverbMix = std::clamp(std::max(patch.reverbMix, 0.10 + t * 0.38), 0.0, 1.0);
        patch.combMix = std::clamp(std::max(patch.combMix, 0.04 + t * 0.24), 0.0, 1.0);
        patch.chorusEnabled = true;
        patch.chorusMix = std::clamp(std::max(patch.chorusMix, 0.10 + t * 0.36), 0.0, 1.0);
    }
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
    bass.analogWarmth = 0.66;
    bass.voiceSlop = 0.52;
    bass.phaseScatter = 0.41;
    bass.chorusTone = 0.44;
    bass.delayDiffusion = 0.26;
    bass.reverbDecay = 0.58;
    bass.reverbEarlyMix = 0.22;
    bass.consoleCrosstalk = 0.09;
    bass.outputGlue = 0.48;
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
    lead.analogWarmth = 0.53;
    lead.voiceSlop = 0.31;
    lead.phaseScatter = 0.46;
    lead.chorusTone = 0.74;
    lead.delayDiffusion = 0.36;
    lead.reverbDecay = 0.72;
    lead.reverbEarlyMix = 0.31;
    lead.consoleCrosstalk = 0.05;
    lead.outputGlue = 0.34;
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
    pad.analogWarmth = 0.61;
    pad.voiceSlop = 0.42;
    pad.phaseScatter = 0.58;
    pad.chorusTone = 0.62;
    pad.delayDiffusion = 0.47;
    pad.reverbDecay = 0.86;
    pad.reverbEarlyMix = 0.37;
    pad.consoleCrosstalk = 0.08;
    pad.outputGlue = 0.41;
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
    kick.analogWarmth = 0.58;
    kick.voiceSlop = 0.19;
    kick.phaseScatter = 0.22;
    kick.chorusTone = 0.32;
    kick.delayDiffusion = 0.08;
    kick.reverbDecay = 0.36;
    kick.reverbEarlyMix = 0.18;
    kick.consoleCrosstalk = 0.07;
    kick.outputGlue = 0.55;
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
    snare.analogWarmth = 0.49;
    snare.voiceSlop = 0.24;
    snare.phaseScatter = 0.35;
    snare.chorusTone = 0.64;
    snare.delayDiffusion = 0.24;
    snare.reverbDecay = 0.64;
    snare.reverbEarlyMix = 0.42;
    snare.consoleCrosstalk = 0.12;
    snare.outputGlue = 0.43;
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
    hat.analogWarmth = 0.37;
    hat.voiceSlop = 0.2;
    hat.phaseScatter = 0.27;
    hat.chorusTone = 0.78;
    hat.delayDiffusion = 0.22;
    hat.reverbDecay = 0.52;
    hat.reverbEarlyMix = 0.36;
    hat.consoleCrosstalk = 0.16;
    hat.outputGlue = 0.32;
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

    // ===== NEW EBM/DARKWAVE PERCUSSION PATCHES =====

    // Industrial Kick — distorted, aggressive, heavy low-end for industrial/EBM
    SynthPatch indKick;
    indKick.name = "Industrial Crusher";
    indKick.oscillatorA = Waveform::Sine;
    indKick.oscillatorB = Waveform::Saw;
    indKick.oscillatorBEnabled = true;
    indKick.oscillatorMix = 0.22;
    indKick.pitchEnvelopeSemitones = 52.0;
    indKick.pitchEnvelopeDecay = 0.035;
    indKick.subOscillator = 0.55;
    indKick.noise = 0.08;
    indKick.noiseTone = 0.28;
    indKick.click = 0.78;
    indKick.transientShape = 0.88;
    indKick.transientNoise = 0.28;
    indKick.transientPitchSemitones = 18.0;
    indKick.transientPitchDecay = 0.008;
    indKick.transientBurstCount = 3;
    indKick.transientBurstSpacing = 0.0022;
    indKick.transientBurstDecay = 0.68;
    indKick.transientTone = 0.42;
    indKick.transientDecay = 0.018;
    indKick.cutoff = 0.48;
    indKick.resonance = 0.34;
    indKick.filterMode = 1;
    indKick.filterDrive = 1.0;
    indKick.filterKeytrack = 0.12;
    indKick.filterEnvelopeAmount = 0.38;
    indKick.fmEnabled = true;
    indKick.fmAmount = 0.18;
    indKick.fmRatio = 0.5;
    indKick.fmFeedback = 0.42;
    indKick.fmAlgorithm = 1;
    indKick.drive = 0.72;
    indKick.wavefold = 0.14;
    indKick.bitCrushEnabled = true;
    indKick.bitCrush = 0.04;
    indKick.combMix = 0.08;
    indKick.combTime = 0.024;
    indKick.combFeedback = 0.28;
    indKick.highPass = 0.08;
    indKick.analogColor = 0.74;
    indKick.toneTilt = -0.52;
    indKick.lowPunch = 0.88;
    indKick.gain = 1.0;
    indKick.ampEnvelope.attack = 0.0005;
    indKick.ampEnvelope.decay = 0.14;
    indKick.ampEnvelope.sustain = 0.0;
    indKick.ampEnvelope.release = 0.045;
    indKick.filterEnvelope.attack = 0.0005;
    indKick.filterEnvelope.decay = 0.08;
    indKick.filterEnvelope.sustain = 0.0;
    indKick.filterEnvelope.release = 0.06;

    // Tight Kick — short, punchy, controlled for fast EBM patterns
    SynthPatch tightKick;
    tightKick.name = "Tight EBM Kick";
    tightKick.oscillatorA = Waveform::Sine;
    tightKick.oscillatorB = Waveform::Triangle;
    tightKick.oscillatorBEnabled = true;
    tightKick.oscillatorMix = 0.12;
    tightKick.pitchEnvelopeSemitones = 36.0;
    tightKick.pitchEnvelopeDecay = 0.025;
    tightKick.subOscillator = 0.42;
    tightKick.noise = 0.04;
    tightKick.noiseTone = 0.22;
    tightKick.click = 0.62;
    tightKick.transientShape = 0.72;
    tightKick.transientNoise = 0.16;
    tightKick.transientPitchSemitones = 12.0;
    tightKick.transientPitchDecay = 0.006;
    tightKick.transientBurstCount = 2;
    tightKick.transientBurstSpacing = 0.0018;
    tightKick.transientBurstDecay = 0.74;
    tightKick.transientTone = 0.38;
    tightKick.transientDecay = 0.01;
    tightKick.cutoff = 0.62;
    tightKick.resonance = 0.18;
    tightKick.filterMode = 0;
    tightKick.filterDrive = 0.48;
    tightKick.filterKeytrack = 0.08;
    tightKick.filterEnvelopeAmount = 0.28;
    tightKick.drive = 0.28;
    tightKick.wavefold = 0.06;
    tightKick.highPass = 0.04;
    tightKick.analogColor = 0.58;
    tightKick.toneTilt = -0.42;
    tightKick.lowPunch = 0.72;
    tightKick.gain = 0.92;
    tightKick.ampEnvelope.attack = 0.0005;
    tightKick.ampEnvelope.decay = 0.085;
    tightKick.ampEnvelope.sustain = 0.0;
    tightKick.ampEnvelope.release = 0.03;
    tightKick.filterEnvelope.attack = 0.0005;
    tightKick.filterEnvelope.decay = 0.06;
    tightKick.filterEnvelope.sustain = 0.0;
    tightKick.filterEnvelope.release = 0.04;

    // Gated Snare — classic EBM gated reverb snare sound
    SynthPatch gatedSnare;
    gatedSnare.name = "Gated EBM Snare";
    gatedSnare.oscillatorA = Waveform::Noise;
    gatedSnare.oscillatorB = Waveform::Triangle;
    gatedSnare.oscillatorBEnabled = true;
    gatedSnare.oscillatorMix = 0.32;
    gatedSnare.fmEnabled = true;
    gatedSnare.fmAmount = 0.14;
    gatedSnare.fmRatio = 2.8;
    gatedSnare.fmFeedback = 0.22;
    gatedSnare.fmAlgorithm = 2;
    gatedSnare.noise = 0.88;
    gatedSnare.noiseTone = 0.76;
    gatedSnare.click = 0.42;
    gatedSnare.transientShape = 0.68;
    gatedSnare.transientNoise = 0.78;
    gatedSnare.transientPitchSemitones = 16.0;
    gatedSnare.transientPitchDecay = 0.008;
    gatedSnare.transientBurstCount = 4;
    gatedSnare.transientBurstSpacing = 0.0024;
    gatedSnare.transientBurstDecay = 0.58;
    gatedSnare.transientTone = 0.82;
    gatedSnare.transientDecay = 0.022;
    gatedSnare.pitchEnvelopeSemitones = 6.0;
    gatedSnare.pitchEnvelopeDecay = 0.025;
    gatedSnare.cutoff = 0.72;
    gatedSnare.resonance = 0.28;
    gatedSnare.filterMode = 0;
    gatedSnare.filterDrive = 0.58;
    gatedSnare.filterKeytrack = 0.22;
    gatedSnare.filterEnvelopeAmount = 0.32;
    gatedSnare.highPass = 0.32;
    gatedSnare.ringEnabled = true;
    gatedSnare.ringMod = 0.18;
    gatedSnare.drive = 0.38;
    gatedSnare.wavefold = 0.12;
    gatedSnare.bitCrushEnabled = true;
    gatedSnare.bitCrush = 0.04;
    gatedSnare.combMix = 0.22;
    gatedSnare.combTime = 0.038;
    gatedSnare.combFeedback = 0.42;
    gatedSnare.reverbMix = 0.28;
    gatedSnare.reverbSize = 0.72;
    gatedSnare.reverbDamping = 0.38;
    gatedSnare.reverbPreDelay = 0.02;
    gatedSnare.analogColor = 0.62;
    gatedSnare.toneTilt = 0.18;
    gatedSnare.gain = 0.68;
    gatedSnare.ampEnvelope.attack = 0.001;
    gatedSnare.ampEnvelope.decay = 0.14;
    gatedSnare.ampEnvelope.sustain = 0.0;
    gatedSnare.ampEnvelope.release = 0.18;
    gatedSnare.filterEnvelope.attack = 0.001;
    gatedSnare.filterEnvelope.decay = 0.06;
    gatedSnare.filterEnvelope.sustain = 0.0;
    gatedSnare.filterEnvelope.release = 0.12;

    // Industrial Snare — harsh, metallic, distorted for aggressive industrial
    SynthPatch indSnare;
    indSnare.name = "Industrial Strike";
    indSnare.oscillatorA = Waveform::Noise;
    indSnare.oscillatorB = Waveform::Square;
    indSnare.oscillatorBEnabled = true;
    indSnare.oscillatorMix = 0.18;
    indSnare.oscillatorC = Waveform::Noise;
    indSnare.oscillatorCEnabled = true;
    indSnare.oscillatorCMix = 0.38;
    indSnare.fmEnabled = true;
    indSnare.fmAmount = 0.32;
    indSnare.fmRatio = 4.2;
    indSnare.fmFeedback = 0.38;
    indSnare.fmAlgorithm = 3;
    indSnare.noise = 0.95;
    indSnare.noiseTone = 0.92;
    indSnare.click = 0.55;
    indSnare.transientShape = 0.92;
    indSnare.transientNoise = 0.88;
    indSnare.transientPitchSemitones = 28.0;
    indSnare.transientPitchDecay = 0.006;
    indSnare.transientBurstCount = 5;
    indSnare.transientBurstSpacing = 0.0018;
    indSnare.transientBurstDecay = 0.48;
    indSnare.transientTone = 0.96;
    indSnare.transientDecay = 0.028;
    indSnare.cutoff = 0.86;
    indSnare.resonance = 0.42;
    indSnare.filterMode = 2;
    indSnare.filterDrive = 0.88;
    indSnare.filterKeytrack = 0.15;
    indSnare.filterEnvelopeAmount = 0.45;
    indSnare.highPass = 0.48;
    indSnare.ringEnabled = true;
    indSnare.ringMod = 0.42;
    indSnare.drive = 0.68;
    indSnare.wavefold = 0.28;
    indSnare.bitCrushEnabled = true;
    indSnare.bitCrush = 0.12;
    indSnare.sampleRateReduction = 0.08;
    indSnare.combMix = 0.14;
    indSnare.combTime = 0.032;
    indSnare.combFeedback = 0.52;
    indSnare.analogColor = 0.78;
    indSnare.toneTilt = 0.42;
    indSnare.gain = 0.72;
    indSnare.ampEnvelope.attack = 0.0005;
    indSnare.ampEnvelope.decay = 0.1;
    indSnare.ampEnvelope.sustain = 0.0;
    indSnare.ampEnvelope.release = 0.12;
    indSnare.filterEnvelope.attack = 0.0005;
    indSnare.filterEnvelope.decay = 0.05;
    indSnare.filterEnvelope.sustain = 0.0;
    indSnare.filterEnvelope.release = 0.08;

    // Closed Hat — tight, crisp, metallic for fast hi-hat patterns
    SynthPatch closedHat;
    closedHat.name = "Closed Metal Hat";
    closedHat.oscillatorA = Waveform::Noise;
    closedHat.oscillatorB = Waveform::Square;
    closedHat.oscillatorBEnabled = true;
    closedHat.oscillatorMix = 0.72;
    closedHat.pulseWidth = 0.32;
    closedHat.fmEnabled = true;
    closedHat.fmAmount = 0.22;
    closedHat.fmRatio = 8.0;
    closedHat.fmFeedback = 0.28;
    closedHat.fmAlgorithm = 3;
    closedHat.noise = 0.88;
    closedHat.noiseTone = 0.96;
    closedHat.click = 0.28;
    closedHat.transientShape = 0.58;
    closedHat.transientNoise = 0.62;
    closedHat.transientPitchSemitones = 18.0;
    closedHat.transientPitchDecay = 0.005;
    closedHat.transientBurstCount = 3;
    closedHat.transientBurstSpacing = 0.0016;
    closedHat.transientBurstDecay = 0.55;
    closedHat.transientTone = 0.98;
    closedHat.transientDecay = 0.006;
    closedHat.cutoff = 0.94;
    closedHat.resonance = 0.38;
    closedHat.filterMode = 2;
    closedHat.filterDrive = 0.52;
    closedHat.filterKeytrack = 0.1;
    closedHat.highPass = 0.72;
    closedHat.ringEnabled = true;
    closedHat.ringMod = 0.48;
    closedHat.hardSyncEnabled = true;
    closedHat.hardSync = 0.38;
    closedHat.bitCrushEnabled = true;
    closedHat.bitCrush = 0.18;
    closedHat.sampleRateReduction = 0.14;
    closedHat.analogColor = 0.48;
    closedHat.toneTilt = 0.52;
    closedHat.gain = 0.26;
    closedHat.ampEnvelope.attack = 0.0005;
    closedHat.ampEnvelope.decay = 0.018;
    closedHat.ampEnvelope.sustain = 0.0;
    closedHat.ampEnvelope.release = 0.015;

    // Open Hat — longer, sizzling, with more body
    SynthPatch openHat;
    openHat.name = "Open Sizzle Hat";
    openHat.oscillatorA = Waveform::Noise;
    openHat.oscillatorB = Waveform::Square;
    openHat.oscillatorBEnabled = true;
    openHat.oscillatorC = Waveform::Noise;
    openHat.oscillatorCEnabled = true;
    openHat.oscillatorCMix = 0.42;
    openHat.oscillatorMix = 0.58;
    openHat.pulseWidth = 0.28;
    openHat.fmEnabled = true;
    openHat.fmAmount = 0.18;
    openHat.fmRatio = 6.5;
    openHat.fmFeedback = 0.22;
    openHat.fmAlgorithm = 3;
    openHat.noise = 0.92;
    openHat.noiseTone = 0.94;
    openHat.click = 0.22;
    openHat.transientShape = 0.48;
    openHat.transientNoise = 0.72;
    openHat.transientPitchSemitones = 14.0;
    openHat.transientPitchDecay = 0.006;
    openHat.transientBurstCount = 4;
    openHat.transientBurstSpacing = 0.002;
    openHat.transientBurstDecay = 0.52;
    openHat.transientTone = 0.96;
    openHat.transientDecay = 0.012;
    openHat.cutoff = 0.88;
    openHat.resonance = 0.32;
    openHat.filterMode = 2;
    openHat.filterDrive = 0.44;
    openHat.highPass = 0.68;
    openHat.ringEnabled = true;
    openHat.ringMod = 0.38;
    openHat.bitCrushEnabled = true;
    openHat.bitCrush = 0.12;
    openHat.sampleRateReduction = 0.1;
    openHat.combMix = 0.08;
    openHat.combTime = 0.018;
    openHat.combFeedback = 0.35;
    openHat.analogColor = 0.52;
    openHat.toneTilt = 0.44;
    openHat.gain = 0.3;
    openHat.ampEnvelope.attack = 0.0005;
    openHat.ampEnvelope.decay = 0.065;
    openHat.ampEnvelope.sustain = 0.0;
    openHat.ampEnvelope.release = 0.045;

    // Crash Cymbal — bright, explosive, with long tail
    SynthPatch crash;
    crash.name = "Industrial Crash";
    crash.oscillatorA = Waveform::Noise;
    crash.oscillatorB = Waveform::Noise;
    crash.oscillatorBEnabled = true;
    crash.oscillatorC = Waveform::Square;
    crash.oscillatorCEnabled = true;
    crash.oscillatorCMix = 0.28;
    crash.oscillatorMix = 0.45;
    crash.fmEnabled = true;
    crash.fmAmount = 0.28;
    crash.fmRatio = 10.0;
    crash.fmFeedback = 0.32;
    crash.fmAlgorithm = 3;
    crash.noise = 0.98;
    crash.noiseTone = 0.99;
    crash.click = 0.15;
    crash.transientShape = 0.55;
    crash.transientNoise = 0.82;
    crash.transientPitchSemitones = 10.0;
    crash.transientPitchDecay = 0.005;
    crash.transientBurstCount = 6;
    crash.transientBurstSpacing = 0.0014;
    crash.transientBurstDecay = 0.45;
    crash.transientTone = 0.99;
    crash.transientDecay = 0.035;
    crash.cutoff = 0.96;
    crash.resonance = 0.28;
    crash.filterMode = 2;
    crash.filterDrive = 0.42;
    crash.highPass = 0.82;
    crash.ringEnabled = true;
    crash.ringMod = 0.28;
    crash.bitCrushEnabled = true;
    crash.bitCrush = 0.08;
    crash.sampleRateReduction = 0.12;
    crash.combMix = 0.12;
    crash.combTime = 0.022;
    crash.combFeedback = 0.48;
    crash.reverbMix = 0.18;
    crash.reverbSize = 0.82;
    crash.reverbDamping = 0.28;
    crash.analogColor = 0.56;
    crash.toneTilt = 0.62;
    crash.gain = 0.34;
    crash.ampEnvelope.attack = 0.001;
    crash.ampEnvelope.decay = 0.38;
    crash.ampEnvelope.sustain = 0.0;
    crash.ampEnvelope.release = 0.28;

    // Industrial Clap — layered, distorted, with reverb tail
    SynthPatch indClap;
    indClap.name = "Industrial Clap";
    indClap.oscillatorA = Waveform::Noise;
    indClap.oscillatorB = Waveform::Square;
    indClap.oscillatorBEnabled = true;
    indClap.oscillatorC = Waveform::Noise;
    indClap.oscillatorCEnabled = true;
    indClap.oscillatorCMix = 0.55;
    indClap.oscillatorMix = 0.28;
    indClap.fmEnabled = true;
    indClap.fmAmount = 0.18;
    indClap.fmRatio = 3.2;
    indClap.fmFeedback = 0.28;
    indClap.fmAlgorithm = 3;
    indClap.noise = 0.9;
    indClap.noiseTone = 0.88;
    indClap.click = 0.48;
    indClap.transientShape = 0.82;
    indClap.transientNoise = 0.75;
    indClap.transientPitchSemitones = 12.0;
    indClap.transientPitchDecay = 0.008;
    indClap.transientBurstCount = 6;
    indClap.transientBurstSpacing = 0.0036;
    indClap.transientBurstDecay = 0.58;
    indClap.transientTone = 0.92;
    indClap.transientDecay = 0.018;
    indClap.cutoff = 0.82;
    indClap.resonance = 0.32;
    indClap.filterMode = 2;
    indClap.filterDrive = 0.62;
    indClap.highPass = 0.45;
    indClap.drive = 0.48;
    indClap.wavefold = 0.16;
    indClap.bitCrushEnabled = true;
    indClap.bitCrush = 0.06;
    indClap.combMix = 0.18;
    indClap.combTime = 0.028;
    indClap.combFeedback = 0.42;
    indClap.reverbMix = 0.22;
    indClap.reverbSize = 0.68;
    indClap.reverbDamping = 0.42;
    indClap.analogColor = 0.58;
    indClap.toneTilt = 0.28;
    indClap.gain = 0.52;
    indClap.ampEnvelope.attack = 0.001;
    indClap.ampEnvelope.decay = 0.065;
    indClap.ampEnvelope.sustain = 0.0;
    indClap.ampEnvelope.release = 0.1;

    // Shaker — bright, rhythmic, metallic rattle
    SynthPatch shaker;
    shaker.name = "Metal Shaker";
    shaker.oscillatorA = Waveform::Noise;
    shaker.oscillatorB = Waveform::Triangle;
    shaker.oscillatorBEnabled = true;
    shaker.oscillatorMix = 0.55;
    shaker.fmEnabled = true;
    shaker.fmAmount = 0.12;
    shaker.fmRatio = 12.0;
    shaker.fmFeedback = 0.18;
    shaker.fmAlgorithm = 3;
    shaker.noise = 0.78;
    shaker.noiseTone = 0.88;
    shaker.click = 0.12;
    shaker.transientShape = 0.42;
    shaker.transientNoise = 0.55;
    shaker.transientPitchSemitones = 8.0;
    shaker.transientPitchDecay = 0.004;
    shaker.transientBurstCount = 5;
    shaker.transientBurstSpacing = 0.0012;
    shaker.transientBurstDecay = 0.62;
    shaker.transientTone = 0.92;
    shaker.transientDecay = 0.008;
    shaker.cutoff = 0.92;
    shaker.resonance = 0.36;
    shaker.filterMode = 2;
    shaker.filterDrive = 0.38;
    shaker.highPass = 0.68;
    shaker.ringEnabled = true;
    shaker.ringMod = 0.22;
    shaker.bitCrushEnabled = true;
    shaker.bitCrush = 0.1;
    shaker.analogColor = 0.44;
    shaker.toneTilt = 0.48;
    shaker.gain = 0.22;
    shaker.ampEnvelope.attack = 0.0005;
    shaker.ampEnvelope.decay = 0.028;
    shaker.ampEnvelope.sustain = 0.0;
    shaker.ampEnvelope.release = 0.02;

    // Floor Tom — deep, resonant, with pitch drop
    SynthPatch floorTom;
    floorTom.name = "Floor Tom Deep";
    floorTom.oscillatorA = Waveform::Sine;
    floorTom.oscillatorB = Waveform::Triangle;
    floorTom.oscillatorBEnabled = true;
    floorTom.oscillatorMix = 0.28;
    floorTom.pitchEnvelopeSemitones = 18.0;
    floorTom.pitchEnvelopeDecay = 0.065;
    floorTom.subOscillator = 0.38;
    floorTom.noise = 0.12;
    floorTom.noiseTone = 0.42;
    floorTom.click = 0.22;
    floorTom.transientShape = 0.38;
    floorTom.transientNoise = 0.22;
    floorTom.transientPitchSemitones = 8.0;
    floorTom.transientPitchDecay = 0.012;
    floorTom.transientBurstCount = 2;
    floorTom.transientBurstSpacing = 0.003;
    floorTom.transientBurstDecay = 0.72;
    floorTom.transientTone = 0.52;
    floorTom.transientDecay = 0.025;
    floorTom.cutoff = 0.42;
    floorTom.resonance = 0.22;
    floorTom.filterMode = 1;
    floorTom.filterDrive = 0.48;
    floorTom.filterKeytrack = 0.28;
    floorTom.filterEnvelopeAmount = 0.18;
    floorTom.drive = 0.28;
    floorTom.wavefold = 0.06;
    floorTom.combMix = 0.1;
    floorTom.combTime = 0.055;
    floorTom.combFeedback = 0.32;
    floorTom.analogColor = 0.54;
    floorTom.toneTilt = -0.22;
    floorTom.gain = 0.62;
    floorTom.ampEnvelope.attack = 0.001;
    floorTom.ampEnvelope.decay = 0.22;
    floorTom.ampEnvelope.sustain = 0.0;
    floorTom.ampEnvelope.release = 0.14;

    // High Tom — bright, punchy, higher pitch
    SynthPatch highTom;
    highTom.name = "High Tom Bright";
    highTom.oscillatorA = Waveform::Sine;
    highTom.oscillatorB = Waveform::Triangle;
    highTom.oscillatorBEnabled = true;
    highTom.oscillatorMix = 0.22;
    highTom.pitchEnvelopeSemitones = 28.0;
    highTom.pitchEnvelopeDecay = 0.045;
    highTom.subOscillator = 0.18;
    highTom.noise = 0.18;
    highTom.noiseTone = 0.58;
    highTom.click = 0.28;
    highTom.transientShape = 0.48;
    highTom.transientNoise = 0.32;
    highTom.transientPitchSemitones = 12.0;
    highTom.transientPitchDecay = 0.008;
    highTom.transientBurstCount = 3;
    highTom.transientBurstSpacing = 0.0024;
    highTom.transientBurstDecay = 0.65;
    highTom.transientTone = 0.68;
    highTom.transientDecay = 0.018;
    highTom.cutoff = 0.58;
    highTom.resonance = 0.26;
    highTom.filterMode = 0;
    highTom.filterDrive = 0.52;
    highTom.filterKeytrack = 0.32;
    highTom.filterEnvelopeAmount = 0.22;
    highTom.drive = 0.22;
    highTom.wavefold = 0.08;
    highTom.analogColor = 0.52;
    highTom.toneTilt = -0.08;
    highTom.gain = 0.55;
    highTom.ampEnvelope.attack = 0.001;
    highTom.ampEnvelope.decay = 0.16;
    highTom.ampEnvelope.sustain = 0.0;
    highTom.ampEnvelope.release = 0.1;

    // Industrial Noise Hit — pure noise burst for industrial percussion
    SynthPatch noiseHit;
    noiseHit.name = "Noise Burst";
    noiseHit.oscillatorA = Waveform::Noise;
    noiseHit.oscillatorB = Waveform::Noise;
    noiseHit.oscillatorBEnabled = true;
    noiseHit.oscillatorC = Waveform::Square;
    noiseHit.oscillatorCEnabled = true;
    noiseHit.oscillatorCMix = 0.22;
    noiseHit.oscillatorMix = 0.35;
    noiseHit.fmEnabled = true;
    noiseHit.fmAmount = 0.42;
    noiseHit.fmRatio = 1.8;
    noiseHit.fmFeedback = 0.55;
    noiseHit.fmAlgorithm = 3;
    noiseHit.noise = 1.0;
    noiseHit.noiseTone = 0.85;
    noiseHit.click = 0.35;
    noiseHit.transientShape = 0.95;
    noiseHit.transientNoise = 0.95;
    noiseHit.transientPitchSemitones = 32.0;
    noiseHit.transientPitchDecay = 0.004;
    noiseHit.transientBurstCount = 7;
    noiseHit.transientBurstSpacing = 0.001;
    noiseHit.transientBurstDecay = 0.38;
    noiseHit.transientTone = 0.98;
    noiseHit.transientDecay = 0.04;
    noiseHit.cutoff = 0.78;
    noiseHit.resonance = 0.48;
    noiseHit.filterMode = 2;
    noiseHit.filterDrive = 1.0;
    noiseHit.highPass = 0.35;
    noiseHit.drive = 0.82;
    noiseHit.wavefold = 0.32;
    noiseHit.bitCrushEnabled = true;
    noiseHit.bitCrush = 0.18;
    noiseHit.sampleRateReduction = 0.16;
    noiseHit.combMix = 0.22;
    noiseHit.combTime = 0.015;
    noiseHit.combFeedback = 0.62;
    noiseHit.analogColor = 0.82;
    noiseHit.toneTilt = 0.55;
    noiseHit.gain = 0.58;
    noiseHit.ampEnvelope.attack = 0.0005;
    noiseHit.ampEnvelope.decay = 0.08;
    noiseHit.ampEnvelope.sustain = 0.0;
    noiseHit.ampEnvelope.release = 0.06;

    // ===== NEW EBM/DARKWAVE BASS PATCHES =====

    // Sub Destroyer — pure sub bass for EBM low-end foundation
    SynthPatch subBass;
    subBass.name = "Sub Destroyer";
    subBass.oscillatorA = Waveform::Sine;
    subBass.oscillatorB = Waveform::Sine;
    subBass.oscillatorBEnabled = true;
    subBass.oscillatorMix = 0.5;
    subBass.detuneCents = -4.0;
    subBass.subEnabled = true;
    subBass.subOscillator = 0.88;
    subBass.cutoff = 0.18;
    subBass.resonance = 0.12;
    subBass.filterMode = 0;
    subBass.filterDrive = 0.35;
    subBass.filterKeytrack = 0.55;
    subBass.filterEnvelopeAmount = 0.28;
    subBass.lfoFilterDepth = 0.02;
    subBass.drive = 0.18;
    subBass.wavefold = 0.04;
    subBass.analogColor = 0.62;
    subBass.toneTilt = -0.72;
    subBass.lowPunch = 0.95;
    subBass.gain = 0.78;
    subBass.ampEnvelope.attack = 0.001;
    subBass.ampEnvelope.decay = 0.06;
    subBass.ampEnvelope.sustain = 0.72;
    subBass.ampEnvelope.release = 0.08;
    subBass.filterEnvelope.attack = 0.001;
    subBass.filterEnvelope.decay = 0.1;
    subBass.filterEnvelope.sustain = 0.22;
    subBass.filterEnvelope.release = 0.08;

    // EBM Distortion Bass — aggressive, distorted bass for industrial EBM
    SynthPatch ebmDistBass;
    ebmDistBass.name = "EBM Distortion";
    ebmDistBass.oscillatorA = Waveform::Saw;
    ebmDistBass.oscillatorB = Waveform::Square;
    ebmDistBass.oscillatorBEnabled = true;
    ebmDistBass.oscillatorC = Waveform::Saw;
    ebmDistBass.oscillatorCEnabled = true;
    ebmDistBass.oscillatorCMix = 0.28;
    ebmDistBass.oscillatorMix = 0.45;
    ebmDistBass.detuneCents = 8.0;
    ebmDistBass.detuneCCents = -12.0;
    ebmDistBass.pulseWidth = 0.38;
    ebmDistBass.pwmDepth = 0.08;
    ebmDistBass.unisonVoices = 3;
    ebmDistBass.unisonDetuneCents = 5.5;
    ebmDistBass.stereoSpread = 0.12;
    ebmDistBass.subOscillator = 0.55;
    ebmDistBass.cutoff = 0.22;
    ebmDistBass.resonance = 0.38;
    ebmDistBass.filterMode = 1;
    ebmDistBass.filterDrive = 0.95;
    ebmDistBass.filterKeytrack = 0.48;
    ebmDistBass.filterEnvelopeAmount = 0.55;
    ebmDistBass.lfoFilterDepth = 0.08;
    ebmDistBass.hardSyncEnabled = true;
    ebmDistBass.hardSync = 0.18;
    ebmDistBass.drive = 0.72;
    ebmDistBass.wavefold = 0.22;
    ebmDistBass.bitCrushEnabled = true;
    ebmDistBass.bitCrush = 0.04;
    ebmDistBass.combMix = 0.12;
    ebmDistBass.combTime = 0.035;
    ebmDistBass.combFeedback = 0.38;
    ebmDistBass.analogColor = 0.78;
    ebmDistBass.toneTilt = -0.45;
    ebmDistBass.lowPunch = 0.82;
    ebmDistBass.gain = 0.58;
    ebmDistBass.ampEnvelope.attack = 0.001;
    ebmDistBass.ampEnvelope.decay = 0.08;
    ebmDistBass.ampEnvelope.sustain = 0.52;
    ebmDistBass.ampEnvelope.release = 0.09;
    ebmDistBass.filterEnvelope.attack = 0.001;
    ebmDistBass.filterEnvelope.decay = 0.12;
    ebmDistBass.filterEnvelope.sustain = 0.18;
    ebmDistBass.filterEnvelope.release = 0.08;

    // Darkwave Bass — moody, warm, with chorus and movement
    SynthPatch darkBass;
    darkBass.name = "Darkwave Bass";
    darkBass.oscillatorA = Waveform::Triangle;
    darkBass.oscillatorB = Waveform::Saw;
    darkBass.oscillatorBEnabled = true;
    darkBass.oscillatorC = Waveform::Sine;
    darkBass.oscillatorCEnabled = true;
    darkBass.oscillatorCMix = 0.22;
    darkBass.oscillatorMix = 0.42;
    darkBass.detuneCents = -6.0;
    darkBass.detuneCCents = 4.0;
    darkBass.pulseWidth = 0.48;
    darkBass.pwmDepth = 0.06;
    darkBass.unisonVoices = 2;
    darkBass.unisonDetuneCents = 4.0;
    darkBass.stereoSpread = 0.18;
    darkBass.subOscillator = 0.42;
    darkBass.cutoff = 0.32;
    darkBass.resonance = 0.28;
    darkBass.filterMode = 0;
    darkBass.filterDrive = 0.55;
    darkBass.filterKeytrack = 0.42;
    darkBass.filterEnvelopeAmount = 0.38;
    darkBass.lfoFilterDepth = 0.12;
    darkBass.lfoRate = 3.2;
    darkBass.vibratoCents = 3.0;
    darkBass.chorusEnabled = true;
    darkBass.chorusMix = 0.14;
    darkBass.chorusRate = 0.28;
    darkBass.chorusDepth = 0.42;
    darkBass.drive = 0.32;
    darkBass.wavefold = 0.1;
    darkBass.analogColor = 0.68;
    darkBass.toneTilt = -0.28;
    darkBass.lowPunch = 0.58;
    darkBass.gain = 0.52;
    darkBass.ampEnvelope.attack = 0.003;
    darkBass.ampEnvelope.decay = 0.1;
    darkBass.ampEnvelope.sustain = 0.62;
    darkBass.ampEnvelope.release = 0.14;
    darkBass.filterEnvelope.attack = 0.002;
    darkBass.filterEnvelope.decay = 0.14;
    darkBass.filterEnvelope.sustain = 0.28;
    darkBass.filterEnvelope.release = 0.12;

    // FM Industrial Bass — metallic, complex, FM-driven bass
    SynthPatch fmBass;
    fmBass.name = "FM Industrial";
    fmBass.oscillatorA = Waveform::Sine;
    fmBass.oscillatorB = Waveform::Sine;
    fmBass.oscillatorBEnabled = true;
    fmBass.oscillatorC = Waveform::Triangle;
    fmBass.oscillatorCEnabled = true;
    fmBass.oscillatorCMix = 0.18;
    fmBass.oscillatorMix = 0.38;
    fmBass.fmEnabled = true;
    fmBass.fmAmount = 0.38;
    fmBass.fmRatio = 1.5;
    fmBass.fmFeedback = 0.45;
    fmBass.fmAlgorithm = 0;
    fmBass.subOscillator = 0.48;
    fmBass.cutoff = 0.26;
    fmBass.resonance = 0.32;
    fmBass.filterMode = 0;
    fmBass.filterDrive = 0.72;
    fmBass.filterKeytrack = 0.38;
    fmBass.filterEnvelopeAmount = 0.48;
    fmBass.lfoFilterDepth = 0.06;
    fmBass.drive = 0.48;
    fmBass.wavefold = 0.16;
    fmBass.bitCrushEnabled = true;
    fmBass.bitCrush = 0.03;
    fmBass.combMix = 0.08;
    fmBass.combTime = 0.04;
    fmBass.combFeedback = 0.28;
    fmBass.analogColor = 0.72;
    fmBass.toneTilt = -0.38;
    fmBass.lowPunch = 0.68;
    fmBass.gain = 0.55;
    fmBass.ampEnvelope.attack = 0.001;
    fmBass.ampEnvelope.decay = 0.07;
    fmBass.ampEnvelope.sustain = 0.55;
    fmBass.ampEnvelope.release = 0.1;
    fmBass.filterEnvelope.attack = 0.001;
    fmBass.filterEnvelope.decay = 0.1;
    fmBass.filterEnvelope.sustain = 0.22;
    fmBass.filterEnvelope.release = 0.08;

    // Reese Dark — dark, moving reese bass with heavy unison
    SynthPatch reeseDark;
    reeseDark.name = "Reese Dark";
    reeseDark.oscillatorA = Waveform::Saw;
    reeseDark.oscillatorB = Waveform::Saw;
    reeseDark.oscillatorBEnabled = true;
    reeseDark.oscillatorC = Waveform::Square;
    reeseDark.oscillatorCEnabled = true;
    reeseDark.oscillatorD = Waveform::Noise;
    reeseDark.oscillatorDEnabled = true;
    reeseDark.oscillatorMix = 0.48;
    reeseDark.oscillatorCMix = 0.18;
    reeseDark.oscillatorDMix = 0.08;
    reeseDark.unisonVoices = 6;
    reeseDark.unisonDetuneCents = 18.0;
    reeseDark.stereoSpread = 0.42;
    reeseDark.detuneCents = 22.0;
    reeseDark.detuneCCents = -26.0;
    reeseDark.detuneDCents = 12.0;
    reeseDark.subOscillator = 0.55;
    reeseDark.cutoff = 0.18;
    reeseDark.resonance = 0.35;
    reeseDark.filterMode = 1;
    reeseDark.filterDrive = 0.78;
    reeseDark.filterKeytrack = 0.42;
    reeseDark.filterEnvelopeAmount = 0.22;
    reeseDark.lfoFilterDepth = 0.22;
    reeseDark.lfoRate = 4.5;
    reeseDark.hardSyncEnabled = true;
    reeseDark.hardSync = 0.12;
    reeseDark.drive = 0.62;
    reeseDark.wavefold = 0.18;
    reeseDark.bitCrushEnabled = true;
    reeseDark.bitCrush = 0.05;
    reeseDark.combMix = 0.18;
    reeseDark.combTime = 0.042;
    reeseDark.combFeedback = 0.35;
    reeseDark.highPass = 0.04;
    reeseDark.analogColor = 0.74;
    reeseDark.toneTilt = -0.42;
    reeseDark.gain = 0.52;
    reeseDark.ampEnvelope.attack = 0.002;
    reeseDark.ampEnvelope.decay = 0.12;
    reeseDark.ampEnvelope.sustain = 0.68;
    reeseDark.ampEnvelope.release = 0.18;
    reeseDark.filterEnvelope.attack = 0.001;
    reeseDark.filterEnvelope.decay = 0.14;
    reeseDark.filterEnvelope.sustain = 0.15;
    reeseDark.filterEnvelope.release = 0.12;

    // Acid Scream — aggressive acid bass with screaming resonance
    SynthPatch acidScream;
    acidScream.name = "Acid Scream";
    acidScream.oscillatorA = Waveform::Saw;
    acidScream.oscillatorB = Waveform::Square;
    acidScream.oscillatorBEnabled = true;
    acidScream.oscillatorC = Waveform::Saw;
    acidScream.oscillatorCEnabled = true;
    acidScream.oscillatorCMix = 0.22;
    acidScream.oscillatorMix = 0.55;
    acidScream.detuneCents = 6.0;
    acidScream.detuneCCents = -14.0;
    acidScream.pulseWidth = 0.32;
    acidScream.pwmDepth = 0.12;
    acidScream.subOscillator = 0.22;
    acidScream.cutoff = 0.2;
    acidScream.resonance = 0.55;
    acidScream.filterMode = 1;
    acidScream.filterDrive = 1.0;
    acidScream.filterKeytrack = 0.55;
    acidScream.filterEnvelopeAmount = 0.72;
    acidScream.lfoFilterDepth = 0.18;
    acidScream.lfoRate = 6.5;
    acidScream.hardSyncEnabled = true;
    acidScream.hardSync = 0.32;
    acidScream.drive = 0.58;
    acidScream.wavefold = 0.28;
    acidScream.combMix = 0.22;
    acidScream.combTime = 0.028;
    acidScream.combFeedback = 0.48;
    acidScream.analogColor = 0.82;
    acidScream.toneTilt = -0.32;
    acidScream.lowPunch = 0.62;
    acidScream.gain = 0.42;
    acidScream.ampEnvelope.attack = 0.0005;
    acidScream.ampEnvelope.decay = 0.06;
    acidScream.ampEnvelope.sustain = 0.42;
    acidScream.ampEnvelope.release = 0.06;
    acidScream.filterEnvelope.attack = 0.0005;
    acidScream.filterEnvelope.decay = 0.08;
    acidScream.filterEnvelope.sustain = 0.12;
    acidScream.filterEnvelope.release = 0.06;

    // Apply polish to all new patches
    applyVintageHiFiPolish(bass, 0.82, false);
    applyVintageHiFiPolish(kick, 0.66, true);
    applyVintageHiFiPolish(snare, 0.70, true);
    applyVintageHiFiPolish(hat, 0.60, true);
    applyVintageHiFiPolish(lead, 0.76, false);
    applyVintageHiFiPolish(pad, 0.92, false);
    applyVintageHiFiPolish(arp, 0.78, false);
    applyVintageHiFiPolish(stab, 0.74, false);
    applyVintageHiFiPolish(drone, 0.94, false);
    applyVintageHiFiPolish(clap, 0.68, true);
    applyVintageHiFiPolish(tom, 0.64, true);
    applyVintageHiFiPolish(ride, 0.58, true);
    applyVintageHiFiPolish(acid, 0.72, false);
    applyVintageHiFiPolish(bell, 0.86, false);
    applyVintageHiFiPolish(choir, 0.98, false);
    applyVintageHiFiPolish(reese, 0.84, false);
    applyVintageHiFiPolish(rim, 0.62, true);
    applyVintageHiFiPolish(indKick, 0.72, true);
    applyVintageHiFiPolish(tightKick, 0.62, true);
    applyVintageHiFiPolish(gatedSnare, 0.68, true);
    applyVintageHiFiPolish(indSnare, 0.74, true);
    applyVintageHiFiPolish(closedHat, 0.58, true);
    applyVintageHiFiPolish(openHat, 0.56, true);
    applyVintageHiFiPolish(crash, 0.60, true);
    applyVintageHiFiPolish(indClap, 0.66, true);
    applyVintageHiFiPolish(shaker, 0.52, true);
    applyVintageHiFiPolish(floorTom, 0.62, true);
    applyVintageHiFiPolish(highTom, 0.60, true);
    applyVintageHiFiPolish(noiseHit, 0.78, true);
    applyVintageHiFiPolish(subBass, 0.76, false);
    applyVintageHiFiPolish(ebmDistBass, 0.80, false);
    applyVintageHiFiPolish(darkBass, 0.74, false);
    applyVintageHiFiPolish(fmBass, 0.78, false);
    applyVintageHiFiPolish(reeseDark, 0.82, false);
    applyVintageHiFiPolish(acidScream, 0.76, false);

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
    tracker.addInstrument(indKick);
    tracker.addInstrument(tightKick);
    tracker.addInstrument(gatedSnare);
    tracker.addInstrument(indSnare);
    tracker.addInstrument(closedHat);
    tracker.addInstrument(openHat);
    tracker.addInstrument(crash);
    tracker.addInstrument(indClap);
    tracker.addInstrument(shaker);
    tracker.addInstrument(floorTom);
    tracker.addInstrument(highTom);
    tracker.addInstrument(noiseHit);
    tracker.addInstrument(subBass);
    tracker.addInstrument(ebmDistBass);
    tracker.addInstrument(darkBass);
    tracker.addInstrument(fmBass);
    tracker.addInstrument(reeseDark);
    tracker.addInstrument(acidScream);

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
    applyVintageHiFiPolish(initPatch, 0.52, false);
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
