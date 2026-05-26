#include "PatternView.h"

#include <algorithm>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "PatternEditor.h"

namespace arachno {

namespace {
std::string formatStep(const PatternStep& step) {
    if (!step.note.has_value()) {
        return ".... .. ..";
    }

    std::ostringstream out;
    std::string automationMarker = (step.automation.empty() && step.effects.empty()) ? " " : "*";
    if (step.retriggerCount > 1) {
        automationMarker = "r";
    } else if (step.probability.has_value() && step.probability.value() < 1.0) {
        automationMarker = "?";
    } else if (!step.effects.empty() && step.automation.empty()) {
        automationMarker = "e";
    }
    out << std::left << std::setw(4) << step.note->name()
        << " i" << std::setw(2) << step.instrument
        << " v" << std::setw(3) << static_cast<int>(step.note->velocity * 100.0f)
        << automationMarker;
    return out.str();
}

std::string trackNameFor(const Song& song, int track) {
    if (track >= 0 && track < static_cast<int>(song.tracks.size())) {
        return song.tracks[static_cast<std::size_t>(track)].name;
    }
    return "T" + std::to_string(track);
}

bool selectionContains(const EditorSelection* selection, int row, int track) {
    if (selection == nullptr) {
        return false;
    }
    return row >= selection->startRow
        && row < selection->startRow + selection->rowCount
        && track >= selection->startTrack
        && track < selection->startTrack + selection->trackCount;
}
} // namespace

PatternGrid buildPatternGrid(
    const Song& song,
    int patternIndex,
    int startRow,
    int rowCount,
    const EditorCursor* cursor,
    const EditorSelection* selection) {
    if (patternIndex < 0 || patternIndex >= static_cast<int>(song.patterns.size())) {
        throw std::out_of_range("pattern index is out of range");
    }

    const Pattern& pattern = song.patterns[static_cast<std::size_t>(patternIndex)];
    startRow = std::clamp(startRow, 0, pattern.rowCount() - 1);
    const int endRow = rowCount < 0
        ? pattern.rowCount()
        : std::min(pattern.rowCount(), startRow + rowCount);

    PatternGrid grid;
    grid.patternIndex = patternIndex;
    grid.patternName = pattern.name();
    grid.startRow = startRow;
    grid.rowCount = endRow - startRow;
    grid.trackCount = pattern.trackCount();
    grid.trackNames.reserve(static_cast<std::size_t>(grid.trackCount));
    grid.cells.reserve(static_cast<std::size_t>(grid.rowCount * grid.trackCount));

    for (int track = 0; track < grid.trackCount; ++track) {
        grid.trackNames.push_back(trackNameFor(song, track));
    }

    for (int row = startRow; row < endRow; ++row) {
        for (int track = 0; track < pattern.trackCount(); ++track) {
            const PatternStep& step = pattern.step(row, track);
            PatternGridCell cell;
            cell.row = row;
            cell.track = track;
            cell.trackName = grid.trackNames[static_cast<std::size_t>(track)];
            cell.hasNote = step.note.has_value();
            if (step.note.has_value()) {
                cell.noteName = step.note->name();
                cell.midiNote = step.note->midi;
                cell.velocity = step.note->velocity;
            }
            cell.instrument = step.instrument;
            cell.gateRows = step.gate;
            cell.hasAutomation = !step.automation.empty();
            cell.hasEffects = !step.effects.empty();
            cell.selected = selectionContains(selection, row, track);
            cell.cursor = cursor != nullptr
                && cursor->pattern == patternIndex
                && cursor->row == row
                && cursor->track == track;
            grid.cells.push_back(cell);
        }
    }

    return grid;
}

std::string renderPatternTable(const Song& song, int patternIndex, int startRow, int rowCount) {
    if (patternIndex < 0 || patternIndex >= static_cast<int>(song.patterns.size())) {
        throw std::out_of_range("pattern index is out of range");
    }

    const Pattern& pattern = song.patterns[static_cast<std::size_t>(patternIndex)];
    startRow = std::clamp(startRow, 0, pattern.rowCount() - 1);
    const int endRow = rowCount < 0
        ? pattern.rowCount()
        : std::min(pattern.rowCount(), startRow + rowCount);

    std::ostringstream out;
    out << "Pattern " << patternIndex << " \"" << pattern.name() << "\"\n";
    out << "row ";
    for (int track = 0; track < pattern.trackCount(); ++track) {
        std::string name = "T" + std::to_string(track);
        if (track < static_cast<int>(song.tracks.size())) {
            name = song.tracks[static_cast<std::size_t>(track)].name;
        }
        out << "| " << std::left << std::setw(12) << name;
    }
    out << "\n";

    for (int row = startRow; row < endRow; ++row) {
        out << std::right << std::setw(3) << row << " ";
        for (int track = 0; track < pattern.trackCount(); ++track) {
            out << "| " << std::left << std::setw(12) << formatStep(pattern.step(row, track));
        }
        out << "\n";
    }

    return out.str();
}

std::string renderArrangementTable(const Song& song) {
    std::ostringstream out;
    out << "Arrangement \"" << song.title << "\"\n";
    if (!song.author.empty()) {
        out << "author " << song.author << "\n";
    }
    if (!song.description.empty()) {
        out << song.description << "\n";
    }
    out << "tempo " << song.bpm << " bpm, rows/beat " << song.rowsPerBeat << "\n";
    out << "idx | pattern | name         | rows | starts at row\n";

    int startRow = 0;
    for (std::size_t index = 0; index < song.order.size(); ++index) {
        const int patternIndex = song.order[index];
        out << std::right << std::setw(3) << index << " | "
            << std::setw(7) << patternIndex << " | ";

        if (patternIndex >= 0 && patternIndex < static_cast<int>(song.patterns.size())) {
            const Pattern& pattern = song.patterns[static_cast<std::size_t>(patternIndex)];
            out << std::left << std::setw(12) << pattern.name() << " | "
                << std::right << std::setw(4) << pattern.rowCount() << " | "
                << std::setw(13) << startRow << "\n";
            startRow += pattern.rowCount();
        } else {
            out << std::left << std::setw(12) << "<missing>" << " | "
                << std::right << std::setw(4) << 0 << " | "
                << std::setw(13) << startRow << "\n";
        }
    }

    out << "total rows " << startRow
        << ", duration " << song.durationSeconds() << " seconds\n";
    return out.str();
}

std::string renderInstrumentTable(const Song& song) {
    std::ostringstream out;
    out << "Instruments\n";
    out << "id | name         | osc A    | osc B    | osc C    | osc D    | pulse | fm   | chor | uni | spread | cutoff | pitch | ring | sync | crush | click | trn | nton | bct | hp   | drive | gain\n";

    for (const Instrument& instrument : song.instruments) {
        const SynthPatch& patch = instrument.patch;
        out << std::right << std::setw(2) << instrument.id << " | "
            << std::left << std::setw(12) << patch.name << " | "
            << std::setw(8) << waveformName(patch.oscillatorA) << " | "
            << std::setw(8) << waveformName(patch.oscillatorB) << " | "
            << std::setw(8) << waveformName(patch.oscillatorC) << " | "
            << std::setw(8) << waveformName(patch.oscillatorD) << " | "
            << std::right << std::setw(5) << patch.pulseWidth << " | "
            << std::setw(4) << patch.fmAmount << " | "
            << std::setw(4) << patch.chorusMix << " | "
            << std::right << std::setw(3) << patch.unisonVoices << " | "
            << std::setw(6) << patch.stereoSpread << " | "
            << std::setw(6) << patch.cutoff << " | "
            << std::setw(5) << patch.pitchEnvelopeSemitones << " | "
            << std::setw(4) << patch.ringMod << " | "
            << std::setw(4) << patch.hardSync << " | "
            << std::setw(5) << patch.bitCrush << " | "
            << std::setw(5) << patch.click << " | "
            << std::setw(3) << patch.transientNoise << " | "
            << std::setw(4) << patch.noiseTone << " | "
            << std::setw(3) << patch.transientBurstCount << " | "
            << std::setw(4) << patch.highPass << " | "
            << std::setw(5) << patch.drive << " | "
            << std::setw(4) << patch.gain << "\n";
    }

    return out.str();
}

std::string renderProjectStats(const Song& song) {
    std::vector<int> notesByTrack(song.tracks.size(), 0);
    std::map<int, int> notesByInstrument;
    int totalNotes = 0;
    int automatedSteps = 0;
    int occupiedSteps = 0;

    for (const Pattern& pattern : song.patterns) {
        for (int row = 0; row < pattern.rowCount(); ++row) {
            for (int track = 0; track < pattern.trackCount(); ++track) {
                const PatternStep& step = pattern.step(row, track);
                if (!step.empty()) {
                    ++occupiedSteps;
                }
                if (!step.automation.empty() || !step.effects.empty()) {
                    ++automatedSteps;
                }
                if (!step.note.has_value()) {
                    continue;
                }

                ++totalNotes;
                if (track >= 0 && track < static_cast<int>(notesByTrack.size())) {
                    ++notesByTrack[static_cast<std::size_t>(track)];
                }
                ++notesByInstrument[step.instrument];
            }
        }
    }

    std::ostringstream out;
    out << "Project stats for \"" << song.title << "\"\n";
    out << "patterns: " << song.patterns.size()
        << ", tracks: " << song.tracks.size()
        << ", instruments: " << song.instruments.size()
        << ", order entries: " << song.order.size() << "\n";
    out << "total rows: " << song.totalRows()
        << ", duration: " << song.durationSeconds() << " seconds"
        << ", bpm: " << song.bpm << "\n";
    out << "notes: " << totalNotes
        << ", occupied steps: " << occupiedSteps
        << ", automated steps: " << automatedSteps << "\n";

    out << "\nNotes by track\n";
    for (std::size_t track = 0; track < song.tracks.size(); ++track) {
        out << std::right << std::setw(2) << track << " | "
            << std::left << std::setw(12) << song.tracks[track].name << " | "
            << notesByTrack[track] << "\n";
    }

    out << "\nNotes by instrument\n";
    for (const auto& [instrument, count] : notesByInstrument) {
        std::string name = "<missing>";
        if (instrument >= 0 && instrument < static_cast<int>(song.instruments.size())) {
            name = song.instruments[static_cast<std::size_t>(instrument)].patch.name;
        }
        out << std::right << std::setw(2) << instrument << " | "
            << std::left << std::setw(12) << name << " | "
            << count << "\n";
    }

    return out.str();
}

} // namespace arachno
