#include "ui/gui/GuiTemporalPaste.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

namespace arachno {

namespace {

std::string trimCopy(const std::string& text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool parsePositiveInt(const std::string& text, int& value) {
    std::istringstream in(text);
    in >> value;
    return static_cast<bool>(in) && in.eof() && value > 0;
}

bool parseRowUnit(std::string token, int rowsPerBeat, int rowsPerBar, int selectionRows, int& value, bool allowZero = false) {
    token = lowerCopy(trimCopy(token));
    if (token.rfind("every_", 0) == 0) {
        token = token.substr(6);
    }
    if (token == "beat") {
        value = rowsPerBeat;
        return true;
    }
    if (token == "bar") {
        value = rowsPerBar;
        return true;
    }
    if (token == "sel" || token == "selection") {
        value = selectionRows;
        return true;
    }
    if (token.rfind("rows:", 0) == 0) {
        int parsed = 0;
        std::istringstream in(token.substr(5));
        in >> parsed;
        if (!in || !in.eof() || parsed < 0 || (!allowZero && parsed == 0)) {
            return false;
        }
        value = parsed;
        return true;
    }
    int parsed = 0;
    std::istringstream in(token);
    in >> parsed;
    if (!in || !in.eof() || parsed < 0 || (!allowZero && parsed == 0)) {
        return false;
    }
    value = parsed;
    return true;
}

bool parseAnchor(std::string token, int cursorRow, int rowsPerBeat, int rowsPerBar, int selectionRows, int& value) {
    token = lowerCopy(trimCopy(token));
    if (token == "cursor") {
        value = cursorRow;
        return true;
    }
    if (token == "next_row" || token == "nextrow") {
        value = cursorRow + 1;
        return true;
    }
    if (token == "next_beat" || token == "nextbeat") {
        value = ((cursorRow / rowsPerBeat) + 1) * rowsPerBeat;
        return true;
    }
    if (token == "next_bar" || token == "nextbar") {
        value = ((cursorRow / rowsPerBar) + 1) * rowsPerBar;
        return true;
    }
    if (token == "next_sel" || token == "next_selection" || token == "nextsel") {
        value = cursorRow + selectionRows;
        return true;
    }
    if (token.rfind("abs:", 0) == 0) {
        int parsed = 0;
        std::istringstream in(token.substr(4));
        in >> parsed;
        if (!in || !in.eof() || parsed < 0) {
            return false;
        }
        value = parsed;
        return true;
    }
    int parsed = 0;
    std::istringstream in(token);
    in >> parsed;
    if (!in || !in.eof() || parsed < 0) {
        return false;
    }
    value = parsed;
    return true;
}

} // namespace

TemporalPasteParseResult parseTemporalPasteSpec(
    const std::string& specText,
    const TemporalPasteParseContext& context) {
    TemporalPasteParseResult result;

    std::vector<std::string> tokens;
    {
        std::istringstream in(specText);
        std::string token;
        while (in >> token) {
            tokens.push_back(token);
        }
    }
    if (tokens.size() < 3) {
        result.error = "expected: anchor repeats interval [offset]";
        return result;
    }

    const int rowsPerBeat = std::max(1, context.rowsPerBeat);
    const int rowsPerBar = std::max(rowsPerBeat, rowsPerBeat * 4);
    const int selectionRows = std::max(1, context.selectionRows);
    const int cursorRow = std::max(0, context.cursorRow);

    if (!parseAnchor(tokens[0], cursorRow, rowsPerBeat, rowsPerBar, selectionRows, result.plan.startRow)) {
        result.error = "invalid anchor token";
        return result;
    }
    if (!parsePositiveInt(tokens[1], result.plan.repeats)) {
        result.error = "repeats must be a positive integer";
        return result;
    }
    result.plan.repeats = std::clamp(result.plan.repeats, 1, 512);

    if (!parseRowUnit(tokens[2], rowsPerBeat, rowsPerBar, selectionRows, result.plan.intervalRows)) {
        result.error = "invalid interval token";
        return result;
    }
    if (tokens.size() >= 4
        && !parseRowUnit(tokens[3], rowsPerBeat, rowsPerBar, selectionRows, result.plan.offsetRows, true)) {
        result.error = "invalid offset token";
        return result;
    }

    result.ok = true;
    return result;
}

} // namespace arachno

