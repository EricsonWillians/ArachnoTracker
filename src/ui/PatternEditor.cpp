#include "PatternEditor.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace arachno {

namespace {
template <typename T>
T readValue(std::istringstream& in, const std::string& field) {
    T value {};
    if (!(in >> value)) {
        throw std::invalid_argument("missing " + field);
    }
    return value;
}

int readOptionalInt(std::istringstream& in, int fallback) {
    int value = fallback;
    in >> value;
    return value;
}

double readOptionalDouble(std::istringstream& in, double fallback) {
    double value = fallback;
    in >> value;
    return value;
}

std::string readOptionalString(std::istringstream& in, const std::string& fallback) {
    std::string value;
    in >> value;
    return value.empty() ? fallback : value;
}

std::string readRemainder(std::istringstream& in, const std::string& field) {
    std::string value;
    std::getline(in, value);
    const std::size_t first = value.find_first_not_of(" \t");
    if (first == std::string::npos) {
        throw std::invalid_argument("missing " + field);
    }
    return value.substr(first);
}

std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool parseBoolToken(const std::string& value) {
    const std::string normalized = lowerCopy(value);
    if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on") {
        return true;
    }
    if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off") {
        return false;
    }
    throw std::invalid_argument("expected boolean value, got: " + value);
}

const std::vector<int>& scaleIntervals(const std::string& scaleName) {
    static const std::map<std::string, std::vector<int>> scales = {
        {"major", {0, 2, 4, 5, 7, 9, 11}},
        {"minor", {0, 2, 3, 5, 7, 8, 10}},
        {"dorian", {0, 2, 3, 5, 7, 9, 10}},
        {"phrygian", {0, 1, 3, 5, 7, 8, 10}},
        {"lydian", {0, 2, 4, 6, 7, 9, 11}},
        {"mixolydian", {0, 2, 4, 5, 7, 9, 10}},
        {"locrian", {0, 1, 3, 5, 6, 8, 10}},
        {"pentatonic", {0, 2, 4, 7, 9}},
        {"minor-pentatonic", {0, 3, 5, 7, 10}},
        {"chromatic", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
    };

    const auto it = scales.find(lowerCopy(scaleName));
    if (it == scales.end()) {
        throw std::invalid_argument("unknown scale: " + scaleName);
    }
    return it->second;
}

int scaleDegreeToMidi(int rootMidi, const std::vector<int>& intervals, int degree) {
    const int size = static_cast<int>(intervals.size());
    const int octave = degree >= 0 ? degree / size : (degree - size + 1) / size;
    const int index = ((degree % size) + size) % size;
    return rootMidi + octave * 12 + intervals[static_cast<std::size_t>(index)];
}

bool euclideanHit(int step, int pulses, int steps) {
    return ((step * pulses) % steps) < pulses;
}

void checkPatternIndex(const Song& song, int pattern) {
    if (pattern < 0 || pattern >= static_cast<int>(song.patterns.size())) {
        throw std::out_of_range("pattern index is out of range");
    }
}

void checkTrackIndex(const Song& song, int track) {
    if (track < 0 || track >= static_cast<int>(song.tracks.size())) {
        throw std::out_of_range("track index is out of range");
    }
}

void checkInstrumentIndex(const Song& song, int instrument) {
    if (instrument < 0 || instrument >= static_cast<int>(song.instruments.size())) {
        throw std::out_of_range("instrument index is out of range");
    }
}

bool commandMutatesProject(const std::string& verb) {
    return verb != "pattern"
        && verb != "move"
        && verb != "up"
        && verb != "down"
        && verb != "left"
        && verb != "right"
        && verb != "select"
        && verb != "copy"
        && verb != "undo"
        && verb != "redo";
}

std::string commandVerb(const std::string& command) {
    std::istringstream in(command);
    std::string verb;
    in >> verb;
    return verb;
}
} // namespace

PatternEditorSession::PatternEditorSession(Song& song) : song_(song) {
    if (song_.patterns.empty()) {
        throw std::invalid_argument("editor requires at least one pattern");
    }
    clampCursor();
}

void PatternEditorSession::selectPattern(int pattern) {
    if (pattern < 0 || pattern >= static_cast<int>(song_.patterns.size())) {
        throw std::out_of_range("pattern selection is out of range");
    }
    cursor_.pattern = pattern;
    clampCursor();
    clampSelection();
}

void PatternEditorSession::moveTo(int row, int track) {
    cursor_.row = row;
    cursor_.track = track;
    clampCursor();
    selectRange(cursor_.row, cursor_.track, 1, 1);
}

void PatternEditorSession::moveBy(int rowDelta, int trackDelta) {
    cursor_.row += rowDelta;
    cursor_.track += trackDelta;
    clampCursor();
    selectRange(cursor_.row, cursor_.track, 1, 1);
}

