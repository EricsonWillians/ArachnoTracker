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

struct EditorSelection {
    int startRow = 0;
    int startTrack = 0;
    int rowCount = 1;
    int trackCount = 1;
};

struct EditorCommandResult {
    bool ok = false;
    std::string message;
    std::string error;
    bool projectChanged = false;
    bool editorStateChanged = false;
};

class PatternEditorSession {
public:
    explicit PatternEditorSession(Song& song);

    const EditorCursor& cursor() const { return cursor_; }
    const EditorSelection& selection() const { return selection_; }
    bool canUndo() const { return !undoStack_.empty(); }
    bool canRedo() const { return !redoStack_.empty(); }
    bool hasClipboard() const { return !clipboard_.empty(); }
    int clipboardRowCount() const { return static_cast<int>(clipboard_.size()); }
    int clipboardTrackCount() const { return clipboard_.empty() ? 0 : static_cast<int>(clipboard_.front().size()); }

    void selectPattern(int pattern);
    void moveTo(int row, int track);
    void moveBy(int rowDelta, int trackDelta);
    void selectRange(int startRow, int startTrack, int rowCount, int trackCount);
    void copySelection();
    void cutSelection();
    void pasteClipboard(int row = -1, int track = -1);
    void clearSelection();
    void undo();
    void redo();
    void enterNote(const Note& note, float velocity = 1.0f);
    void clearStep();
    void setInstrument(int instrument);
    void setGate(double gateRows);
    void setProbability(double probability);
    void clearProbability();
    void setRetrigger(int count, double spacingRows, double velocityDecay);
    void setAutomation(const std::string& parameter, double value);
    void clearAutomation(const std::string& parameter);
    void clearAllAutomation();
    void setStepEffect(const std::string& name, const std::string& parameter, double value);
    void clearStepEffect(const std::string& name);
    void clearAllStepEffects();
    void transposeCurrent(int semitones);
    void transposeTrack(int track, int semitones);
    void setSelectionOctave(int octave);
    void setSelectionVelocity(double velocity);
    void nudgeSelectionVelocity(double velocityDelta);
    void transposeSelection(int semitones);
    void repeatSelection(int repeats, int rowSpacing, int trackSpacing = 0);
    void setTempo(double bpm);
    void setRowsPerBeat(int rowsPerBeat);
    void setTitle(const std::string& title);
    void setAuthor(const std::string& author);
    void setDescription(const std::string& description);
    void setNotes(const std::string& notes);
    void renameActivePattern(const std::string& name);
    int createPattern(const std::string& name, int rows, int tracks = -1);
    int cloneActivePattern(const std::string& name);
    void deletePattern(int pattern);
    void appendOrder(int pattern = -1);
    void insertOrder(int index, int pattern = -1);
    void removeOrderEntry(int index);
    void setOrder(const std::vector<int>& order);
    int createTrack(const std::string& name);
    int duplicateTrack(int sourceTrack, const std::string& name);
    void deleteTrack(int track);
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
    EditorCommandResult tryApplyCommand(const std::string& command);

private:
    struct Snapshot {
        Song song;
        EditorCursor cursor;
        EditorSelection selection;
    };

    void rememberUndo(const Snapshot& snapshot);
    Snapshot snapshot() const;
    void restore(const Snapshot& snapshot);
    Pattern& activePattern();
    const Pattern& activePattern() const;
    PatternStep& activeStep();
    void clampCursor();
    void clampSelection();

    Song& song_;
    EditorCursor cursor_;
    EditorSelection selection_;
    std::vector<std::vector<PatternStep>> clipboard_;
    std::vector<Snapshot> undoStack_;
    std::vector<Snapshot> redoStack_;
};

} // namespace arachno
