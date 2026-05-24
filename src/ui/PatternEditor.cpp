#include "PatternEditor.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>

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
    } else if (verb == "transpose") {
        const int semitones = readValue<int>(in, "semitones");
        std::string scope;
        in >> scope;
        if (scope == "track") {
            transposeTrack(cursor_.track, semitones);
        } else {
            transposeCurrent(semitones);
        }
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
