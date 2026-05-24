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

std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
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
