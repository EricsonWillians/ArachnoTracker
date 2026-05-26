#pragma once

#include <string>
#include <vector>

namespace arachno {

class PatternEditorSession;

struct EditorAction {
    std::string id;
    std::string command;
    std::string label;
    std::string category;
    std::string defaultShortcut;
    std::string description;
    bool mutatesProject = false;
};

struct EditorActionState {
    std::string id;
    bool enabled = true;
    std::string disabledReason;
};

const std::vector<EditorAction>& editorActions();
const EditorAction* findEditorAction(const std::string& id);
std::vector<EditorAction> searchEditorActions(const std::string& query);
std::vector<EditorActionState> buildEditorActionStates(const PatternEditorSession& editor);
bool isEditorActionEnabled(const PatternEditorSession& editor, const std::string& id);
std::string renderEditorActionTable();

} // namespace arachno