void PatternEditorSession::selectRange(int startRow, int startTrack, int rowCount, int trackCount) {
    if (rowCount <= 0 || trackCount <= 0) {
        throw std::invalid_argument("selection dimensions must be positive");
    }

    selection_.startRow = startRow;
    selection_.startTrack = startTrack;
    selection_.rowCount = rowCount;
    selection_.trackCount = trackCount;
    clampSelection();
}

void PatternEditorSession::copySelection() {
    const Pattern& pattern = activePattern();
    clipboard_.clear();
    clipboard_.reserve(static_cast<std::size_t>(selection_.rowCount));
    for (int rowOffset = 0; rowOffset < selection_.rowCount; ++rowOffset) {
        std::vector<PatternStep> row;
        row.reserve(static_cast<std::size_t>(selection_.trackCount));
        for (int trackOffset = 0; trackOffset < selection_.trackCount; ++trackOffset) {
            row.push_back(pattern.step(
                selection_.startRow + rowOffset,
                selection_.startTrack + trackOffset));
        }
        clipboard_.push_back(row);
    }
}

void PatternEditorSession::cutSelection() {
    copySelection();
    clearSelection();
}

void PatternEditorSession::pasteClipboard(int row, int track) {
    if (clipboard_.empty()) {
        throw std::invalid_argument("clipboard is empty");
    }

    Pattern& pattern = activePattern();
    const int targetRow = row >= 0 ? row : cursor_.row;
    const int targetTrack = track >= 0 ? track : cursor_.track;
    for (std::size_t rowOffset = 0; rowOffset < clipboard_.size(); ++rowOffset) {
        const int destinationRow = targetRow + static_cast<int>(rowOffset);
        if (destinationRow < 0 || destinationRow >= pattern.rowCount()) {
            continue;
        }

        for (std::size_t trackOffset = 0; trackOffset < clipboard_[rowOffset].size(); ++trackOffset) {
            const int destinationTrack = targetTrack + static_cast<int>(trackOffset);
            if (destinationTrack < 0 || destinationTrack >= pattern.trackCount()) {
                continue;
            }
            pattern.step(destinationRow, destinationTrack) = clipboard_[rowOffset][trackOffset];
        }
    }
}

void PatternEditorSession::clearSelection() {
    Pattern& pattern = activePattern();
    for (int rowOffset = 0; rowOffset < selection_.rowCount; ++rowOffset) {
        for (int trackOffset = 0; trackOffset < selection_.trackCount; ++trackOffset) {
            pattern.step(
                selection_.startRow + rowOffset,
                selection_.startTrack + trackOffset) = PatternStep {};
        }
    }
}

void PatternEditorSession::undo() {
    if (undoStack_.empty()) {
        return;
    }

    redoStack_.push_back(snapshot());
    restore(undoStack_.back());
    undoStack_.pop_back();
}

void PatternEditorSession::redo() {
    if (redoStack_.empty()) {
        return;
    }

    undoStack_.push_back(snapshot());
    restore(redoStack_.back());
    redoStack_.pop_back();
}

void PatternEditorSession::enterNote(const Note& note, float velocity) {
    PatternStep& step = activeStep();
    step.note = Note(note.midi, velocity);
    if (step.instrument < 0 && !song_.instruments.empty()) {
        step.instrument = 0;
    }
}

void PatternEditorSession::clearStep() {
    activeStep() = PatternStep {};
}

void PatternEditorSession::setInstrument(int instrument) {
    if (instrument < 0 || instrument >= static_cast<int>(song_.instruments.size())) {
        throw std::out_of_range("instrument is out of range");
    }
    activeStep().instrument = instrument;
}

void PatternEditorSession::setGate(double gateRows) {
    if (gateRows <= 0.0) {
        throw std::invalid_argument("gate must be positive");
    }
    activeStep().gate = gateRows;
}

void PatternEditorSession::setProbability(double probability) {
    if (probability < 0.0 || probability > 1.0) {
        throw std::invalid_argument("probability must be between 0 and 1");
    }
    activeStep().probability = probability;
}

void PatternEditorSession::clearProbability() {
    activeStep().probability.reset();
}

void PatternEditorSession::setRetrigger(int count, double spacingRows, double velocityDecay) {
    if (count <= 0) {
        throw std::invalid_argument("retrigger count must be positive");
    }
    if (spacingRows <= 0.0) {
        throw std::invalid_argument("retrigger spacing must be positive");
    }
    if (velocityDecay < 0.0 || velocityDecay > 1.0) {
        throw std::invalid_argument("retrigger velocity decay must be between 0 and 1");
    }

    PatternStep& step = activeStep();
    step.retriggerCount = count;
    step.retriggerSpacingRows = spacingRows;
    step.retriggerVelocityDecay = velocityDecay;
}

