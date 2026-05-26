#include "EditorCommandPalette.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <map>
#include <sstream>

namespace arachno {

namespace {
std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool containsCaseInsensitive(const std::string& haystack, const std::string& normalizedNeedle) {
    return normalizedNeedle.empty() || lowerCopy(haystack).find(normalizedNeedle) != std::string::npos;
}

bool matchesQuery(const EditorAction& action, const std::string& shortcut, const std::string& normalizedQuery) {
    return containsCaseInsensitive(action.id, normalizedQuery)
        || containsCaseInsensitive(action.label, normalizedQuery)
        || containsCaseInsensitive(action.category, normalizedQuery)
        || containsCaseInsensitive(action.command, normalizedQuery)
        || containsCaseInsensitive(action.description, normalizedQuery)
        || containsCaseInsensitive(shortcut, normalizedQuery);
}

std::map<std::string, std::string> shortcutByActionId(const std::vector<ShortcutBinding>& bindings) {
    std::map<std::string, std::string> shortcuts;
    for (const ShortcutBinding& binding : bindings) {
        shortcuts.emplace(binding.actionId, normalizeShortcut(binding.shortcut));
    }
    return shortcuts;
}

std::map<std::string, EditorActionState> stateByActionId(const PatternEditorSession& editor) {
    std::map<std::string, EditorActionState> states;
    for (const EditorActionState& state : buildEditorActionStates(editor)) {
        states[state.id] = state;
    }
    return states;
}
} // namespace

std::vector<CommandPaletteEntry> buildEditorCommandPalette(
    const PatternEditorSession& editor,
    const std::vector<ShortcutBinding>& bindings,
    const std::string& query) {
    const std::map<std::string, std::string> shortcuts = shortcutByActionId(bindings);
    const std::map<std::string, EditorActionState> states = stateByActionId(editor);
    const std::string normalizedQuery = lowerCopy(query);

    std::vector<CommandPaletteEntry> entries;
    for (const EditorAction& action : editorActions()) {
        const auto shortcutIt = shortcuts.find(action.id);
        const std::string shortcut = shortcutIt == shortcuts.end() ? "" : shortcutIt->second;
        if (!matchesQuery(action, shortcut, normalizedQuery)) {
            continue;
        }

        CommandPaletteEntry entry;
        entry.actionId = action.id;
        entry.label = action.label;
        entry.category = action.category;
        entry.command = action.command;
        entry.shortcut = shortcut;
        entry.description = action.description;
        entry.mutatesProject = action.mutatesProject;

        const auto stateIt = states.find(action.id);
        if (stateIt != states.end()) {
            entry.enabled = stateIt->second.enabled;
            entry.disabledReason = stateIt->second.disabledReason;
        }
        entries.push_back(entry);
    }

    std::stable_sort(entries.begin(), entries.end(), [](const CommandPaletteEntry& left, const CommandPaletteEntry& right) {
        if (left.enabled != right.enabled) {
            return left.enabled;
        }
        if (left.category != right.category) {
            return left.category < right.category;
        }
        return left.label < right.label;
    });
    return entries;
}

std::string renderEditorCommandPalette(const std::vector<CommandPaletteEntry>& entries) {
    std::ostringstream out;
    out << "Command palette\n";
    out << std::left
        << std::setw(14) << "Category"
        << std::setw(24) << "Label"
        << std::setw(18) << "Shortcut"
        << std::setw(10) << "State"
        << "Command\n";

    for (const CommandPaletteEntry& entry : entries) {
        out << std::left
            << std::setw(14) << entry.category
            << std::setw(24) << entry.label
            << std::setw(18) << (entry.shortcut.empty() ? "-" : entry.shortcut)
            << std::setw(10) << (entry.enabled ? "enabled" : "disabled")
            << entry.command;
        if (!entry.disabledReason.empty()) {
            out << " (" << entry.disabledReason << ")";
        }
        out << "\n";
    }
    return out.str();
}

} // namespace arachno
