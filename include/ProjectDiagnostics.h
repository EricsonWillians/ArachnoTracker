#pragma once

#include <string>
#include <vector>

#include "Tracker.h"

namespace arachno {

enum class DiagnosticSeverity {
    Warning,
    Error
};

struct ProjectDiagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::Warning;
    std::string message;
};

std::vector<ProjectDiagnostic> validateProject(const Song& song);
bool hasErrors(const std::vector<ProjectDiagnostic>& diagnostics);
std::string formatDiagnostics(const std::vector<ProjectDiagnostic>& diagnostics);

} // namespace arachno