void PatternEditorSession::setAutomation(const std::string& parameter, double value) {
    SynthPatch validationPatch;
    if (!setSynthPatchParameter(validationPatch, parameter, value)) {
        throw std::invalid_argument("unknown synth parameter: " + parameter);
    }
    activeStep().automation[parameter] = value;
}

void PatternEditorSession::clearAutomation(const std::string& parameter) {
    activeStep().automation.erase(parameter);
}

void PatternEditorSession::clearAllAutomation() {
    activeStep().automation.clear();
}

void PatternEditorSession::setStepEffect(const std::string& name, const std::string& parameter, double value) {
    if (name.empty()) {
        throw std::invalid_argument("effect name must not be empty");
    }
    if (parameter.empty()) {
        throw std::invalid_argument("effect parameter must not be empty");
    }
    PatternStep& step = activeStep();
    auto effectIt = std::find_if(step.effects.begin(), step.effects.end(), [&name](const EffectCommand& effect) {
        return lowerCopy(effect.name) == lowerCopy(name);
    });
    if (effectIt == step.effects.end()) {
        EffectCommand effect;
        effect.name = name;
        effect.parameters[parameter] = value;
        step.effects.push_back(effect);
    } else {
        effectIt->parameters[parameter] = value;
    }
}

void PatternEditorSession::clearStepEffect(const std::string& name) {
    PatternStep& step = activeStep();
    const std::string normalized = lowerCopy(name);
    step.effects.erase(
        std::remove_if(step.effects.begin(), step.effects.end(), [&normalized](const EffectCommand& effect) {
            return lowerCopy(effect.name) == normalized;
        }),
        step.effects.end());
}

void PatternEditorSession::clearAllStepEffects() {
    activeStep().effects.clear();
}

void PatternEditorSession::transposeCurrent(int semitones) {
    PatternStep& step = activeStep();
    if (step.note.has_value()) {
        step.note->midi = std::clamp(step.note->midi + semitones, 0, 127);
    }
}

void PatternEditorSession::transposeTrack(int track, int semitones) {
    Pattern& pattern = activePattern();
    if (track < 0 || track >= pattern.trackCount()) {
        throw std::out_of_range("track is out of range");
    }

    for (PatternRow& row : pattern.rows()) {
        PatternStep& step = row.steps[static_cast<std::size_t>(track)];
        if (step.note.has_value()) {
            step.note->midi = std::clamp(step.note->midi + semitones, 0, 127);
        }
    }
}

void PatternEditorSession::setTempo(double bpm) {
    if (bpm <= 0.0) {
        throw std::invalid_argument("tempo must be positive");
    }
    song_.bpm = bpm;
}

void PatternEditorSession::setRowsPerBeat(int rowsPerBeat) {
    if (rowsPerBeat <= 0) {
        throw std::invalid_argument("rows per beat must be positive");
    }
    song_.rowsPerBeat = rowsPerBeat;
}

void PatternEditorSession::setTitle(const std::string& title) {
    if (title.empty()) {
        throw std::invalid_argument("title must not be empty");
    }
    song_.title = title;
}

void PatternEditorSession::setAuthor(const std::string& author) {
    song_.author = author;
}

void PatternEditorSession::setDescription(const std::string& description) {
    song_.description = description;
}

void PatternEditorSession::setNotes(const std::string& notes) {
    song_.notes = notes;
}

void PatternEditorSession::renameActivePattern(const std::string& name) {
    if (name.empty()) {
        throw std::invalid_argument("pattern name must not be empty");
    }
    activePattern().setName(name);
}

int PatternEditorSession::createPattern(const std::string& name, int rows, int tracks) {
    if (name.empty()) {
        throw std::invalid_argument("pattern name must not be empty");
    }
    if (rows <= 0) {
        throw std::invalid_argument("pattern rows must be positive");
    }

    const int trackCount = tracks > 0 ? tracks : std::max(1, static_cast<int>(song_.tracks.size()));
    song_.patterns.emplace_back(name, rows, trackCount);
    cursor_.pattern = static_cast<int>(song_.patterns.size() - 1);
    clampCursor();
    return cursor_.pattern;
}

int PatternEditorSession::cloneActivePattern(const std::string& name) {
    Pattern clone = activePattern();
    clone.setName(name.empty() ? activePattern().name() + "_copy" : name);
    song_.patterns.push_back(clone);
    cursor_.pattern = static_cast<int>(song_.patterns.size() - 1);
    clampCursor();
    return cursor_.pattern;
}

