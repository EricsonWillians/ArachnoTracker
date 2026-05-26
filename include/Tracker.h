#pragma once

#include <string>
#include <vector>

#include "Instrument.h"
#include "Pattern.h"

namespace arachno {

struct Track {
    std::string name = "Track";
    double volume = 0.85;
    double pan = 0.0;
    bool muted = false;
    bool solo = false;
};

class Song {
public:
    std::string title = "Untitled";
    std::string author;
    std::string description;
    std::string notes;
    double bpm = 138.0;
    int rowsPerBeat = 4;
    int sampleRate = 48000;

    std::vector<Track> tracks;
    std::vector<Instrument> instruments;
    std::vector<Pattern> patterns;
    std::vector<int> order;

    double secondsPerRow() const;
    int totalRows() const;
    double durationSeconds() const;
};

class Tracker {
public:
    Song& song() { return song_; }
    const Song& song() const { return song_; }

    int addTrack(const std::string& name);
    int addInstrument(const SynthPatch& patch);
    int addPattern(const Pattern& pattern);
    void appendPatternToOrder(int patternIndex);

private:
    Song song_;
};

Song makeDemoSong();
Song makeTemplateSong(const std::string& templateName);
std::vector<std::string> demoTemplateNames();

} // namespace arachno
