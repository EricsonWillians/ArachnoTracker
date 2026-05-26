#pragma once

#include <string>
#include <vector>

#include "EditorActions.h"
#include "EditorShortcuts.h"
#include "PatternEditor.h"

namespace arachno {

struct CommandPaletteEntry {
    std::string actionId;
    std::string label;
    std::string category;
    std::string command;
    std::string shortcut;
    std::string description;
    bool enabled = true;
    std::string disabledReason;
    bool mutatesProject = false;
};

std::vector<CommandPaletteEntry> buildEditorCommandPalette(
    const PatternEditorSession& editor,
    const std::vector<ShortcutBinding>& bindings = defaultEditorShortcuts(),
    const std::string& query = "");
std::string renderEditorCommandPalette(const std::vector<CommandPaletteEntry>& entries);

} // namespace arachno
