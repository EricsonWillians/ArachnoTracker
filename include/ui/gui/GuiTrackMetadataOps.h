#pragma once

#include <functional>
#include <string>

#include "AppActions.h"
#include "ProjectLifecycle.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

using GuiTrackBeginInlinePromptFn = std::function<void(
    InlinePromptKind,
    const std::string&,
    const std::string&,
    const std::string&,
    int,
    int)>;

struct GuiTrackMetadataActionContext {
    std::function<AppSessionSnapshot()> activeSnapshot;
    GuiTrackBeginInlinePromptFn beginInlinePrompt;
    std::function<AppActionResult(const AppActionRequest&)> runAction;
};

void runTrackMetadataAction(const GuiTrackMetadataActionContext& context, const std::string& role, int track);

} // namespace arachno
