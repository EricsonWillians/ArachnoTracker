#pragma once

#include <string>
#include <vector>

#include "Tracker.h"

namespace arachno {

enum class DiagnosticSeverity {
    Warning,
    Error
};

struct DiagnosticLocation {
    int orderIndex = -1;
    int pattern = -1;
    int row = -1;
    int track = -1;
    int instrument = -1;
};

struct ProjectDiagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::Warning;
    std::string code;
    std::string message;
    DiagnosticLocation location;
};

std::vector<ProjectDiagnostic> validateProject(const Song& song);
bool hasErrors(const std::vector<ProjectDiagnostic>& diagnostics);
int countDiagnostics(
    const std::vector<ProjectDiagnostic>& diagnostics,
    DiagnosticSeverity severity);
const char* diagnosticSeverityName(DiagnosticSeverity severity);
std::string formatDiagnosticLocation(const DiagnosticLocation& location);
std::string formatDiagnostics(const std::vector<ProjectDiagnostic>& diagnostics);

} // namespace arachno
