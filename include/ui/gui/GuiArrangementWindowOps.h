#pragma once

#include <functional>

#include "ui/gui/GuiArrangementOps.h"

namespace arachno {

GuiArrangementOpsContext makeArrangementOpsContextFromWindowState(
    const std::function<AppSessionSnapshot()>& snapshot,
    const std::function<AppActionResult(const AppActionRequest&)>& runAction);

bool selectPatternIndexFromWindowState(
    int index,
    bool resetViewStart,
    const GuiArrangementOpsContext& arrangementContext,
    bool& keyboardSelectionActive,
    int& viewStartRow);

bool selectOrderIndexFromWindowState(
    int index,
    bool syncPatternSelection,
    const GuiArrangementOpsContext& arrangementContext,
    int& selectedOrderIndex,
    bool& keyboardSelectionActive,
    int& viewStartRow);

bool appendOrderFromActivePatternFromWindowState(
    const GuiArrangementOpsContext& arrangementContext,
    int& selectedOrderIndex);

bool insertOrderAtSelectionFromWindowState(
    const GuiArrangementOpsContext& arrangementContext,
    int& selectedOrderIndex);

bool removeSelectedOrderFromWindowState(
    const GuiArrangementOpsContext& arrangementContext,
    int& selectedOrderIndex);

void buildSongToTargetSecondsFromWindowState(
    double targetSeconds,
    const GuiArrangementOpsContext& arrangementContext,
    const std::function<void(const GuiLastActionState&)>& applyLastActionState);

void trimSongToTargetSecondsFromWindowState(
    double targetSeconds,
    const GuiArrangementOpsContext& arrangementContext,
    const std::function<void(const GuiLastActionState&)>& applyLastActionState);

} // namespace arachno
