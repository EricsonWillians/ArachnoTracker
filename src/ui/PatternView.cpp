#include "PatternView.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace arachno {

namespace {
std::string formatStep(const PatternStep& step) {
    if (!step.note.has_value()) {
        return ".... .. ..";
    }

    std::ostringstream out;
    const std::string automationMarker = step.automation.empty() ? " " : "*";
    out << std::left << std::setw(4) << step.note->name()
        << " i" << std::setw(2) << step.instrument
        << " v" << std::setw(3) << static_cast<int>(step.note->velocity * 100.0f)
        << automationMarker;
    return out.str();
}
} // namespace

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
    out << "id | name         | osc A    | osc B    | mix  | cutoff | drive | gain | lfo  | vib\n";

    for (const Instrument& instrument : song.instruments) {
        const SynthPatch& patch = instrument.patch;
        out << std::right << std::setw(2) << instrument.id << " | "
            << std::left << std::setw(12) << patch.name << " | "
            << std::setw(8) << waveformName(patch.oscillatorA) << " | "
            << std::setw(8) << waveformName(patch.oscillatorB) << " | "
            << std::right << std::setw(4) << patch.oscillatorMix << " | "
            << std::setw(6) << patch.cutoff << " | "
            << std::setw(5) << patch.drive << " | "
            << std::setw(4) << patch.gain << " | "
            << std::setw(4) << patch.lfoRate << " | "
            << std::setw(3) << patch.vibratoCents << "\n";
    }

    return out.str();
}

} // namespace arachno
