#include "EditorActions.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <string>

#include "PatternEditor.h"

namespace arachno {

namespace {
std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool containsCaseInsensitive(const std::string& haystack, const std::string& needle) {
    return lowerCopy(haystack).find(needle) != std::string::npos;
}

bool actionMatchesQuery(const EditorAction& action, const std::string& normalizedQuery) {
    if (normalizedQuery.empty()) {
        return true;
    }
    return containsCaseInsensitive(action.id, normalizedQuery)
        || containsCaseInsensitive(action.command, normalizedQuery)
        || containsCaseInsensitive(action.label, normalizedQuery)
        || containsCaseInsensitive(action.category, normalizedQuery)
        || containsCaseInsensitive(action.defaultShortcut, normalizedQuery)
        || containsCaseInsensitive(action.description, normalizedQuery);
}

EditorActionState enabledState(const EditorAction& action) {
    EditorActionState state;
    state.id = action.id;
    return state;
}

EditorActionState disabledState(const EditorAction& action, const std::string& reason) {
    EditorActionState state;
    state.id = action.id;
    state.enabled = false;
    state.disabledReason = reason;
    return state;
}
} // namespace

const std::vector<EditorAction>& editorActions() {
    static const std::vector<EditorAction> actions = {
        {"navigation.pattern", "pattern <index>", "Select pattern", "Navigation", "Ctrl+P", "Switch the active pattern.", false},
        {"navigation.move", "move <row> <track>", "Move cursor", "Navigation", "Ctrl+G", "Move directly to a row and track.", false},
        {"navigation.up", "up [rows]", "Move up", "Navigation", "Up", "Move the cursor up by one or more rows.", false},
        {"navigation.down", "down [rows]", "Move down", "Navigation", "Down", "Move the cursor down by one or more rows.", false},
        {"navigation.left", "left [tracks]", "Move left", "Navigation", "Left", "Move the cursor left by one or more tracks.", false},
        {"navigation.right", "right [tracks]", "Move right", "Navigation", "Right", "Move the cursor right by one or more tracks.", false},

        {"selection.select", "select <row> <track> <rows> <tracks>", "Select range", "Selection", "Shift+Drag", "Select a rectangular pattern range.", false},
        {"selection.copy", "copy", "Copy", "Selection", "Ctrl+C", "Copy the current selection to the editor clipboard.", false},
        {"selection.cut", "cut", "Cut", "Selection", "Ctrl+X", "Copy then clear the current selection.", true},
        {"selection.paste", "paste [row] [track]", "Paste", "Selection", "Ctrl+V", "Paste clipboard contents at the cursor or target cell.", true},
        {"selection.clear", "clear-selection", "Clear selection", "Selection", "Delete", "Clear every step in the current selection.", true},

        {"history.undo", "undo", "Undo", "History", "Ctrl+Z", "Restore the previous editing state.", true},
        {"history.redo", "redo", "Redo", "History", "Ctrl+Shift+Z", "Restore the next editing state after undo.", true},

        {"step.note", "note <note> [velocity]", "Enter note", "Step Editing", "Return", "Write a note into the active step.", true},
        {"step.instrument", "inst <index>", "Set instrument", "Step Editing", "I", "Assign an instrument to the active step.", true},
        {"step.gate", "gate <rows>", "Set gate", "Step Editing", "G", "Set note length in tracker rows.", true},
        {"step.probability", "probability <0..1|clear>", "Set probability", "Step Editing", "P", "Set deterministic trigger probability for the active step.", true},
        {"step.retrigger", "retrig <count> [spacing] [decay]", "Set retrigger", "Step Editing", "R", "Repeat the active step for rolls, ratchets, and drum fills.", true},
        {"step.clear", "clear", "Clear step", "Step Editing", "Backspace", "Clear the active step.", true},
        {"step.automation", "param <name> <value>", "Set automation", "Step Editing", "A", "Write step automation for a synthesizer parameter.", true},
        {"step.automation.clear", "param-clear [name|*]", "Clear automation", "Step Editing", "Shift+A", "Clear one or all automation values on the active step.", true},
        {"step.effect", "fx <name> <value>", "Set effect value", "Step Editing", "E", "Set a per-cell effect value on the active step.", true},
        {"step.effect.param", "fxp <name> <param> <value>", "Set effect parameter", "Step Editing", "Shift+E", "Set a named per-cell effect parameter on the active step.", true},
        {"step.effect.clear", "fx-clear [name|*]", "Clear effects", "Step Editing", "", "Clear one or all per-cell effects on the active step.", true},
        {"step.transpose", "transpose <semitones> [track]", "Transpose", "Step Editing", "T", "Transpose the active step or an entire track.", true},

        {"generate.scale", "fill-scale <track> <start> <count> <stride> <root> <scale> <inst> [velocity] [gate]", "Fill scale", "Composition", "Ctrl+F", "Generate a scale-based melodic or harmonic sequence.", true},
        {"generate.euclid", "euclid <track> <start> <steps> <pulses> <root> <inst> [velocity] [gate]", "Euclidean rhythm", "Composition", "Ctrl+E", "Generate an evenly distributed rhythmic pattern.", true},

        {"arrangement.tempo", "tempo <bpm>", "Set tempo", "Arrangement", "Ctrl+T", "Set project tempo.", true},
        {"arrangement.rows_per_beat", "rows-per-beat <rows>", "Set rows per beat", "Arrangement", "", "Set rhythmic resolution.", true},
        {"arrangement.append_order", "append-order [pattern]", "Append order", "Arrangement", "", "Append a pattern to the song order.", true},
        {"arrangement.insert_order", "insert-order <index> [pattern]", "Insert order", "Arrangement", "", "Insert a pattern into the song order.", true},
        {"arrangement.remove_order", "remove-order <index>", "Remove order", "Arrangement", "", "Remove an order entry.", true},
        {"arrangement.set_order", "set-order <pattern...>", "Set order", "Arrangement", "", "Replace the full song order.", true},

        {"pattern.new", "new-pattern <name> <rows> [tracks]", "New pattern", "Pattern", "Ctrl+N", "Create a new pattern.", true},
        {"pattern.clone", "clone-pattern [name]", "Clone pattern", "Pattern", "Ctrl+D", "Duplicate the active pattern.", true},
        {"pattern.delete", "delete-pattern [pattern]", "Delete pattern", "Pattern", "", "Delete a pattern and repair order references.", true},
        {"pattern.rename", "pattern-name <name>", "Rename pattern", "Pattern", "F2", "Rename the active pattern.", true},
        {"pattern.resize", "resize-pattern <rows>", "Resize pattern", "Pattern", "", "Resize the active pattern row count.", true},

        {"track.new", "new-track <name>", "New track", "Track", "Ctrl+Shift+N", "Create a new track across all patterns.", true},
        {"track.duplicate", "duplicate-track <source> [name]", "Duplicate track", "Track", "Ctrl+Shift+D", "Duplicate a track across all patterns.", true},
        {"track.delete", "delete-track <track>", "Delete track", "Track", "", "Delete a track across the project.", true},
        {"track.rename", "track-name <track> <name>", "Rename track", "Track", "", "Rename a track.", true},
        {"track.clear", "clear-track <track>", "Clear track", "Track", "", "Clear all steps in one track.", true},
        {"track.volume", "track-volume <track> <value>", "Track volume", "Mixer", "", "Set track volume.", true},
        {"track.pan", "track-pan <track> <value>", "Track pan", "Mixer", "", "Set track pan.", true},
        {"track.mute", "track-mute <track> <true|false>", "Track mute", "Mixer", "M", "Mute or unmute a track.", true},
        {"track.solo", "track-solo <track> <true|false>", "Track solo", "Mixer", "S", "Solo or unsolo a track.", true},

        {"instrument.new", "new-instrument <name>", "New instrument", "Instrument", "", "Create a new synthesizer instrument.", true},
        {"instrument.clone", "clone-instrument <source> [name]", "Clone instrument", "Instrument", "", "Duplicate a synthesizer instrument.", true},
        {"instrument.rename", "instrument-name <instrument> <name>", "Rename instrument", "Instrument", "", "Rename a synthesizer instrument.", true},
        {"instrument.wave", "instrument-wave <instrument> <A|B|C|D> <wave>", "Set oscillator wave", "Instrument", "", "Set oscillator waveform.", true},
        {"instrument.parameter", "instrument-param <instrument> <name> <value>", "Set synth parameter", "Instrument", "", "Set a synthesizer patch parameter.", true},

        {"project.title", "title <text>", "Set title", "Project", "", "Set project title.", true},
        {"project.author", "author <text>", "Set author", "Project", "", "Set project author.", true},
        {"project.description", "description <text>", "Set description", "Project", "", "Set project description.", true},
        {"project.notes", "notes <text>", "Set notes", "Project", "", "Set project notes.", true},
    };
    return actions;
}

const EditorAction* findEditorAction(const std::string& id) {
    const std::vector<EditorAction>& actions = editorActions();
    const auto it = std::find_if(actions.begin(), actions.end(), [&id](const EditorAction& action) {
        return action.id == id;
    });
    return it == actions.end() ? nullptr : &(*it);
}

std::vector<EditorAction> searchEditorActions(const std::string& query) {
    const std::string normalizedQuery = lowerCopy(query);
    std::vector<EditorAction> matches;
    for (const EditorAction& action : editorActions()) {
        if (actionMatchesQuery(action, normalizedQuery)) {
            matches.push_back(action);
        }
    }
    return matches;
}

std::vector<EditorActionState> buildEditorActionStates(const PatternEditorSession& editor) {
    std::vector<EditorActionState> states;
    states.reserve(editorActions().size());
    for (const EditorAction& action : editorActions()) {
        if (action.id == "history.undo" && !editor.canUndo()) {
            states.push_back(disabledState(action, "nothing to undo"));
        } else if (action.id == "history.redo" && !editor.canRedo()) {
            states.push_back(disabledState(action, "nothing to redo"));
        } else if (action.id == "selection.paste" && !editor.hasClipboard()) {
            states.push_back(disabledState(action, "clipboard is empty"));
        } else {
            states.push_back(enabledState(action));
        }
    }
    return states;
}

bool isEditorActionEnabled(const PatternEditorSession& editor, const std::string& id) {
    const std::vector<EditorActionState> states = buildEditorActionStates(editor);
    const auto it = std::find_if(states.begin(), states.end(), [&id](const EditorActionState& state) {
        return state.id == id;
    });
    return it != states.end() && it->enabled;
}

std::string renderEditorActionTable() {
    std::ostringstream out;
    out << "Editor actions\n";
    out << std::left
        << std::setw(28) << "ID"
        << std::setw(14) << "Category"
        << std::setw(18) << "Shortcut"
        << std::setw(22) << "Label"
        << "Command\n";

    for (const EditorAction& action : editorActions()) {
        out << std::left
            << std::setw(28) << action.id
            << std::setw(14) << action.category
            << std::setw(18) << (action.defaultShortcut.empty() ? "-" : action.defaultShortcut)
            << std::setw(22) << action.label
            << action.command
            << (action.mutatesProject ? " *" : "")
            << "\n";
    }
    out << "* changes the project\n";
    return out.str();
}

} // namespace arachno
