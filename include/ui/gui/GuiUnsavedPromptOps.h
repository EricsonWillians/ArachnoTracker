#pragma once

#include <functional>

#include "AppActions.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiUnsavedPromptResolveContext {
    UnsavedDecisionPromptState& unsavedPrompt;
    AppActionResult& lastAction;
    bool& hasDeferredPostSaveAction;
    AppActionRequest& deferredPostSaveAction;

    std::function<void()> clearUnsavedPrompt;
    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<void(InlinePromptKind, const std::string&, const std::string&, const std::string&, int, int)>
        beginInlinePrompt;
    std::function<void(const AppActionRequest&)> runLifecycleAction;
};

void resolveUnsavedPromptChoice(const GuiUnsavedPromptResolveContext& context, UnsavedChangesChoice choice);

} // namespace arachno
