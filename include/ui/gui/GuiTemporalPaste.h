#pragma once

#include <string>

namespace arachno {

struct TemporalPasteParseContext {
    int cursorRow = 0;
    int rowsPerBeat = 4;
    int selectionRows = 1;
};

struct TemporalPastePlan {
    int startRow = 0;
    int repeats = 1;
    int intervalRows = 1;
    int offsetRows = 0;
};

struct TemporalPasteParseResult {
    bool ok = false;
    std::string error;
    TemporalPastePlan plan;
};

TemporalPasteParseResult parseTemporalPasteSpec(
    const std::string& specText,
    const TemporalPasteParseContext& context);

} // namespace arachno