void PatternEditorSession::deletePattern(int pattern) {
    checkPatternIndex(song_, pattern);
    if (song_.patterns.size() <= 1) {
        throw std::invalid_argument("project must keep at least one pattern");
    }

    song_.patterns.erase(song_.patterns.begin() + pattern);
    std::vector<int> remappedOrder;
    remappedOrder.reserve(song_.order.size());
    for (int patternIndex : song_.order) {
        if (patternIndex == pattern) {
            continue;
        }
        remappedOrder.push_back(patternIndex > pattern ? patternIndex - 1 : patternIndex);
    }
    if (remappedOrder.empty()) {
        remappedOrder.push_back(0);
    }
    song_.order = remappedOrder;
    cursor_.pattern = std::clamp(cursor_.pattern, 0, static_cast<int>(song_.patterns.size() - 1));
    clampCursor();
}

void PatternEditorSession::appendOrder(int pattern) {
    const int patternToAppend = pattern >= 0 ? pattern : cursor_.pattern;
    checkPatternIndex(song_, patternToAppend);
    song_.order.push_back(patternToAppend);
}

void PatternEditorSession::insertOrder(int index, int pattern) {
    const int patternToInsert = pattern >= 0 ? pattern : cursor_.pattern;
    checkPatternIndex(song_, patternToInsert);
    if (index < 0 || index > static_cast<int>(song_.order.size())) {
        throw std::out_of_range("order insert index is out of range");
    }
    song_.order.insert(song_.order.begin() + index, patternToInsert);
}

void PatternEditorSession::removeOrderEntry(int index) {
    if (song_.order.size() <= 1) {
        throw std::invalid_argument("arrangement order must keep at least one entry");
    }
    if (index < 0 || index >= static_cast<int>(song_.order.size())) {
        throw std::out_of_range("order index is out of range");
    }
    song_.order.erase(song_.order.begin() + index);
}

void PatternEditorSession::setOrder(const std::vector<int>& order) {
    if (order.empty()) {
        throw std::invalid_argument("order list must not be empty");
    }
    for (int pattern : order) {
        checkPatternIndex(song_, pattern);
    }
    song_.order = order;
}

int PatternEditorSession::createTrack(const std::string& name) {
    if (name.empty()) {
        throw std::invalid_argument("track name must not be empty");
    }

    Track track;
    track.name = name;
    song_.tracks.push_back(track);
    const int trackIndex = static_cast<int>(song_.tracks.size() - 1);

    for (Pattern& pattern : song_.patterns) {
        pattern.resizeTracks(static_cast<int>(song_.tracks.size()));
    }

    cursor_.track = trackIndex;
    clampCursor();
    return trackIndex;
}

int PatternEditorSession::duplicateTrack(int sourceTrack, const std::string& name) {
    checkTrackIndex(song_, sourceTrack);

    Track track = song_.tracks[static_cast<std::size_t>(sourceTrack)];
    track.name = name.empty() ? track.name + "_copy" : name;
    song_.tracks.push_back(track);
    const int destinationTrack = static_cast<int>(song_.tracks.size() - 1);

    for (Pattern& pattern : song_.patterns) {
        const int oldTrackCount = pattern.trackCount();
        pattern.resizeTracks(static_cast<int>(song_.tracks.size()));
        if (sourceTrack < oldTrackCount) {
            for (PatternRow& row : pattern.rows()) {
                row.steps[static_cast<std::size_t>(destinationTrack)] =
                    row.steps[static_cast<std::size_t>(sourceTrack)];
            }
        }
    }

    cursor_.track = destinationTrack;
    clampCursor();
    return destinationTrack;
}

void PatternEditorSession::deleteTrack(int track) {
    checkTrackIndex(song_, track);
    if (song_.tracks.size() <= 1) {
        throw std::invalid_argument("project must keep at least one track");
    }

    song_.tracks.erase(song_.tracks.begin() + track);
    for (Pattern& pattern : song_.patterns) {
        if (track < pattern.trackCount()) {
            pattern.removeTrack(track);
        }
    }
    cursor_.track = std::clamp(cursor_.track, 0, static_cast<int>(song_.tracks.size() - 1));
    clampCursor();
}

void PatternEditorSession::renameTrack(int track, const std::string& name) {
    checkTrackIndex(song_, track);
    if (name.empty()) {
        throw std::invalid_argument("track name must not be empty");
    }
    song_.tracks[static_cast<std::size_t>(track)].name = name;
}

void PatternEditorSession::clearTrack(int track) {
    checkTrackIndex(song_, track);
    for (Pattern& pattern : song_.patterns) {
        if (track >= pattern.trackCount()) {
            continue;
        }
        for (PatternRow& row : pattern.rows()) {
            row.steps[static_cast<std::size_t>(track)] = PatternStep {};
        }
    }
}

