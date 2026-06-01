#pragma once

#include <functional>
#include <string>

#include "AppActions.h"
#include "ui/gui/GuiArrangementOps.h"
#include "ui/gui/GuiPatternPromptOps.h"
#include "ui/gui/GuiTrackMetadataOps.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiWindowArrangementAdapterContext {
    int& selectedOrderIndex;
    bool& keyboardSelectionActive;
    int& viewStartRow;
    int activePatternRows = 64;

    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<AppActionResult(const AppActionRequest&)> runAction;
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

GuiArrangementOpsContext makeArrangementOpsContextFromAdapter(const GuiWindowArrangementAdapterContext& context);

bool selectPatternIndexFromAdapter(
    const GuiWindowArrangementAdapterContext& context,
    int index,
    bool resetViewStart);
bool selectOrderIndexFromAdapter(
    const GuiWindowArrangementAdapterContext& context,
    int index,
    bool syncPatternSelection);
bool appendOrderFromActivePatternFromAdapter(const GuiWindowArrangementAdapterContext& context);
bool insertOrderAtSelectionFromAdapter(const GuiWindowArrangementAdapterContext& context);
bool removeSelectedOrderFromAdapter(const GuiWindowArrangementAdapterContext& context);

void beginPatternCreatePromptFromAdapter(const GuiWindowArrangementAdapterContext& context);
void beginPatternClonePromptFromAdapter(const GuiWindowArrangementAdapterContext& context);
bool deleteActivePatternFromAdapter(const GuiWindowArrangementAdapterContext& context);
bool executeTemporalPasteSpecFromAdapter(const GuiWindowArrangementAdapterContext& context, const std::string& specText);

void runTrackMetadataActionFromAdapter(
    const GuiWindowArrangementAdapterContext& context,
    const std::string& role,
    int track);
void buildSongToTargetSecondsFromAdapter(const GuiWindowArrangementAdapterContext& context, double targetSeconds);
void trimSongToTargetSecondsFromAdapter(const GuiWindowArrangementAdapterContext& context, double targetSeconds);

} // namespace arachno
