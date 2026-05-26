#pragma once

#include <string>
#include <vector>

#include "Tracker.h"

namespace arachno {

struct MidiImportOptions {
    int rowsPerBeat = 4;
    int patternRows = 64;
    int sampleRate = 48000;
    bool splitByTrack = true;
    bool splitByProgram = true;
    bool preserveTempoMap = true;
};

struct MidiImportWarning {
    std::string message;
};

struct MidiImportTrackMapping {
    int trackIndex = -1;
    std::string trackName;
    int instrumentIndex = -1;
    std::string instrumentName;
    int midiSourceTrack = -1;
    int midiChannel = -1;
    int dominantProgram = 0;
};

struct MidiImportReport {
    Song song;
    int midiFormat = 1;
    int ticksPerQuarterNote = 480;
    int importedTrackCount = 0;
    int importedNoteCount = 0;
    std::vector<MidiImportTrackMapping> trackMappings;
    std::vector<MidiImportWarning> warnings;
};

MidiImportReport importMidiFile(const std::string& path, const MidiImportOptions& options = MidiImportOptions {});
Song loadMidiFileAsSong(const std::string& path, const MidiImportOptions& options = MidiImportOptions {});

} // namespace arachno
