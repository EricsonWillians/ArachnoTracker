#include "ProjectDiagnostics.h"

#include <algorithm>
#include <sstream>

namespace arachno {

namespace {
void addDiagnostic(
    std::vector<ProjectDiagnostic>& diagnostics,
    DiagnosticSeverity severity,
    const std::string& message) {
    diagnostics.push_back({severity, message});
}

bool hasAnyNotes(const Pattern& pattern) {
    for (const PatternRow& row : pattern.rows()) {
        for (const PatternStep& step : row.steps) {
            if (step.note.has_value()) {
                return true;
            }
        }
    }
    return false;
}

const char* severityName(DiagnosticSeverity severity) {
    return severity == DiagnosticSeverity::Error ? "error" : "warning";
}
} // namespace

std::vector<ProjectDiagnostic> validateProject(const Song& song) {
    std::vector<ProjectDiagnostic> diagnostics;

    if (song.title.empty()) {
        addDiagnostic(diagnostics, DiagnosticSeverity::Warning, "project title is empty");
    }
    if (song.bpm <= 0.0) {
        addDiagnostic(diagnostics, DiagnosticSeverity::Error, "tempo must be positive");
    }
    if (song.rowsPerBeat <= 0) {
        addDiagnostic(diagnostics, DiagnosticSeverity::Error, "rows per beat must be positive");
    }
    if (song.sampleRate <= 0) {
        addDiagnostic(diagnostics, DiagnosticSeverity::Error, "sample rate must be positive");
    }
    if (song.tracks.empty()) {
        addDiagnostic(diagnostics, DiagnosticSeverity::Error, "project has no tracks");
    }
    if (song.instruments.empty()) {
        addDiagnostic(diagnostics, DiagnosticSeverity::Error, "project has no instruments");
    }
    if (song.patterns.empty()) {
        addDiagnostic(diagnostics, DiagnosticSeverity::Error, "project has no patterns");
    }
    if (song.order.empty()) {
        addDiagnostic(diagnostics, DiagnosticSeverity::Error, "arrangement order is empty");
    }

    for (std::size_t orderIndex = 0; orderIndex < song.order.size(); ++orderIndex) {
        const int patternIndex = song.order[orderIndex];
        if (patternIndex < 0 || patternIndex >= static_cast<int>(song.patterns.size())) {
            addDiagnostic(
                diagnostics,
                DiagnosticSeverity::Error,
                "order entry " + std::to_string(orderIndex) + " references missing pattern "
                    + std::to_string(patternIndex));
        }
    }

    for (std::size_t trackIndex = 0; trackIndex < song.tracks.size(); ++trackIndex) {
        const Track& track = song.tracks[trackIndex];
        if (track.name.empty()) {
            addDiagnostic(
                diagnostics,
                DiagnosticSeverity::Warning,
                "track " + std::to_string(trackIndex) + " has an empty name");
        }
        if (track.volume < 0.0) {
            addDiagnostic(
                diagnostics,
                DiagnosticSeverity::Error,
                "track " + std::to_string(trackIndex) + " has negative volume");
        }
        if (track.pan < -1.0 || track.pan > 1.0) {
            addDiagnostic(
                diagnostics,
                DiagnosticSeverity::Warning,
                "track " + std::to_string(trackIndex) + " pan is outside -1..1");
        }
    }

    for (std::size_t patternIndex = 0; patternIndex < song.patterns.size(); ++patternIndex) {
        const Pattern& pattern = song.patterns[patternIndex];
        if (pattern.trackCount() != static_cast<int>(song.tracks.size())) {
            addDiagnostic(
                diagnostics,
                DiagnosticSeverity::Warning,
                "pattern " + std::to_string(patternIndex) + " track count differs from project tracks");
        }
        if (!hasAnyNotes(pattern)) {
            addDiagnostic(
                diagnostics,
                DiagnosticSeverity::Warning,
                "pattern " + std::to_string(patternIndex) + " contains no notes");
        }

        for (int row = 0; row < pattern.rowCount(); ++row) {
            for (int track = 0; track < pattern.trackCount(); ++track) {
                const PatternStep& step = pattern.step(row, track);
                if (!step.note.has_value()) {
                    continue;
                }

                if (step.instrument < 0 || step.instrument >= static_cast<int>(song.instruments.size())) {
                    addDiagnostic(
                        diagnostics,
                        DiagnosticSeverity::Error,
                        "pattern " + std::to_string(patternIndex) + " row " + std::to_string(row)
                            + " track " + std::to_string(track) + " references missing instrument "
                            + std::to_string(step.instrument));
                }
                if (step.gate <= 0.0) {
                    addDiagnostic(
                        diagnostics,
                        DiagnosticSeverity::Error,
                        "pattern " + std::to_string(patternIndex) + " row " + std::to_string(row)
                            + " track " + std::to_string(track) + " has non-positive gate");
                }
                if (step.note->midi < 0 || step.note->midi > 127) {
                    addDiagnostic(
                        diagnostics,
                        DiagnosticSeverity::Error,
                        "pattern " + std::to_string(patternIndex) + " row " + std::to_string(row)
                            + " track " + std::to_string(track) + " has MIDI note outside 0..127");
                }
                if (step.note->velocity < 0.0f || step.note->velocity > 1.0f) {
                    addDiagnostic(
                        diagnostics,
                        DiagnosticSeverity::Warning,
                        "pattern " + std::to_string(patternIndex) + " row " + std::to_string(row)
                            + " track " + std::to_string(track) + " velocity is outside 0..1");
                }
            }
        }
    }

    return diagnostics;
}

bool hasErrors(const std::vector<ProjectDiagnostic>& diagnostics) {
    return std::any_of(diagnostics.begin(), diagnostics.end(), [](const ProjectDiagnostic& diagnostic) {
        return diagnostic.severity == DiagnosticSeverity::Error;
    });
}

std::string formatDiagnostics(const std::vector<ProjectDiagnostic>& diagnostics) {
    if (diagnostics.empty()) {
        return "Project diagnostics: OK\n";
    }

    std::ostringstream out;
    out << "Project diagnostics:\n";
    for (const ProjectDiagnostic& diagnostic : diagnostics) {
        out << "- " << severityName(diagnostic.severity) << ": " << diagnostic.message << "\n";
    }
    return out.str();
}

} // namespace arachno