void PatternEditorSession::resizeActivePattern(int rows) {
    activePattern().resizeRows(rows);
    clampCursor();
}

void PatternEditorSession::setTrackVolume(int track, double volume) {
    checkTrackIndex(song_, track);
    song_.tracks[static_cast<std::size_t>(track)].volume = std::clamp(volume, 0.0, 2.0);
}

void PatternEditorSession::setTrackPan(int track, double pan) {
    checkTrackIndex(song_, track);
    song_.tracks[static_cast<std::size_t>(track)].pan = std::clamp(pan, -1.0, 1.0);
}

void PatternEditorSession::setTrackMuted(int track, bool muted) {
    checkTrackIndex(song_, track);
    song_.tracks[static_cast<std::size_t>(track)].muted = muted;
}

void PatternEditorSession::setTrackSolo(int track, bool solo) {
    checkTrackIndex(song_, track);
    song_.tracks[static_cast<std::size_t>(track)].solo = solo;
}

int PatternEditorSession::createInstrument(const std::string& name) {
    if (name.empty()) {
        throw std::invalid_argument("instrument name must not be empty");
    }

    Instrument instrument;
    instrument.id = static_cast<int>(song_.instruments.size());
    instrument.patch.name = name;
    song_.instruments.push_back(instrument);
    return instrument.id;
}

int PatternEditorSession::cloneInstrument(int sourceInstrument, const std::string& name) {
    checkInstrumentIndex(song_, sourceInstrument);

    Instrument instrument = song_.instruments[static_cast<std::size_t>(sourceInstrument)];
    instrument.id = static_cast<int>(song_.instruments.size());
    instrument.patch.name = name.empty()
        ? instrument.patch.name + "_copy"
        : name;
    song_.instruments.push_back(instrument);
    return instrument.id;
}

void PatternEditorSession::renameInstrument(int instrument, const std::string& name) {
    checkInstrumentIndex(song_, instrument);
    if (name.empty()) {
        throw std::invalid_argument("instrument name must not be empty");
    }
    song_.instruments[static_cast<std::size_t>(instrument)].patch.name = name;
}

void PatternEditorSession::setInstrumentWaveform(int instrument, const std::string& oscillator, Waveform waveform) {
    checkInstrumentIndex(song_, instrument);
    SynthPatch& patch = song_.instruments[static_cast<std::size_t>(instrument)].patch;
    const std::string normalized = lowerCopy(oscillator);
    if (normalized == "a" || normalized == "osc_a" || normalized == "oscillator_a") {
        patch.oscillatorA = waveform;
    } else if (normalized == "b" || normalized == "osc_b" || normalized == "oscillator_b") {
        patch.oscillatorB = waveform;
    } else if (normalized == "c" || normalized == "osc_c" || normalized == "oscillator_c") {
        patch.oscillatorC = waveform;
    } else if (normalized == "d" || normalized == "osc_d" || normalized == "oscillator_d") {
        patch.oscillatorD = waveform;
    } else {
        throw std::invalid_argument("oscillator must be A, B, C, or D");
    }
}

void PatternEditorSession::setInstrumentParameter(int instrument, const std::string& parameter, double value) {
    checkInstrumentIndex(song_, instrument);
    SynthPatch& patch = song_.instruments[static_cast<std::size_t>(instrument)].patch;
    if (!setSynthPatchParameter(patch, parameter, value)) {
        throw std::invalid_argument("unknown synth parameter: " + parameter);
    }
}

void PatternEditorSession::fillScale(
    int track,
    int startRow,
    int count,
    int stride,
    int rootMidi,
    const std::string& scaleName,
    int instrument,
    float velocity,
    double gateRows) {
    if (count <= 0) {
        throw std::invalid_argument("scale fill count must be positive");
    }
    if (stride <= 0) {
        throw std::invalid_argument("scale fill stride must be positive");
    }
    if (instrument < 0 || instrument >= static_cast<int>(song_.instruments.size())) {
        throw std::out_of_range("instrument is out of range");
    }
    if (gateRows <= 0.0) {
        throw std::invalid_argument("gate must be positive");
    }

    Pattern& pattern = activePattern();
    if (track < 0 || track >= pattern.trackCount()) {
        throw std::out_of_range("track is out of range");
    }

    const std::vector<int>& intervals = scaleIntervals(scaleName);
    for (int index = 0; index < count; ++index) {
        const int row = startRow + index * stride;
        if (row < 0 || row >= pattern.rowCount()) {
            continue;
        }

        PatternStep& step = pattern.step(row, track);
        step.note = Note(std::clamp(scaleDegreeToMidi(rootMidi, intervals, index), 0, 127), velocity);
        step.instrument = instrument;
        step.gate = gateRows;
    }
}

