#pragma once

#include <functional>
#include <string>

#include "AppActions.h"

namespace arachno {

struct GuiLastActionState {
    bool ok = false;
    std::string actionId;
    std::string message;
    std::string error;
};

struct GuiArrangementOpsContext {
    std::function<AppSessionSnapshot()> snapshot;
    std::function<AppActionResult(const AppActionRequest& request)> runAction;
};

int expandedPatternRowsForRow(int activePatternRows, int row);

bool selectPatternIndex(
    int index,
    bool resetViewStart,
    const GuiArrangementOpsContext& context,
    bool& keyboardSelectionActive,
    int& viewStartRow);

bool selectOrderIndex(
    int index,
    bool syncPatternSelection,
    const GuiArrangementOpsContext& context,
    int& selectedOrderIndex,
    bool& keyboardSelectionActive,
    int& viewStartRow);

bool appendOrderFromActivePattern(
    const GuiArrangementOpsContext& context,
    int& selectedOrderIndex);

bool insertOrderAtSelection(
    const GuiArrangementOpsContext& context,
    int& selectedOrderIndex);

bool removeSelectedOrder(
    const GuiArrangementOpsContext& context,
    int& selectedOrderIndex);

std::string normalizedPatternNameToken(std::string value, const std::string& fallback);

GuiLastActionState buildSongToTargetSeconds(
    double targetSeconds,
    const GuiArrangementOpsContext& context);

GuiLastActionState trimSongToTargetSeconds(
    double targetSeconds,
    const GuiArrangementOpsContext& context);

GuiLastActionState executeTemporalPasteSpec(
    const std::string& specText,
    const GuiArrangementOpsContext& context,
    const std::function<void(int row)>& ensurePatternRowsForRow);

} // namespace arachno
