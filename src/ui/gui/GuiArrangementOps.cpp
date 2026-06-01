#include "ui/gui/GuiArrangementOps.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

#include "ui/gui/GuiTemporalPaste.h"

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

} // namespace

int expandedPatternRowsForRow(int activePatternRows, int row) {
    if (row < activePatternRows) {
        return std::clamp(activePatternRows, 8, 8192);
    }
    int targetRows = std::max(8, activePatternRows);
    while (targetRows <= row && targetRows < 8192) {
        targetRows += (targetRows < 256 ? 16 : 32);
    }
    return std::clamp(targetRows, 8, 8192);
}

bool selectPatternIndex(
    int index,
    bool resetViewStart,
    const GuiArrangementOpsContext& context,
    bool& keyboardSelectionActive,
    int& viewStartRow) {
    const AppSessionSnapshot snap = context.snapshot();
    const int count = static_cast<int>(snap.editor.patterns.size());
    if (count <= 0) {
        return false;
    }
    const int clamped = std::clamp(index, 0, count - 1);
    AppActionRequest pattern;
    pattern.actionId = "editor.navigation.pattern";
    pattern.parameters = {{"index", std::to_string(clamped)}};
    const AppActionResult nav = context.runAction(pattern);
    if (nav.ok) {
        keyboardSelectionActive = false;
        if (resetViewStart) {
            viewStartRow = 0;
        }
    }
    return nav.ok;
}

bool selectOrderIndex(
    int index,
    bool syncPatternSelection,
    const GuiArrangementOpsContext& context,
    int& selectedOrderIndex,
    bool& keyboardSelectionActive,
    int& viewStartRow) {
    const AppSessionSnapshot snap = context.snapshot();
    const int count = static_cast<int>(snap.editor.order.size());
    if (count <= 0) {
        selectedOrderIndex = 0;
        return false;
    }
    selectedOrderIndex = std::clamp(index, 0, count - 1);
    if (!syncPatternSelection) {
        return true;
    }
    const OrderSlotSummary& slot = snap.editor.order[static_cast<std::size_t>(selectedOrderIndex)];
    if (slot.missing || slot.pattern < 0) {
        return false;
    }
    return selectPatternIndex(slot.pattern, true, context, keyboardSelectionActive, viewStartRow);
}

bool appendOrderFromActivePattern(
    const GuiArrangementOpsContext& context,
    int& selectedOrderIndex) {
    const AppSessionSnapshot snap = context.snapshot();
    if (snap.editor.patterns.empty()) {
        return false;
    }
    const int activePattern = std::clamp(
        snap.editor.status.activePattern,
        0,
        static_cast<int>(snap.editor.patterns.size()) - 1);
    AppActionRequest append;
    append.actionId = "editor.arrangement.append_order";
    append.parameters = {{"pattern", std::to_string(activePattern)}};
    const AppActionResult result = context.runAction(append);
    if (result.ok) {
        const AppSessionSnapshot updated = context.snapshot();
        selectedOrderIndex = std::max(0, static_cast<int>(updated.editor.order.size()) - 1);
    }
    return result.ok;
}

bool insertOrderAtSelection(
    const GuiArrangementOpsContext& context,
    int& selectedOrderIndex) {
    const AppSessionSnapshot snap = context.snapshot();
    if (snap.editor.patterns.empty()) {
        return false;
    }
    const int activePattern = std::clamp(
        snap.editor.status.activePattern,
        0,
        static_cast<int>(snap.editor.patterns.size()) - 1);
    const int orderCount = static_cast<int>(snap.editor.order.size());
    const int insertIndex = std::clamp(selectedOrderIndex, 0, std::max(0, orderCount));
    AppActionRequest insert;
    insert.actionId = "editor.arrangement.insert_order";
    insert.parameters = {
        {"index", std::to_string(insertIndex)},
        {"pattern", std::to_string(activePattern)}};
    const AppActionResult result = context.runAction(insert);
    if (result.ok) {
        selectedOrderIndex = insertIndex;
    }
    return result.ok;
}

bool removeSelectedOrder(
    const GuiArrangementOpsContext& context,
    int& selectedOrderIndex) {
    const AppSessionSnapshot snap = context.snapshot();
    const int orderCount = static_cast<int>(snap.editor.order.size());
    if (orderCount <= 1) {
        return false;
    }
    const int removeIndex = std::clamp(selectedOrderIndex, 0, orderCount - 1);
    AppActionRequest remove;
    remove.actionId = "editor.arrangement.remove_order";
    remove.parameters = {{"index", std::to_string(removeIndex)}};
    const AppActionResult result = context.runAction(remove);
    if (result.ok) {
        const AppSessionSnapshot updated = context.snapshot();
        selectedOrderIndex = std::clamp(removeIndex, 0, std::max(0, static_cast<int>(updated.editor.order.size()) - 1));
    }
    return result.ok;
}

std::string normalizedPatternNameToken(std::string value, const std::string& fallback) {
    value = trimCopy(value);
    std::string out;
    out.reserve(value.size());
    bool previousUnderscore = false;
    for (const unsigned char raw : value) {
        const char ch = static_cast<char>(raw);
        if (std::isspace(raw)) {
            if (!out.empty() && !previousUnderscore) {
                out.push_back('_');
                previousUnderscore = true;
            }
            continue;
        }
        out.push_back(ch);
        previousUnderscore = ch == '_';
    }
    while (!out.empty() && out.back() == '_') {
        out.pop_back();
    }
    return out.empty() ? fallback : out;
}