void PatternEditorSession::fillEuclidean(
    int track,
    int startRow,
    int steps,
    int pulses,
    int rootMidi,
    int instrument,
    float velocity,
    double gateRows) {
    if (steps <= 0) {
        throw std::invalid_argument("euclidean steps must be positive");
    }
    if (pulses <= 0 || pulses > steps) {
        throw std::invalid_argument("euclidean pulses must be between 1 and steps");
    }
    if (instrument < 0 || instrument >= static_cast<int>(song_.instruments.size())) {
        throw std::out_of_range("instrument is out of range");
    }
    if (gateRows <= 0.0) {
        throw std::invalid_argument("gate must be positive");
    }

    Pattern& pattern = activePattern();
    if (track < 0 || track >= pattern.trackCount()) {
        throw std::out_of_range("track is out of range");
    }

    for (int stepIndex = 0; stepIndex < steps; ++stepIndex) {
        const int row = startRow + stepIndex;
        if (row < 0 || row >= pattern.rowCount()) {
            continue;
        }
        if (!euclideanHit(stepIndex, pulses, steps)) {
            continue;
        }

        PatternStep& step = pattern.step(row, track);
        step.note = Note(rootMidi, velocity);
        step.instrument = instrument;
        step.gate = gateRows;
    }
}

std::string PatternEditorSession::applyCommand(const std::string& command) {
    std::istringstream in(command);
    std::string verb;
    in >> verb;
    if (verb.empty() || verb == "#") {
        return "noop";
    }

    const bool mutatesProject = commandMutatesProject(verb);
    const Snapshot before = snapshot();

    try {
        if (verb == "pattern") {
            selectPattern(readValue<int>(in, "pattern"));
        } else if (verb == "undo") {
            undo();
        } else if (verb == "redo") {
            redo();
        } else if (verb == "select") {
            const int row = readValue<int>(in, "selection row");
            const int track = readValue<int>(in, "selection track");
            const int rows = readValue<int>(in, "selection rows");
            const int tracks = readValue<int>(in, "selection tracks");
            selectRange(row, track, rows, tracks);
        } else if (verb == "copy") {
            copySelection();
        } else if (verb == "cut") {
            cutSelection();
        } else if (verb == "paste") {
            const int row = readOptionalInt(in, -1);
            const int track = readOptionalInt(in, -1);
            pasteClipboard(row, track);
        } else if (verb == "clear-selection") {
            clearSelection();
        } else if (verb == "tempo") {
        setTempo(readValue<double>(in, "tempo"));
    } else if (verb == "rows-per-beat") {
        setRowsPerBeat(readValue<int>(in, "rows per beat"));
    } else if (verb == "title") {
        setTitle(readRemainder(in, "title"));
    } else if (verb == "author") {
        setAuthor(readRemainder(in, "author"));
    } else if (verb == "description") {
        setDescription(readRemainder(in, "description"));
    } else if (verb == "notes") {
        setNotes(readRemainder(in, "notes"));
    } else if (verb == "pattern-name") {
        renameActivePattern(readValue<std::string>(in, "pattern name"));
    } else if (verb == "new-pattern") {
        const std::string name = readValue<std::string>(in, "pattern name");
        const int rows = readValue<int>(in, "pattern rows");
        const int tracks = readOptionalInt(in, -1);
        createPattern(name, rows, tracks);
    } else if (verb == "clone-pattern") {
        cloneActivePattern(readOptionalString(in, ""));
    } else if (verb == "delete-pattern") {
        deletePattern(readOptionalInt(in, cursor_.pattern));
    } else if (verb == "append-order") {
        appendOrder(readOptionalInt(in, -1));
    } else if (verb == "insert-order") {
        const int index = readValue<int>(in, "order index");
        insertOrder(index, readOptionalInt(in, -1));
    } else if (verb == "remove-order") {
        removeOrderEntry(readValue<int>(in, "order index"));
    } else if (verb == "set-order") {
        std::vector<int> order;
        int pattern = 0;
        while (in >> pattern) {
            order.push_back(pattern);
        }
        setOrder(order);
    } else if (verb == "new-track") {
        createTrack(readValue<std::string>(in, "track name"));
    } else if (verb == "duplicate-track") {
        const int sourceTrack = readValue<int>(in, "source track");
        duplicateTrack(sourceTrack, readOptionalString(in, ""));
    } else if (verb == "delete-track") {
        deleteTrack(readValue<int>(in, "track"));
    } else if (verb == "track-name") {
        const int track = readValue<int>(in, "track");
        renameTrack(track, readValue<std::string>(in, "track name"));
    } else if (verb == "clear-track") {
        clearTrack(readValue<int>(in, "track"));
    } else if (verb == "resize-pattern") {
        resizeActivePattern(readValue<int>(in, "rows"));
    } else if (verb == "track-volume") {
        const int track = readValue<int>(in, "track");
        const double volume = readValue<double>(in, "volume");
        setTrackVolume(track, volume);
    } else if (verb == "track-pan") {
        const int track = readValue<int>(in, "track");
        const double pan = readValue<double>(in, "pan");
        setTrackPan(track, pan);
    } else if (verb == "track-mute") {
        const int track = readValue<int>(in, "track");
        setTrackMuted(track, parseBoolToken(readValue<std::string>(in, "muted")));
    } else if (verb == "track-solo") {
        const int track = readValue<int>(in, "track");
        setTrackSolo(track, parseBoolToken(readValue<std::string>(in, "solo")));
    } else if (verb == "new-instrument") {
        createInstrument(readValue<std::string>(in, "instrument name"));
    } else if (verb == "clone-instrument") {
        const int source = readValue<int>(in, "source instrument");
        cloneInstrument(source, readOptionalString(in, ""));
    } else if (verb == "instrument-name") {
        const int instrument = readValue<int>(in, "instrument");
        renameInstrument(instrument, readValue<std::string>(in, "instrument name"));
    } else if (verb == "instrument-wave") {
        const int instrument = readValue<int>(in, "instrument");
        const std::string oscillator = readValue<std::string>(in, "oscillator");
        const std::string waveform = readValue<std::string>(in, "waveform");
        setInstrumentWaveform(instrument, oscillator, waveformFromName(waveform));
    } else if (verb == "instrument-param") {
        const int instrument = readValue<int>(in, "instrument");
        const std::string parameter = readValue<std::string>(in, "parameter");
        const double value = readValue<double>(in, "value");
        setInstrumentParameter(instrument, parameter, value);
    } else if (verb == "move") {
        const int row = readValue<int>(in, "row");
        const int track = readValue<int>(in, "track");
        moveTo(row, track);
    } else if (verb == "up") {
        moveBy(-readOptionalInt(in, 1), 0);
    } else if (verb == "down") {
        moveBy(readOptionalInt(in, 1), 0);
    } else if (verb == "left") {
        moveBy(0, -readOptionalInt(in, 1));
    } else if (verb == "right") {
        moveBy(0, readOptionalInt(in, 1));
    } else if (verb == "note") {
        std::string noteName;
        in >> noteName;
        if (noteName.empty()) {
            throw std::invalid_argument("note command requires a note name");
        }
        const double velocity = readOptionalDouble(in, 1.0);
        enterNote(Note(noteNameToMidi(noteName), static_cast<float>(velocity)), static_cast<float>(velocity));
    } else if (verb == "clear" || verb == "rest") {
        clearStep();
    } else if (verb == "instrument" || verb == "inst") {
        setInstrument(readValue<int>(in, "instrument"));
    } else if (verb == "gate") {
        setGate(readValue<double>(in, "gate"));
    } else if (verb == "probability" || verb == "prob") {
        std::string value;
        in >> value;
        if (value.empty() || value == "clear" || value == "off") {
            clearProbability();
        } else {
            setProbability(std::stod(value));
        }
    } else if (verb == "retrig" || verb == "retrigger") {
        const int count = readValue<int>(in, "retrigger count");
        const double spacing = readOptionalDouble(in, 0.25);
        const double decay = readOptionalDouble(in, 0.85);
        setRetrigger(count, spacing, decay);
    } else if (verb == "param") {
        const std::string parameter = readValue<std::string>(in, "parameter");
        const double value = readValue<double>(in, "value");
        setAutomation(parameter, value);
    } else if (verb == "param-clear") {
        std::string parameter;
        in >> parameter;
        if (parameter.empty() || parameter == "*") {
            clearAllAutomation();
        } else {
            clearAutomation(parameter);
        }
    } else if (verb == "fx" || verb == "effect") {
        const std::string name = readValue<std::string>(in, "effect name");
        const double value = readValue<double>(in, "effect value");
        setStepEffect(name, "value", value);
    } else if (verb == "fxp" || verb == "effect-param") {
        const std::string name = readValue<std::string>(in, "effect name");
        const std::string parameter = readValue<std::string>(in, "effect parameter");
        const double value = readValue<double>(in, "effect value");
        setStepEffect(name, parameter, value);
    } else if (verb == "fx-clear" || verb == "effect-clear") {
        std::string name;
        in >> name;
        if (name.empty() || name == "*") {
            clearAllStepEffects();
        } else {
            clearStepEffect(name);
        }
    } else if (verb == "transpose") {
        const int semitones = readValue<int>(in, "semitones");
        std::string scope;
        in >> scope;
        if (scope == "track") {
            transposeTrack(cursor_.track, semitones);
        } else {
            transposeCurrent(semitones);
        }
    } else if (verb == "fill-scale") {
        const int track = readValue<int>(in, "track");
        const int startRow = readValue<int>(in, "start row");
        const int count = readValue<int>(in, "count");
        const int stride = readValue<int>(in, "stride");
        const std::string root = readValue<std::string>(in, "root note");
        const std::string scale = readValue<std::string>(in, "scale");
        const int instrument = readValue<int>(in, "instrument");
        const double velocity = readOptionalDouble(in, 0.8);
        const double gate = readOptionalDouble(in, 0.8);
        fillScale(
            track,
            startRow,
            count,
            stride,
            noteNameToMidi(root),
            scale,
            instrument,
            static_cast<float>(velocity),
            gate);
    } else if (verb == "euclid") {
        const int track = readValue<int>(in, "track");
        const int startRow = readValue<int>(in, "start row");
        const int steps = readValue<int>(in, "steps");
        const int pulses = readValue<int>(in, "pulses");
        const std::string root = readValue<std::string>(in, "root note");
        const int instrument = readValue<int>(in, "instrument");
        const double velocity = readOptionalDouble(in, 0.8);
        const double gate = readOptionalDouble(in, 0.6);
        fillEuclidean(
            track,
            startRow,
            steps,
            pulses,
            noteNameToMidi(root),
            instrument,
            static_cast<float>(velocity),
            gate);
    } else {
        throw std::invalid_argument("unknown editor command: " + verb);
    }
    } catch (...) {
        if (mutatesProject) {
            restore(before);
        }
        throw;
    }

    if (mutatesProject) {
        rememberUndo(before);
        redoStack_.clear();
    }

    std::ostringstream out;
    out << "pattern=" << cursor_.pattern
        << " row=" << cursor_.row
        << " track=" << cursor_.track;
    return out.str();
}

