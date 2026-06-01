#pragma once

#include <functional>
#include <string>

#include "AppActions.h"
#include "ApplicationSession.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiFileButtonContext {
    bool& audioTuningDialogActive;
    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<void()> toggleSynthWindow;
    std::function<void(const AppActionRequest&)> runLifecycleAction;
    std::function<void(const AppActionRequest&)> runAction;
    std::function<void(InlinePromptKind, const std::string&, const std::string&, const std::string&, int, int)>
        beginInlinePrompt;
};

void runFileButtonAction(const GuiFileButtonContext& context, const std::string& actionId);

} // namespace arachno
