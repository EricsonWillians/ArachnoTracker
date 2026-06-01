#pragma once

#include <functional>
#include <string>

#include "AppActions.h"
#include "ui/gui/GuiArrangementOps.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiWindowEditingBindingsContext {
    int& selectedOrderIndex;
    bool& keyboardSelectionActive;
    int& viewStartRow;
    int& activePatternRows;
    TrackerWindowLayout& layout;
    int& gridTrackStart;
    int& armedInstrument;
    float& defaultVelocity;

    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<AppActionResult(const AppActionRequest&)> runAction;
    std::function<AppActionResult(const AppActionRequest&, bool)> runActionWithRefresh;
    std::function<void(
        InlinePromptKind,
        const std::string&,
        const std::string&,
        const std::string&,
        int,
        int)> beginInlinePrompt;
    std::function<std::string(std::string, const std::string&)> normalizedPatternNameToken;
    std::function<void(int)> ensurePatternRowsForRow;
    std::function<void(const GuiLastActionState&)> applyLastActionState;
};

struct GuiWindowEditingBindings {
    std::function<bool(int, bool)> selectPatternIndex;
    std::function<bool(int, bool)> selectOrderIndex;
    std::function<bool()> appendOrderFromActivePattern;
    std::function<bool()> insertOrderAtSelection;
    std::function<bool()> removeSelectedOrder;
    std::function<void()> beginPatternCreatePrompt;
    std::function<void()> beginPatternClonePrompt;
    std::function<bool()> deleteActivePattern;
    std::function<void()> beginTemporalPastePrompt;
    std::function<bool(const std::string&)> executeTemporalPasteSpec;
    std::function<void(const std::string&, int)> runTrackMetadataAction;
    std::function<void(double)> buildSongToTargetSeconds;
    std::function<void(double)> trimSongToTargetSeconds;
    std::function<AppActionResult(int, int, bool)> moveCursor;
    std::function<void(int, int, int, int)> applySelectionRange;
    std::function<void(int, int, int)> paintNoteAt;
    std::function<bool(int, int, int&, int&)> gridPositionToCell;
};

GuiWindowEditingBindings makeWindowEditingBindingsFromState(const GuiWindowEditingBindingsContext& context);

} // namespace arachno