GuiLastActionState buildSongToTargetSeconds(
    double targetSeconds,
    const GuiArrangementOpsContext& context) {
    GuiLastActionState lastAction;
    if (targetSeconds <= 1.0) {
        lastAction.ok = false;
        lastAction.actionId = "arrangement.build_length";
        lastAction.error = "target length must be greater than 1 second";
        return lastAction;
    }
    AppSessionSnapshot snap = context.snapshot();
    if (snap.editor.patterns.empty()) {
        lastAction.ok = false;
        lastAction.actionId = "arrangement.build_length";
        lastAction.error = "no patterns available";
        return lastAction;
    }
    const int activePattern = std::clamp(
        snap.editor.status.activePattern,
        0,
        static_cast<int>(snap.editor.patterns.size()) - 1);
    int appended = 0;
    constexpr int kMaxAppend = 4096;
    double previousDuration = snap.editor.status.durationSeconds;
    while (snap.editor.status.durationSeconds + 0.01 < targetSeconds && appended < kMaxAppend) {
        AppActionRequest append;
        append.actionId = "editor.arrangement.append_order";
        append.parameters = {{"pattern", std::to_string(activePattern)}};
        context.runAction(append);
        snap = context.snapshot();
        ++appended;
        if (snap.editor.status.durationSeconds <= previousDuration + 1e-6) {
            break;
        }
        previousDuration = snap.editor.status.durationSeconds;
    }
    if (appended <= 0) {
        lastAction.ok = false;
        lastAction.actionId = "arrangement.build_length";
        lastAction.error = "unable to extend arrangement";
        return lastAction;
    }
    std::ostringstream message;
    message.setf(std::ios::fixed);
    message.precision(2);
    message << "built arrangement to " << snap.editor.status.durationSeconds << "s";
    lastAction.ok = true;
    lastAction.actionId = "arrangement.build_length";
    lastAction.message = message.str();
    return lastAction;
}

GuiLastActionState trimSongToTargetSeconds(
    double targetSeconds,
    const GuiArrangementOpsContext& context) {
    GuiLastActionState lastAction;
    if (targetSeconds <= 1.0) {
        lastAction.ok = false;
        lastAction.actionId = "arrangement.trim_length";
        lastAction.error = "target length must be greater than 1 second";
        return lastAction;
    }
    AppSessionSnapshot snap = context.snapshot();
    int removed = 0;
    constexpr int kMaxTrim = 4096;
    while (snap.editor.status.durationSeconds - 0.01 > targetSeconds
           && static_cast<int>(snap.editor.order.size()) > 1
           && removed < kMaxTrim) {
        AppActionRequest remove;
        remove.actionId = "editor.arrangement.remove_order";
        remove.parameters = {{"index", std::to_string(static_cast<int>(snap.editor.order.size()) - 1)}};
        context.runAction(remove);
        snap = context.snapshot();
        ++removed;
    }
    if (removed <= 0) {
        lastAction.ok = false;
        lastAction.actionId = "arrangement.trim_length";
        lastAction.error = "nothing to trim";
        return lastAction;
    }
    std::ostringstream message;
    message.setf(std::ios::fixed);
    message.precision(2);
    message << "trimmed arrangement to " << snap.editor.status.durationSeconds << "s";
    lastAction.ok = true;
    lastAction.actionId = "arrangement.trim_length";
    lastAction.message = message.str();
    return lastAction;
}

GuiLastActionState executeTemporalPasteSpec(
    const std::string& specText,
    const GuiArrangementOpsContext& context,
    const std::function<void(int row)>& ensurePatternRowsForRow) {
    GuiLastActionState lastAction;
    AppSessionSnapshot snap = context.snapshot();
    if (!snap.editor.clipboard.available) {
        AppActionRequest copy;
        copy.actionId = "editor.selection.copy";
        context.runAction(copy);
        snap = context.snapshot();
    }
    if (!snap.editor.clipboard.available) {
        lastAction.ok = false;
        lastAction.actionId = "editor.selection.paste_temporal";
        lastAction.error = "clipboard is empty (copy a selection first)";
        return lastAction;
    }

    const int selectionRows = std::max(1, snap.editor.status.selectionRows);
    const int clipboardRows = std::max(1, snap.editor.clipboard.rowCount);
    const int targetTrack = std::max(0, snap.editor.status.selectionStartTrack);
    TemporalPasteParseContext parseContext;
    parseContext.cursorRow = std::max(0, snap.editor.status.cursorRow);
    parseContext.rowsPerBeat = std::max(1, snap.editor.status.rowsPerBeat);
    parseContext.selectionRows = selectionRows;
    const TemporalPasteParseResult parsed = parseTemporalPasteSpec(specText, parseContext);
    if (!parsed.ok) {
        lastAction.ok = false;
        lastAction.actionId = "editor.selection.paste_temporal";
        lastAction.error = parsed.error;
        return lastAction;
    }

    const int firstRow = std::max(0, parsed.plan.startRow + parsed.plan.offsetRows);
    int pasted = 0;
    for (int index = 0; index < parsed.plan.repeats; ++index) {
        const int row = firstRow + (index * parsed.plan.intervalRows);
        ensurePatternRowsForRow(row + clipboardRows - 1);
        AppActionRequest paste;
        paste.actionId = "editor.selection.paste";
        paste.parameters = {
            {"row", std::to_string(row)},
            {"track", std::to_string(targetTrack)}};
        const AppActionResult action = context.runAction(paste);
        if (!action.ok) {
            lastAction.ok = false;
            lastAction.actionId = "editor.selection.paste_temporal";
            lastAction.error = "paste failed";
            return lastAction;
        }
        ++pasted;
    }

    lastAction.ok = true;
    lastAction.actionId = "editor.selection.paste_temporal";
    std::ostringstream message;
    message << "Pasted " << pasted << "x every " << parsed.plan.intervalRows << " rows";
    lastAction.message = message.str();
    return lastAction;
}

} // namespace arachno
