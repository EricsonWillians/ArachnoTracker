#include "EditorShortcuts.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace arachno {

namespace {
std::string trim(const std::string& value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string canonicalToken(const std::string& rawToken) {
    const std::string token = lowerCopy(trim(rawToken));
    if (token.empty()) {
        return "";
    }
    if (token == "control" || token == "ctrl") {
        return "Ctrl";
    }
    if (token == "shift") {
        return "Shift";
    }
    if (token == "alt" || token == "option") {
        return "Alt";
    }
    if (token == "super" || token == "meta" || token == "cmd" || token == "command") {
        return "Super";
    }
    if (token == "return" || token == "enter") {
        return "Return";
    }
    if (token == "delete" || token == "del") {
        return "Delete";
    }
    if (token == "backspace" || token == "bksp") {
        return "Backspace";
    }
    if (token == "up" || token == "down" || token == "left" || token == "right") {
        std::string copy = token;
        copy.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(copy.front())));
        return copy;
    }
    if (token.size() == 1) {
        return std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(token.front()))));
    }
    if (token.front() == 'f' && token.size() <= 3) {
        bool number = token.size() > 1;
        for (std::size_t i = 1; i < token.size(); ++i) {
            number = number && std::isdigit(static_cast<unsigned char>(token[i]));
        }
        if (number) {
            std::string copy = token;
            copy.front() = 'F';
            return copy;
        }
    }

    std::string copy = token;
    copy.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(copy.front())));
    return copy;
}

int modifierRank(const std::string& token) {
    if (token == "Ctrl") {
        return 0;
    }
    if (token == "Shift") {
        return 1;
    }
    if (token == "Alt") {
        return 2;
    }
    if (token == "Super") {
        return 3;
    }
    return 4;
}

std::vector<std::string> splitShortcut(const std::string& shortcut) {
    std::vector<std::string> tokens;
    std::size_t start = 0;
    while (start <= shortcut.size()) {
        const std::size_t plus = shortcut.find('+', start);
        const std::size_t end = plus == std::string::npos ? shortcut.size() : plus;
        const std::string token = canonicalToken(shortcut.substr(start, end - start));
        if (!token.empty()) {
            tokens.push_back(token);
        }
        if (plus == std::string::npos) {
            break;
        }
        start = plus + 1;
    }
    return tokens;
}
} // namespace

std::string normalizeShortcut(const std::string& shortcut) {
    std::vector<std::string> tokens = splitShortcut(shortcut);
    if (tokens.empty()) {
        return "";
    }

    std::stable_sort(tokens.begin(), tokens.end(), [](const std::string& left, const std::string& right) {
        return modifierRank(left) < modifierRank(right);
    });

    std::ostringstream out;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0) {
            out << "+";
        }
        out << tokens[i];
    }
    return out.str();
}

std::vector<ShortcutBinding> defaultEditorShortcuts() {
    std::vector<ShortcutBinding> bindings;
    for (const EditorAction& action : editorActions()) {
        if (!action.defaultShortcut.empty()) {
            bindings.push_back({normalizeShortcut(action.defaultShortcut), action.id});
        }
    }
    return bindings;
}

std::vector<const EditorAction*> findEditorActionsForShortcut(
    const std::vector<ShortcutBinding>& bindings,
    const std::string& shortcut) {
    const std::string normalized = normalizeShortcut(shortcut);
    std::vector<const EditorAction*> actions;
    for (const ShortcutBinding& binding : bindings) {
        if (normalizeShortcut(binding.shortcut) != normalized) {
            continue;
        }
        const EditorAction* action = findEditorAction(binding.actionId);
        if (action != nullptr) {
            actions.push_back(action);
        }
    }
    return actions;
}

const EditorAction* findEditorActionForShortcut(
    const std::vector<ShortcutBinding>& bindings,
    const std::string& shortcut) {
    const std::vector<const EditorAction*> actions = findEditorActionsForShortcut(bindings, shortcut);
    return actions.empty() ? nullptr : actions.front();
}

std::vector<ShortcutConflict> validateEditorShortcuts(const std::vector<ShortcutBinding>& bindings) {
    std::map<std::string, std::set<std::string>> grouped;
    for (const ShortcutBinding& binding : bindings) {
        const std::string normalized = normalizeShortcut(binding.shortcut);
        if (normalized.empty()) {
            continue;
        }
        if (findEditorAction(binding.actionId) == nullptr) {
            throw std::invalid_argument("shortcut references unknown action: " + binding.actionId);
        }
        grouped[normalized].insert(binding.actionId);
    }

    std::vector<ShortcutConflict> conflicts;
    for (const auto& [shortcut, actionIds] : grouped) {
        if (actionIds.size() <= 1) {
            continue;
        }
        ShortcutConflict conflict;
        conflict.shortcut = shortcut;
        conflict.actionIds.assign(actionIds.begin(), actionIds.end());
        conflicts.push_back(conflict);
    }
    return conflicts;
}

std::string renderEditorShortcutTable(const std::vector<ShortcutBinding>& bindings) {
    std::ostringstream out;
    out << "Editor shortcuts\n";
    out << std::left
        << std::setw(18) << "Shortcut"
        << std::setw(28) << "Action"
        << "Label\n";

    std::vector<ShortcutBinding> sorted = bindings;
    std::sort(sorted.begin(), sorted.end(), [](const ShortcutBinding& left, const ShortcutBinding& right) {
        const std::string leftShortcut = normalizeShortcut(left.shortcut);
        const std::string rightShortcut = normalizeShortcut(right.shortcut);
        if (leftShortcut == rightShortcut) {
            return left.actionId < right.actionId;
        }
        return leftShortcut < rightShortcut;
    });

    for (const ShortcutBinding& binding : sorted) {
        const EditorAction* action = findEditorAction(binding.actionId);
        out << std::left
            << std::setw(18) << normalizeShortcut(binding.shortcut)
            << std::setw(28) << binding.actionId
            << (action == nullptr ? "<unknown>" : action->label)
            << "\n";
    }
    return out.str();
}

} // namespace arachno
