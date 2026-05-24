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
}

void PatternEditorSession::moveTo(int row, int track) {
    cursor_.row = row;
    cursor_.track = track;
    clampCursor();
}

void PatternEditorSession::moveBy(int rowDelta, int trackDelta) {
    cursor_.row += rowDelta;
    cursor_.track += trackDelta;
    clampCursor();
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

void PatternEditorSession::appendOrder(int pattern) {
    const int patternToAppend = pattern >= 0 ? pattern : cursor_.pattern;
    checkPatternIndex(song_, patternToAppend);
    song_.order.push_back(patternToAppend);
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
    } else {
        throw std::invalid_argument("oscillator must be A or B");
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

    if (verb == "pattern") {
        selectPattern(readValue<int>(in, "pattern"));
    } else if (verb == "tempo") {
        setTempo(readValue<double>(in, "tempo"));
    } else if (verb == "rows-per-beat") {
        setRowsPerBeat(readValue<int>(in, "rows per beat"));
    } else if (verb == "pattern-name") {
        renameActivePattern(readValue<std::string>(in, "pattern name"));
    } else if (verb == "new-pattern") {
        const std::string name = readValue<std::string>(in, "pattern name");
        const int rows = readValue<int>(in, "pattern rows");
        const int tracks = readOptionalInt(in, -1);
        createPattern(name, rows, tracks);
    } else if (verb == "clone-pattern") {
        cloneActivePattern(readOptionalString(in, ""));
    } else if (verb == "append-order") {
        appendOrder(readOptionalInt(in, -1));
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

    std::ostringstream out;
    out << "pattern=" << cursor_.pattern
        << " row=" << cursor_.row
        << " track=" << cursor_.track;
    return out.str();
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

} // namespace arachno
