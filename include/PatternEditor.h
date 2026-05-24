#pragma once

#include <string>
#include <vector>

#include "Tracker.h"

namespace arachno {

struct EditorCursor {
    int pattern = 0;
    int row = 0;
    int track = 0;
};

class PatternEditorSession {
public:
    explicit PatternEditorSession(Song& song);

    const EditorCursor& cursor() const { return cursor_; }

    void selectPattern(int pattern);
    void moveTo(int row, int track);
    void moveBy(int rowDelta, int trackDelta);
    void enterNote(const Note& note, float velocity = 1.0f);
    void clearStep();
    void setInstrument(int instrument);
    void setGate(double gateRows);
    void setAutomation(const std::string& parameter, double value);
    void clearAutomation(const std::string& parameter);
    void clearAllAutomation();
    void transposeCurrent(int semitones);
    void transposeTrack(int track, int semitones);
    void setTempo(double bpm);
    void setRowsPerBeat(int rowsPerBeat);
    void renameActivePattern(const std::string& name);
    int createPattern(const std::string& name, int rows, int tracks = -1);
    int cloneActivePattern(const std::string& name);
    void appendOrder(int pattern = -1);
    void setOrder(const std::vector<int>& order);
    int createTrack(const std::string& name);
    int duplicateTrack(int sourceTrack, const std::string& name);
    void renameTrack(int track, const std::string& name);
    void clearTrack(int track);
    void resizeActivePattern(int rows);
    void setTrackVolume(int track, double volume);
    void setTrackPan(int track, double pan);
    void setTrackMuted(int track, bool muted);
    void setTrackSolo(int track, bool solo);
    int createInstrument(const std::string& name);
    int cloneInstrument(int sourceInstrument, const std::string& name);
    void renameInstrument(int instrument, const std::string& name);
    void setInstrumentWaveform(int instrument, const std::string& oscillator, Waveform waveform);
    void setInstrumentParameter(int instrument, const std::string& parameter, double value);
    void fillScale(
        int track,
        int startRow,
        int count,
        int stride,
        int rootMidi,
        const std::string& scaleName,
        int instrument,
        float velocity,
        double gateRows);
    void fillEuclidean(
        int track,
        int startRow,
        int steps,
        int pulses,
        int rootMidi,
        int instrument,
        float velocity,
        double gateRows);

    std::string applyCommand(const std::string& command);

private:
    Pattern& activePattern();
    const Pattern& activePattern() const;
    PatternStep& activeStep();
    void clampCursor();

    Song& song_;
    EditorCursor cursor_;
};

} // namespace arachno
