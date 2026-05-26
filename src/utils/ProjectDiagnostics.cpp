#include "ProjectDiagnostics.h"

#include <algorithm>
#include <sstream>

#include "Instrument.h"
#include "StepEffects.h"

namespace arachno {

namespace {
void addDiagnostic(
    std::vector<ProjectDiagnostic>& diagnostics,
    DiagnosticSeverity severity,
    const std::string& code,
    const std::string& message,
    DiagnosticLocation location = {}) {
    diagnostics.push_back({severity, code, message, location});
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

DiagnosticLocation orderLocation(int orderIndex) {
    DiagnosticLocation location;
    location.orderIndex = orderIndex;
    return location;
}

DiagnosticLocation trackLocation(int track) {
    DiagnosticLocation location;
    location.track = track;
    return location;
}

DiagnosticLocation instrumentLocation(int instrument) {
    DiagnosticLocation location;
    location.instrument = instrument;
    return location;
}

DiagnosticLocation patternLocation(int pattern) {
    DiagnosticLocation location;
    location.pattern = pattern;
    return location;
}

DiagnosticLocation stepLocation(int pattern, int row, int track) {
    DiagnosticLocation location;
    location.pattern = pattern;
    location.row = row;
    location.track = track;
    return location;
}
} // namespace

std::vector<ProjectDiagnostic> validateProject(const Song& song) {
    std::vector<ProjectDiagnostic> diagnostics;

    if (song.title.empty()) {
        addDiagnostic(diagnostics, DiagnosticSeverity::Warning, "project.empty_title", "project title is empty");
    }
    if (song.bpm <= 0.0) {
        addDiagnostic(diagnostics, DiagnosticSeverity::Error, "tempo.invalid", "tempo must be positive");
    }
    if (song.rowsPerBeat <= 0) {
        addDiagnostic(diagnostics, DiagnosticSeverity::Error, "rhythm.invalid_rows_per_beat", "rows per beat must be positive");
    }
    if (song.sampleRate <= 0) {
        addDiagnostic(diagnostics, DiagnosticSeverity::Error, "audio.invalid_sample_rate", "sample rate must be positive");
    }
    if (song.tracks.empty()) {
        addDiagnostic(diagnostics, DiagnosticSeverity::Error, "tracks.empty", "project has no tracks");
    }
    if (song.instruments.empty()) {
        addDiagnostic(diagnostics, DiagnosticSeverity::Error, "instruments.empty", "project has no instruments");
    }
    if (song.patterns.empty()) {
        addDiagnostic(diagnostics, DiagnosticSeverity::Error, "patterns.empty", "project has no patterns");
    }
    if (song.order.empty()) {
        addDiagnostic(diagnostics, DiagnosticSeverity::Error, "order.empty", "arrangement order is empty");
    }

    for (std::size_t orderIndex = 0; orderIndex < song.order.size(); ++orderIndex) {
        const int patternIndex = song.order[orderIndex];
        if (patternIndex < 0 || patternIndex >= static_cast<int>(song.patterns.size())) {
            addDiagnostic(
                diagnostics,
                DiagnosticSeverity::Error,
                "order.missing_pattern",
                "order entry " + std::to_string(orderIndex) + " references missing pattern "
                    + std::to_string(patternIndex),
                orderLocation(static_cast<int>(orderIndex)));
        }
    }

    for (std::size_t trackIndex = 0; trackIndex < song.tracks.size(); ++trackIndex) {
        const Track& track = song.tracks[trackIndex];
        if (track.name.empty()) {
            addDiagnostic(
                diagnostics,
                DiagnosticSeverity::Warning,
                "track.empty_name",
                "track " + std::to_string(trackIndex) + " has an empty name",
                trackLocation(static_cast<int>(trackIndex)));
        }
        if (track.volume < 0.0) {
            addDiagnostic(
                diagnostics,
                DiagnosticSeverity::Error,
                "track.negative_volume",
                "track " + std::to_string(trackIndex) + " has negative volume",
                trackLocation(static_cast<int>(trackIndex)));
        }
        if (track.pan < -1.0 || track.pan > 1.0) {
            addDiagnostic(
                diagnostics,
                DiagnosticSeverity::Warning,
                "track.pan_range",
                "track " + std::to_string(trackIndex) + " pan is outside -1..1",
                trackLocation(static_cast<int>(trackIndex)));
        }
    }

    for (std::size_t instrumentIndex = 0; instrumentIndex < song.instruments.size(); ++instrumentIndex) {
        const Instrument& instrument = song.instruments[instrumentIndex];
        if (instrument.patch.name.empty()) {
            addDiagnostic(
                diagnostics,
                DiagnosticSeverity::Warning,
                "instrument.empty_name",
                "instrument " + std::to_string(instrumentIndex) + " has an empty name",
                instrumentLocation(static_cast<int>(instrumentIndex)));
        }
    }

    for (std::size_t patternIndex = 0; patternIndex < song.patterns.size(); ++patternIndex) {
        const Pattern& pattern = song.patterns[patternIndex];
        if (pattern.trackCount() != static_cast<int>(song.tracks.size())) {
            addDiagnostic(
                diagnostics,
                DiagnosticSeverity::Warning,
                "pattern.track_count_mismatch",
                "pattern " + std::to_string(patternIndex) + " track count differs from project tracks",
                patternLocation(static_cast<int>(patternIndex)));
        }
        if (!hasAnyNotes(pattern)) {
            addDiagnostic(
                diagnostics,
                DiagnosticSeverity::Warning,
                "pattern.empty",
                "pattern " + std::to_string(patternIndex) + " contains no notes",
                patternLocation(static_cast<int>(patternIndex)));
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
                        "step.missing_instrument",
                        "pattern " + std::to_string(patternIndex) + " row " + std::to_string(row)
                            + " track " + std::to_string(track) + " references missing instrument "
                            + std::to_string(step.instrument),
                        stepLocation(static_cast<int>(patternIndex), row, track));
                }
                if (step.gate <= 0.0) {
                    addDiagnostic(
                        diagnostics,
                        DiagnosticSeverity::Error,
                        "step.invalid_gate",
                        "pattern " + std::to_string(patternIndex) + " row " + std::to_string(row)
                            + " track " + std::to_string(track) + " has non-positive gate",
                        stepLocation(static_cast<int>(patternIndex), row, track));
                }
                if (step.note->midi < 0 || step.note->midi > 127) {
                    addDiagnostic(
                        diagnostics,
                        DiagnosticSeverity::Error,
                        "step.invalid_midi",
                        "pattern " + std::to_string(patternIndex) + " row " + std::to_string(row)
                            + " track " + std::to_string(track) + " has MIDI note outside 0..127",
                        stepLocation(static_cast<int>(patternIndex), row, track));
                }
                if (step.note->velocity < 0.0f || step.note->velocity > 1.0f) {
                    addDiagnostic(
                        diagnostics,
                        DiagnosticSeverity::Warning,
                        "step.velocity_range",
                        "pattern " + std::to_string(patternIndex) + " row " + std::to_string(row)
                            + " track " + std::to_string(track) + " velocity is outside 0..1",
                        stepLocation(static_cast<int>(patternIndex), row, track));
                }
                if (step.probability.has_value()
                    && (step.probability.value() < 0.0 || step.probability.value() > 1.0)) {
                    addDiagnostic(
                        diagnostics,
                        DiagnosticSeverity::Error,
                        "step.probability_range",
                        "pattern " + std::to_string(patternIndex) + " row " + std::to_string(row)
                            + " track " + std::to_string(track) + " probability is outside 0..1",
                        stepLocation(static_cast<int>(patternIndex), row, track));
                }
                if (step.retriggerCount <= 0 || step.retriggerSpacingRows <= 0.0
                    || step.retriggerVelocityDecay < 0.0 || step.retriggerVelocityDecay > 1.0) {
                    addDiagnostic(
                        diagnostics,
                        DiagnosticSeverity::Error,
                        "step.invalid_retrigger",
                        "pattern " + std::to_string(patternIndex) + " row " + std::to_string(row)
                            + " track " + std::to_string(track) + " has invalid retrigger settings",
                        stepLocation(static_cast<int>(patternIndex), row, track));
                }
                for (const auto& [parameter, value] : step.automation) {
                    SynthPatch patch;
                    if (!setSynthPatchParameter(patch, parameter, value)) {
                        addDiagnostic(
                            diagnostics,
                            DiagnosticSeverity::Error,
                            "step.unknown_automation",
                            "pattern " + std::to_string(patternIndex) + " row " + std::to_string(row)
                                + " track " + std::to_string(track) + " automates unknown parameter "
                                + parameter,
                            stepLocation(static_cast<int>(patternIndex), row, track));
                    }
                }
                for (const EffectCommand& effect : step.effects) {
                    if (!isKnownStepEffectCommand(effect)) {
                        addDiagnostic(
                            diagnostics,
                            DiagnosticSeverity::Error,
                            "step.unknown_effect",
                            "pattern " + std::to_string(patternIndex) + " row " + std::to_string(row)
                                + " track " + std::to_string(track) + " uses unsupported effect "
                                + effect.name,
                            stepLocation(static_cast<int>(patternIndex), row, track));
                    }
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

int countDiagnostics(
    const std::vector<ProjectDiagnostic>& diagnostics,
    DiagnosticSeverity severity) {
    return static_cast<int>(std::count_if(diagnostics.begin(), diagnostics.end(), [severity](const ProjectDiagnostic& diagnostic) {
        return diagnostic.severity == severity;
    }));
}

const char* diagnosticSeverityName(DiagnosticSeverity severity) {
    return severity == DiagnosticSeverity::Error ? "error" : "warning";
}

std::string formatDiagnosticLocation(const DiagnosticLocation& location) {
    std::ostringstream out;
    bool wrote = false;
    if (location.orderIndex >= 0) {
        out << "order " << location.orderIndex;
        wrote = true;
    }
    if (location.pattern >= 0) {
        if (wrote) {
            out << " ";
        }
        out << "pattern " << location.pattern;
        wrote = true;
    }
    if (location.row >= 0) {
        out << " row " << location.row;
        wrote = true;
    }
    if (location.track >= 0) {
        if (!wrote) {
            out << "track";
        } else {
            out << " track";
        }
        out << " " << location.track;
        wrote = true;
    }
    if (location.instrument >= 0) {
        if (wrote) {
            out << " ";
        }
        out << "instrument " << location.instrument;
        wrote = true;
    }
    return wrote ? out.str() : "project";
}

std::string formatDiagnostics(const std::vector<ProjectDiagnostic>& diagnostics) {
    if (diagnostics.empty()) {
        return "Project diagnostics: OK\n";
    }

    std::ostringstream out;
    out << "Project diagnostics: "
        << countDiagnostics(diagnostics, DiagnosticSeverity::Error) << " error(s), "
        << countDiagnostics(diagnostics, DiagnosticSeverity::Warning) << " warning(s)\n";
    for (const ProjectDiagnostic& diagnostic : diagnostics) {
        out << "- " << diagnosticSeverityName(diagnostic.severity)
            << " [" << diagnostic.code << "] "
            << formatDiagnosticLocation(diagnostic.location)
            << ": " << diagnostic.message << "\n";
    }
    return out.str();
}

} // namespace arachno
