#pragma once

#include <functional>
#include <string>

#include "AppActions.h"
#include "ProjectLifecycle.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

using GuiBeginInlinePromptFn = std::function<void(
    InlinePromptKind,
    const std::string&,
    const std::string&,
    const std::string&,
    int,
    int)>;

struct GuiPatternPromptContext {
    std::function<AppSessionSnapshot()> activeSnapshot;
    int activePatternRows = 64;
    GuiBeginInlinePromptFn beginInlinePrompt;
    std::function<std::string(std::string, const std::string&)> normalizedPatternNameToken;
};

void beginPatternCreatePrompt(const GuiPatternPromptContext& context);
void beginPatternClonePrompt(const GuiPatternPromptContext& context);
void beginTemporalPastePrompt(const GuiBeginInlinePromptFn& beginInlinePrompt);

struct GuiPatternDeleteContext {
    std::function<AppActionResult(const AppActionRequest&)> runAction;
    bool& keyboardSelectionActive;
    int& viewStartRow;
};

bool deleteActivePattern(const GuiPatternDeleteContext& context);

} // namespace arachno
