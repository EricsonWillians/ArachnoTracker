#pragma once

#include <string>
#include <vector>

#include "EditorActions.h"

namespace arachno {

struct ShortcutBinding {
    std::string shortcut;
    std::string actionId;
};

struct ShortcutConflict {
    std::string shortcut;
    std::vector<std::string> actionIds;
};

std::string normalizeShortcut(const std::string& shortcut);
std::vector<ShortcutBinding> defaultEditorShortcuts();
std::vector<const EditorAction*> findEditorActionsForShortcut(
    const std::vector<ShortcutBinding>& bindings,
    const std::string& shortcut);
const EditorAction* findEditorActionForShortcut(
    const std::vector<ShortcutBinding>& bindings,
    const std::string& shortcut);
std::vector<ShortcutConflict> validateEditorShortcuts(const std::vector<ShortcutBinding>& bindings);
std::string renderEditorShortcutTable(const std::vector<ShortcutBinding>& bindings);

} // namespace arachno