EditorCommandResult PatternEditorSession::tryApplyCommand(const std::string& command) {
    const std::string verb = commandVerb(command);
    const bool canUndoBefore = canUndo();
    const bool canRedoBefore = canRedo();
    const bool mayChangeProject = commandMutatesProject(verb)
        || (verb == "undo" && canUndoBefore)
        || (verb == "redo" && canRedoBefore);

    EditorCommandResult result;
    try {
        result.message = applyCommand(command);
        result.ok = true;
        result.projectChanged = mayChangeProject && result.message != "noop";
        result.editorStateChanged = result.message != "noop";
    } catch (const std::exception& error) {
        result.ok = false;
        result.error = error.what();
    } catch (...) {
        result.ok = false;
        result.error = "unknown editor command error";
    }
    return result;
}

Pattern& PatternEditorSession::activePattern() {
    return song_.patterns[static_cast<std::size_t>(cursor_.pattern)];
}

const Pattern& PatternEditorSession::activePattern() const {
    return song_.patterns[static_cast<std::size_t>(cursor_.pattern)];
}

PatternStep& PatternEditorSession::activeStep() {
    return activePattern().step(cursor_.row, cursor_.track);
}

void PatternEditorSession::clampCursor() {
    const Pattern& pattern = activePattern();
    cursor_.row = std::clamp(cursor_.row, 0, pattern.rowCount() - 1);
    cursor_.track = std::clamp(cursor_.track, 0, pattern.trackCount() - 1);
}

void PatternEditorSession::rememberUndo(const Snapshot& snapshot) {
    constexpr std::size_t maxUndoDepth = 128;
    undoStack_.push_back(snapshot);
    if (undoStack_.size() > maxUndoDepth) {
        undoStack_.erase(undoStack_.begin());
    }
}

PatternEditorSession::Snapshot PatternEditorSession::snapshot() const {
    return Snapshot {song_, cursor_, selection_};
}

void PatternEditorSession::restore(const Snapshot& snapshot) {
    song_ = snapshot.song;
    cursor_ = snapshot.cursor;
    selection_ = snapshot.selection;
    clampCursor();
    clampSelection();
}

void PatternEditorSession::clampSelection() {
    const Pattern& pattern = activePattern();
    selection_.startRow = std::clamp(selection_.startRow, 0, pattern.rowCount() - 1);
    selection_.startTrack = std::clamp(selection_.startTrack, 0, pattern.trackCount() - 1);
    selection_.rowCount = std::max(1, std::min(selection_.rowCount, pattern.rowCount() - selection_.startRow));
    selection_.trackCount = std::max(1, std::min(selection_.trackCount, pattern.trackCount() - selection_.startTrack));
}

} // namespace arachno
