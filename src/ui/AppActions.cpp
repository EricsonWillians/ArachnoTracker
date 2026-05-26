#include "AppActions.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "Note.h"

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

bool actionMatches(
    const std::string& id,
    const std::string& label,
    const std::string& category,
    const std::string& shortcut,
    const std::string& description,
    const std::string& normalizedQuery) {
    return containsCaseInsensitive(id, normalizedQuery)
        || containsCaseInsensitive(label, normalizedQuery)
        || containsCaseInsensitive(category, normalizedQuery)
        || containsCaseInsensitive(shortcut, normalizedQuery)
        || containsCaseInsensitive(description, normalizedQuery);
}

bool commandTemplateNeedsText(const std::string& command) {
    return command.find('<') != std::string::npos || command.find('[') != std::string::npos;
}

bool isRequiredToken(const std::string& token) {
    return token.size() > 2 && token.front() == '<' && token.back() == '>';
}

bool isOptionalToken(const std::string& token) {
    return token.size() > 2 && token.front() == '[' && token.back() == ']';
}

std::string placeholderBody(const std::string& token) {
    if (isRequiredToken(token) || isOptionalToken(token)) {
        return token.substr(1, token.size() - 2);
    }
    return "";
}

std::string humanizeName(const std::string& name) {
    std::string result = name;
    for (char& ch : result) {
        if (ch == '_' || ch == '-') {
            ch = ' ';
        }
    }
    if (!result.empty()) {
        result.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(result.front())));
    }
    return result;
}

std::vector<std::string> splitTemplate(const std::string& commandTemplate) {
    std::istringstream in(commandTemplate);
    std::vector<std::string> tokens;
    std::string token;
    while (in >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string normalizeParameterName(const std::string& rawName, const std::string& actionId) {
    if (rawName == "0..1|clear" || rawName == "true|false") {
        return "value";
    }
    std::string name = lowerCopy(rawName);
    name.erase(std::remove(name.begin(), name.end(), '.'), name.end());
    std::replace(name.begin(), name.end(), '-', '_');
    if (name == "a|b") {
        return "oscillator";
    }
    if (name == "pattern") {
        return actionId == "arrangement.set_order" ? "patterns" : "pattern";
    }
    if (name == "rows" && actionId == "step.gate") {
        return "gate_rows";
    }
    if (name == "rows" && actionId == "navigation.up") {
        return "row_count";
    }
    if (name == "rows" && actionId == "navigation.down") {
        return "row_count";
    }
    if (name == "tracks" && (actionId == "navigation.left" || actionId == "navigation.right")) {
        return "track_count";
    }
    if (name == "source") {
        return "source_index";
    }
    if (name == "inst") {
        return "instrument";
    }
    return name;
}

AppActionParameterType inferParameterType(const std::string& name, const std::string& rawName) {
    if (name == "path") {
        return AppActionParameterType::FilePath;
    }
    if (name == "note" || name == "root") {
        return AppActionParameterType::NoteName;
    }
    if (name == "oscillator" || name == "wave" || name == "scale" || rawName.find('|') != std::string::npos) {
        return AppActionParameterType::Choice;
    }
    if (name == "value"
        || name == "velocity"
        || name == "gate"
        || name == "gate_rows"
        || name == "spacing"
        || name == "decay"
        || name == "bpm") {
        return AppActionParameterType::Number;
    }
    if (name == "row"
        || name == "track"
        || name == "index"
        || name == "instrument"
        || name == "source_index"
        || name == "count"
        || name == "steps"
        || name == "pulses"
        || name == "stride"
        || name == "row_count"
        || name == "track_count") {
        return AppActionParameterType::Integer;
    }
    return AppActionParameterType::Text;
}

std::vector<std::string> choicesForParameter(const std::string& name, const std::string& rawName) {
    if (name == "oscillator") {
        return {"A", "B", "C"};
    }
    if (name == "wave") {
        return {"sine", "square", "saw", "triangle", "noise"};
    }
    if (name == "scale") {
        return {"major", "minor", "pentatonic", "chromatic"};
    }
    if (rawName == "0..1|clear") {
        return {"clear"};
    }
    if (rawName == "true|false") {
        return {"true", "false"};
    }
    return {};
}

void applyParameterBounds(AppActionParameter& parameter) {
    if (parameter.name == "velocity"
        || parameter.name == "value"
        || parameter.name == "probability") {
        parameter.hasMinimum = true;
        parameter.minimum = 0.0;
        parameter.hasMaximum = true;
        parameter.maximum = 1.0;
    }
    if (parameter.name == "bpm") {
        parameter.hasMinimum = true;
        parameter.minimum = 20.0;
        parameter.hasMaximum = true;
        parameter.maximum = 300.0;
    }
}

std::vector<AppActionParameter> parametersFromCommandTemplate(
    const std::string& actionId,
    const std::string& commandTemplate) {
    std::vector<AppActionParameter> parameters;
    for (const std::string& token : splitTemplate(commandTemplate)) {
        if (!isRequiredToken(token) && !isOptionalToken(token)) {
            continue;
        }

        const std::string rawName = placeholderBody(token);
        AppActionParameter parameter;
        parameter.name = normalizeParameterName(rawName, actionId);
        parameter.label = humanizeName(parameter.name);
        parameter.required = isRequiredToken(token);
        parameter.type = inferParameterType(parameter.name, rawName);
        parameter.choices = choicesForParameter(parameter.name, rawName);
        applyParameterBounds(parameter);
        parameters.push_back(parameter);
    }
    return parameters;
}

std::string commandTemplateForAction(const std::string& actionId) {
    std::string editorActionId = actionId;
    if (editorActionId.rfind("editor.", 0) == 0) {
        editorActionId = editorActionId.substr(7);
    }
    const EditorAction* action = findEditorAction(editorActionId);
    return action == nullptr ? "" : action->command;
}

bool hasParameter(const std::map<std::string, std::string>& parameters, const std::string& name) {
    const auto it = parameters.find(name);
    return it != parameters.end() && !it->second.empty();
}

std::string parameterValue(const std::map<std::string, std::string>& parameters, const std::string& name) {
    const auto it = parameters.find(name);
    return it == parameters.end() ? "" : it->second;
}

bool parseInteger(const std::string& value, int& parsed) {
    std::istringstream in(value);
    in >> parsed;
    return !in.fail() && in.eof();
}

bool parseNumber(const std::string& value, double& parsed) {
    std::istringstream in(value);
    in >> parsed;
    return !in.fail() && in.eof();
}

bool parseBoolean(const std::string& value, bool& parsed) {
    const std::string normalized = lowerCopy(value);
    if (normalized == "true" || normalized == "1" || normalized == "yes" || normalized == "on") {
        parsed = true;
        return true;
    }
    if (normalized == "false" || normalized == "0" || normalized == "no" || normalized == "off") {
        parsed = false;
        return true;
    }
    return false;
}

bool parseExportFormatText(const std::string& value, ExportFormat& format) {
    const std::string normalized = lowerCopy(value);
    if (normalized == "wav") {
        format = ExportFormat::Wav;
        return true;
    }
    if (normalized == "mp3") {
        format = ExportFormat::Mp3;
        return true;
    }
    if (normalized == "ogg") {
        format = ExportFormat::Ogg;
        return true;
    }
    return false;
}

std::optional<std::pair<int, int>> firstOrderRangeForPattern(const Song& song, int patternIndex) {
    if (patternIndex < 0 || patternIndex >= static_cast<int>(song.patterns.size())) {
        return std::nullopt;
    }
    int startRow = 0;
    for (int orderPattern : song.order) {
        if (orderPattern < 0 || orderPattern >= static_cast<int>(song.patterns.size())) {
            continue;
        }
        const Pattern& pattern = song.patterns[static_cast<std::size_t>(orderPattern)];
        const int endRow = startRow + pattern.rowCount();
        if (orderPattern == patternIndex) {
            return std::make_pair(startRow, endRow);
        }
        startRow = endRow;
    }
    return std::nullopt;
}

std::string buildSessionSyncFingerprint(const ApplicationSession& session) {
    const Song& song = session.song();
    std::ostringstream out;
    out << "v1|";
    out << "path=" << session.projectPath() << "|";
    out << "title=" << song.title << "|";
    out << "bpm=" << song.bpm << "|";
    out << "rows_per_beat=" << song.rowsPerBeat << "|";
    out << "sample_rate=" << song.sampleRate << "|";
    out << "tracks=" << song.tracks.size() << "|";
    out << "patterns=" << song.patterns.size() << "|";
    out << "order=" << song.order.size() << "|";
    out << "instruments=" << song.instruments.size();
    return out.str();
}

void populateSyncCheckpointIdentity(SyncCheckpoint& checkpoint, const ApplicationSession& session) {
    checkpoint.projectPath = session.projectPath();
    checkpoint.projectFingerprint = buildSessionSyncFingerprint(session);
}

SyncCheckpointCompatibility inspectSyncCheckpointCompatibility(
    const SyncCheckpoint& checkpoint,
    const ApplicationSession& session,
    std::string& reason) {
    const std::string currentPath = session.projectPath();
    const std::string currentFingerprint = buildSessionSyncFingerprint(session);
    if (!checkpoint.projectPath.empty() && checkpoint.projectPath != currentPath) {
        reason = "project path changed";
        return SyncCheckpointCompatibility::StalePath;
    }
    if (!checkpoint.projectFingerprint.empty() && checkpoint.projectFingerprint != currentFingerprint) {
        reason = "project fingerprint changed";
        return SyncCheckpointCompatibility::StaleFingerprint;
    }
    return SyncCheckpointCompatibility::Ok;
}

AppActionResult::EventDeltaSummary summarizeEvents(
    const std::vector<AppEvent>& events,
    int totalBeforeLimit,
    int droppedByLimit) {
    AppActionResult::EventDeltaSummary summary;
    summary.total = std::max(0, totalBeforeLimit);
    summary.returned = static_cast<int>(events.size());
    summary.dropped = std::max(0, droppedByLimit);
    summary.truncated = summary.dropped > 0;

    for (const AppEvent& event : events) {
        switch (event.type) {
            case AppEventType::ProjectChanged:
            case AppEventType::ProjectLoaded:
            case AppEventType::ProjectSaved:
                ++summary.project;
                break;
            case AppEventType::SettingsChanged:
                ++summary.settings;
                break;
            case AppEventType::EditorChanged:
                ++summary.editor;
                break;
            case AppEventType::DiagnosticsChanged:
                ++summary.diagnostics;
                break;
            case AppEventType::PlaybackChanged:
                ++summary.playback;
                break;
            case AppEventType::AudioRuntimeChanged:
                ++summary.audio;
                break;
            case AppEventType::MessageChanged:
                ++summary.message;
                break;
            case AppEventType::TaskStarted:
            case AppEventType::TaskUpdated:
            case AppEventType::TaskFinished:
                ++summary.tasks;
                break;
            case AppEventType::ScriptImported:
                ++summary.script;
                break;
            case AppEventType::RecoveryChanged:
                ++summary.recovery;
                break;
            case AppEventType::SessionReady:
                ++summary.other;
                break;
        }
    }
    return summary;
}

void populateEventCursor(AppActionResult& result, std::uint64_t requestedSince, bool drain) {
    result.hasEventCursor = true;
    result.eventCursor.requestedSince = requestedSince;
    result.eventCursor.includesSnapshot = result.hasSessionSnapshot;
    result.eventCursor.truncated = result.hasEventDeltaSummary && result.eventDeltaSummary.truncated;
    result.eventCursor.drain = drain;
    if (!result.events.empty()) {
        result.eventCursor.hasReturnedRange = true;
        result.eventCursor.returnedFrom = result.events.front().sequence;
        result.eventCursor.returnedTo = result.events.back().sequence;
    }
    if (result.eventCursor.includesSnapshot && result.hasLastEventSequence) {
        result.eventCursor.recommendedSince = result.lastEventSequence;
    } else if (result.eventCursor.hasReturnedRange) {
        result.eventCursor.recommendedSince = result.eventCursor.returnedTo;
    } else if (drain && result.hasLastEventSequence) {
        result.eventCursor.recommendedSince = result.lastEventSequence;
    } else {
        result.eventCursor.recommendedSince = requestedSince;
    }
}

std::string escapeJson(const std::string& value) {
    std::ostringstream out;
    for (char ch : value) {
        switch (ch) {
            case '\\':
                out << "\\\\";
                break;
            case '\"':
                out << "\\\"";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                out << ch;
                break;
        }
    }
    return out.str();
}

bool choiceMatches(const std::vector<std::string>& choices, const std::string& value) {
    const std::string normalizedValue = lowerCopy(value);
    return std::any_of(choices.begin(), choices.end(), [&normalizedValue](const std::string& choice) {
        return lowerCopy(choice) == normalizedValue;
    });
}

void addFieldError(
    AppActionValidationResult& result,
    const std::string& name,
    const std::string& message) {
    result.fieldErrors.push_back({name, message});
}

void validateParameterValue(
    AppActionValidationResult& result,
    const AppActionParameter& parameter,
    const std::string& value) {
    if (value.empty()) {
        if (parameter.required) {
            result.missingParameters.push_back(parameter.name);
        }
        return;
    }

    if (!parameter.choices.empty() && choiceMatches(parameter.choices, value)) {
        return;
    }

    if (parameter.type == AppActionParameterType::Integer) {
        int parsed = 0;
        if (!parseInteger(value, parsed)) {
            addFieldError(result, parameter.name, "must be an integer");
        }
        return;
    }

    if (parameter.type == AppActionParameterType::Number
        || parameter.hasMinimum
        || parameter.hasMaximum) {
        double parsed = 0.0;
        if (!parseNumber(value, parsed)) {
            addFieldError(result, parameter.name, "must be a number");
            return;
        }
        if (parameter.hasMinimum && parsed < parameter.minimum) {
            addFieldError(result, parameter.name, "must be greater than or equal to " + std::to_string(parameter.minimum));
        }
        if (parameter.hasMaximum && parsed > parameter.maximum) {
            addFieldError(result, parameter.name, "must be less than or equal to " + std::to_string(parameter.maximum));
        }
        return;
    }

    if (parameter.type == AppActionParameterType::Boolean) {
        const std::string normalized = lowerCopy(value);
        if (normalized != "true" && normalized != "false" && normalized != "1" && normalized != "0") {
            addFieldError(result, parameter.name, "must be true or false");
        }
        return;
    }

    if (parameter.type == AppActionParameterType::NoteName) {
        try {
            (void)noteNameToMidi(value);
        } catch (const std::exception&) {
            addFieldError(result, parameter.name, "must be a valid note name");
        }
        return;
    }

    if (parameter.type == AppActionParameterType::Choice && !parameter.choices.empty()) {
        addFieldError(result, parameter.name, "must be one of the allowed choices");
    }
}

AppActionState enabledState(const std::string& id) {
    AppActionState state;
    state.id = id;
    return state;
}

AppActionState disabledState(const std::string& id, const std::string& reason) {
    AppActionState state;
    state.id = id;
    state.enabled = false;
    state.disabledReason = reason;
    return state;
}

std::map<std::string, AppActionState> stateById(const ApplicationSession& session) {
    std::map<std::string, AppActionState> states;
    for (const AppActionState& state : buildApplicationActionStates(session)) {
        states[state.id] = state;
    }
    return states;
}

void copyOperationResult(
    AppActionResult& result,
    const AppOperationResult& operation,
    bool projectChanged = false) {
    result.ok = operation.ok;
    result.message = operation.message;
    result.error = operation.error;
    result.projectChanged = projectChanged && operation.ok;
    result.editorStateChanged = result.projectChanged;
}

ProjectLifecycleAction lifecycleActionForAppAction(const std::string& id) {
    if (id == "project.new") {
        return ProjectLifecycleAction::NewProject;
    }
    if (id == "project.close") {
        return ProjectLifecycleAction::CloseProject;
    }
    if (id == "application.quit") {
        return ProjectLifecycleAction::QuitApplication;
    }
    if (id == "recovery.restore") {
        return ProjectLifecycleAction::RestoreRecovery;
    }
    return ProjectLifecycleAction::OpenProject;
}
} // namespace

const std::vector<AppAction>& applicationActions() {
    static const std::vector<AppAction> actions = {
        {"project.new", "New project", "Project", "Create a fresh project after unsaved-change handling.", "Ctrl+N", AppActionKind::Application, true, false, false},
        {"project.open", "Open project", "Project", "Open an existing project with compatibility and recovery preflight.", "Ctrl+O", AppActionKind::Application, false, true, false},
        {"project.save", "Save project", "Project", "Save the current project to its existing path.", "Ctrl+S", AppActionKind::Application, false, false, false},
        {"project.save_as", "Save project as", "Project", "Save the current project to a chosen path.", "Ctrl+Shift+S", AppActionKind::Application, false, true, false},
        {"project.close", "Close project", "Project", "Close the current project after unsaved-change handling.", "Ctrl+W", AppActionKind::Application, true, false, false},
        {"application.quit", "Quit", "Application", "Quit after unsaved-change handling.", "Ctrl+Q", AppActionKind::Application, false, false, false},
        {"session.snapshot", "Session snapshot", "Session", "Read a full GUI-facing session snapshot.", "", AppActionKind::Application, false, false, false},
        {"session.events", "Session events", "Session", "Poll or drain session events for GUI updates.", "", AppActionKind::Application, false, false, false},
        {"session.sync", "Session sync", "Session", "Sync from a named checkpoint with delta and optional snapshot fallback.", "", AppActionKind::Application, false, false, false},
        {"session.checkpoint.list", "List sync checkpoints", "Session", "List stored sync checkpoints.", "", AppActionKind::Application, false, false, false},
        {"session.checkpoint.save", "Save sync checkpoint", "Session", "Save a named sync checkpoint for event/task resume.", "", AppActionKind::Application, false, false, false},
        {"session.checkpoint.advance", "Advance sync checkpoint", "Session", "Advance a named sync checkpoint with optional compare-and-swap guards.", "", AppActionKind::Application, false, false, false},
        {"session.checkpoint.load", "Load sync checkpoint", "Session", "Load a named sync checkpoint.", "", AppActionKind::Application, false, false, false},
        {"session.checkpoint.clear", "Clear sync checkpoint", "Session", "Remove a named sync checkpoint.", "", AppActionKind::Application, false, false, false},
        {"export.mixdown", "Export mixdown", "Export", "Render full-song audio mixdown.", "", AppActionKind::Application, false, true, false},
        {"export.stems", "Export stems", "Export", "Render one audio file per track.", "", AppActionKind::Application, false, true, false},
        {"export.midi", "Export MIDI", "Export", "Export a Standard MIDI file.", "", AppActionKind::Application, false, true, false},
        {"import.midi", "Import MIDI", "Project", "Import a Standard MIDI file into a tracker project.", "", AppActionKind::Application, true, true, false},
        {"script.import", "Import script artifact", "Scripts", "Import a generated project, patch, or command file artifact.", "", AppActionKind::Application, true, true, false},

        {"playback.play", "Play", "Transport", "Start full-song realtime playback from the beginning.", "Space", AppActionKind::Application, false, false, false},
        {"playback.play_pattern", "Play pattern", "Transport", "Play the active pattern only (looped).", "", AppActionKind::Application, false, false, false},
        {"playback.play_song", "Play song", "Transport", "Play the full arrangement from the beginning.", "F5", AppActionKind::Application, false, false, false},
        {"playback.pause", "Pause", "Transport", "Pause realtime playback.", "Shift+Space", AppActionKind::Application, false, false, false},
        {"playback.stop", "Stop", "Transport", "Stop realtime playback.", "Esc", AppActionKind::Application, false, false, false},
        {"preview.cursor", "Preview cursor", "Transport", "Audition the active tracker step.", "Ctrl+Space", AppActionKind::Application, false, false, false},

        {"audio.runtime.configure", "Configure audio runtime", "Audio", "Configure backend, device, latency, and realtime output preferences.", "", AppActionKind::Application, false, false, false},
        {"audio.runtime.devices", "Audio devices", "Audio", "Inspect detected audio backends/devices and availability.", "", AppActionKind::Application, false, false, false},
        {"audio.runtime.status", "Audio runtime status", "Audio", "Inspect current audio runtime health and latency counters.", "", AppActionKind::Application, false, false, false},
        {"audio.runtime.render_test", "Audio render test", "Audio", "Render test blocks through the runtime for diagnostics.", "", AppActionKind::Application, false, false, false},
        {"audio.runtime.start", "Start audio runtime", "Audio", "Start realtime audio processing with current runtime settings.", "", AppActionKind::Application, false, false, false},
        {"audio.runtime.stop", "Stop audio runtime", "Audio", "Stop realtime audio processing.", "", AppActionKind::Application, false, false, false},
        {"audio.runtime.simulate_underrun", "Simulate underrun", "Audio", "Inject an underrun diagnostic event for UI testing.", "", AppActionKind::Application, false, false, false},

        {"task.cancel", "Cancel task", "Tasks", "Cancel a running background task by task id.", "", AppActionKind::Application, false, false, false},
        {"task.clear_finished", "Clear finished tasks", "Tasks", "Remove succeeded/failed/canceled tasks from the task list.", "", AppActionKind::Application, false, false, false},

        {"recovery.save", "Save recovery snapshot", "Recovery", "Write an autosave recovery snapshot.", "", AppActionKind::Application, false, true, false},
        {"recovery.restore", "Restore recovery snapshot", "Recovery", "Restore a recovery snapshot after unsaved-change handling.", "", AppActionKind::Application, true, true, false},
        {"recovery.clear", "Clear recovery snapshot", "Recovery", "Delete a recovery snapshot.", "", AppActionKind::Application, false, true, false},
    };
    return actions;
}

const char* appActionKindName(AppActionKind kind) {
    switch (kind) {
        case AppActionKind::Application:
            return "application";
        case AppActionKind::Editor:
            return "editor";
    }
    return "unknown";
}

const char* syncCheckpointCompatibilityName(SyncCheckpointCompatibility compatibility) {
    switch (compatibility) {
        case SyncCheckpointCompatibility::NotChecked:
            return "not_checked";
        case SyncCheckpointCompatibility::Ok:
            return "ok";
        case SyncCheckpointCompatibility::Missing:
            return "missing";
        case SyncCheckpointCompatibility::StalePath:
            return "stale_path";
        case SyncCheckpointCompatibility::StaleFingerprint:
            return "stale_fingerprint";
    }
    return "unknown";
}

const char* syncCheckpointUpdateStatusName(SyncCheckpointUpdateStatus status) {
    switch (status) {
        case SyncCheckpointUpdateStatus::NotChecked:
            return "not_checked";
        case SyncCheckpointUpdateStatus::Advanced:
            return "advanced";
        case SyncCheckpointUpdateStatus::SkippedUpdateDisabled:
            return "skipped_update_disabled";
        case SyncCheckpointUpdateStatus::SkippedUnsafeDelta:
            return "skipped_unsafe_delta";
        case SyncCheckpointUpdateStatus::SkippedStalePolicy:
            return "skipped_stale_policy";
        case SyncCheckpointUpdateStatus::Failed:
            return "failed";
    }
    return "unknown";
}

const char* appActionParameterTypeName(AppActionParameterType type) {
    switch (type) {
        case AppActionParameterType::Text:
            return "text";
        case AppActionParameterType::Integer:
            return "integer";
        case AppActionParameterType::Number:
            return "number";
        case AppActionParameterType::Boolean:
            return "boolean";
        case AppActionParameterType::NoteName:
            return "note";
        case AppActionParameterType::FilePath:
            return "file-path";
        case AppActionParameterType::Choice:
            return "choice";
    }
    return "unknown";
}

const AppAction* findApplicationAction(const std::string& id) {
    const auto it = std::find_if(applicationActions().begin(), applicationActions().end(), [&id](const AppAction& action) {
        return action.id == id;
    });
    return it == applicationActions().end() ? nullptr : &(*it);
}

AppActionSchema buildAppActionSchema(const std::string& actionId) {
    AppActionSchema schema;
    schema.actionId = actionId;

    if (actionId.rfind("editor.", 0) == 0) {
        const std::string editorActionId = actionId.substr(7);
        const EditorAction* action = findEditorAction(editorActionId);
        if (action == nullptr) {
            return schema;
        }
        schema.label = action->label;
        schema.commandTemplate = action->command;
        schema.kind = AppActionKind::Editor;
        schema.requiresCommandText = commandTemplateNeedsText(action->command);
        schema.parameters = parametersFromCommandTemplate(editorActionId, action->command);
        return schema;
    }

    const AppAction* action = findApplicationAction(actionId);
    if (action == nullptr) {
        return schema;
    }
    schema.label = action->label;
    schema.kind = AppActionKind::Application;
    schema.requiresPath = action->requiresPath;
    schema.requiresCommandText = action->requiresCommandText;
    if (action->requiresPath) {
        AppActionParameter path;
        path.name = "path";
        path.label = "Path";
        path.type = AppActionParameterType::FilePath;
        path.required = true;
        path.description = "Filesystem path used by this action.";
        schema.parameters.push_back(path);
    }
    if (actionId == "audio.runtime.configure") {
        AppActionParameter backend;
        backend.name = "backend";
        backend.label = "Backend";
        backend.type = AppActionParameterType::Choice;
        backend.required = false;
        backend.defaultValue = "auto";
        backend.choices = {"auto", "pipewire", "jack", "alsa", "dummy"};
        backend.description = "Linux audio backend preference.";
        schema.parameters.push_back(backend);

        AppActionParameter deviceId;
        deviceId.name = "device_id";
        deviceId.label = "Device ID";
        deviceId.type = AppActionParameterType::Text;
        deviceId.required = false;
        deviceId.description = "Backend-specific device identifier.";
        schema.parameters.push_back(deviceId);

        AppActionParameter sampleRate;
        sampleRate.name = "sample_rate";
        sampleRate.label = "Sample Rate";
        sampleRate.type = AppActionParameterType::Integer;
        sampleRate.required = false;
        sampleRate.description = "Output sample rate in Hz.";
        schema.parameters.push_back(sampleRate);

        AppActionParameter bufferFrames;
        bufferFrames.name = "buffer_frames";
        bufferFrames.label = "Buffer Frames";
        bufferFrames.type = AppActionParameterType::Integer;
        bufferFrames.required = false;
        bufferFrames.description = "Audio callback buffer size in frames.";
        schema.parameters.push_back(bufferFrames);

        AppActionParameter periods;
        periods.name = "periods";
        periods.label = "Periods";
        periods.type = AppActionParameterType::Integer;
        periods.required = false;
        periods.description = "Period count used for latency estimation.";
        schema.parameters.push_back(periods);

        AppActionParameter realtimePriority;
        realtimePriority.name = "realtime_priority";
        realtimePriority.label = "Realtime Priority";
        realtimePriority.type = AppActionParameterType::Boolean;
        realtimePriority.required = false;
        realtimePriority.defaultValue = "true";
        realtimePriority.description = "Request realtime scheduling priority.";
        schema.parameters.push_back(realtimePriority);

        AppActionParameter connectOutputs;
        connectOutputs.name = "connect_outputs";
        connectOutputs.label = "Connect Outputs";
        connectOutputs.type = AppActionParameterType::Boolean;
        connectOutputs.required = false;
        connectOutputs.defaultValue = "true";
        connectOutputs.description = "Auto-connect runtime outputs to system playback.";
        schema.parameters.push_back(connectOutputs);
    } else if (actionId == "audio.runtime.devices") {
        AppActionParameter backend;
        backend.name = "backend";
        backend.label = "Backend";
        backend.type = AppActionParameterType::Choice;
        backend.required = false;
        backend.choices = {"auto", "pipewire", "jack", "alsa", "dummy"};
        backend.description = "Optional backend filter for the device list.";
        schema.parameters.push_back(backend);

        AppActionParameter onlyAvailable;
        onlyAvailable.name = "only_available";
        onlyAvailable.label = "Only Available";
        onlyAvailable.type = AppActionParameterType::Boolean;
        onlyAvailable.required = false;
        onlyAvailable.defaultValue = "false";
        onlyAvailable.description = "When true, only available devices are returned.";
        schema.parameters.push_back(onlyAvailable);
    } else if (actionId == "audio.runtime.render_test") {
        AppActionParameter frameCount;
        frameCount.name = "frame_count";
        frameCount.label = "Frame Count";
        frameCount.type = AppActionParameterType::Integer;
        frameCount.required = false;
        frameCount.defaultValue = "256";
        frameCount.description = "Frames per render callback block.";
        schema.parameters.push_back(frameCount);

        AppActionParameter blockCount;
        blockCount.name = "block_count";
        blockCount.label = "Block Count";
        blockCount.type = AppActionParameterType::Integer;
        blockCount.required = false;
        blockCount.defaultValue = "4";
        blockCount.description = "How many blocks to render for this diagnostic test.";
        schema.parameters.push_back(blockCount);
    } else if (actionId == "audio.runtime.simulate_underrun") {
        AppActionParameter detail;
        detail.name = "detail";
        detail.label = "Detail";
        detail.type = AppActionParameterType::Text;
        detail.required = false;
        detail.description = "Optional diagnostic detail for the simulated underrun.";
        schema.parameters.push_back(detail);
    } else if (actionId == "task.cancel") {
        AppActionParameter taskId;
        taskId.name = "task_id";
        taskId.label = "Task ID";
        taskId.type = AppActionParameterType::Integer;
        taskId.required = true;
        taskId.description = "Identifier of the task to cancel.";
        schema.parameters.push_back(taskId);

        AppActionParameter message;
        message.name = "message";
        message.label = "Message";
        message.type = AppActionParameterType::Text;
        message.required = false;
        message.defaultValue = "Canceled";
        message.description = "Optional cancel message shown in task history.";
        schema.parameters.push_back(message);
    } else if (actionId == "session.events") {
        AppActionParameter since;
        since.name = "since";
        since.label = "Since";
        since.type = AppActionParameterType::Integer;
        since.required = false;
        since.defaultValue = "0";
        since.description = "Return events with sequence greater than this value.";
        schema.parameters.push_back(since);

        AppActionParameter drain;
        drain.name = "drain";
        drain.label = "Drain";
        drain.type = AppActionParameterType::Boolean;
        drain.required = false;
        drain.defaultValue = "false";
        drain.description = "When true, drain queued events instead of filtering by sequence.";
        schema.parameters.push_back(drain);

        AppActionParameter maxEvents;
        maxEvents.name = "max_events";
        maxEvents.label = "Max Events";
        maxEvents.type = AppActionParameterType::Integer;
        maxEvents.required = false;
        maxEvents.hasMinimum = true;
        maxEvents.minimum = 1.0;
        maxEvents.description = "Optional hard limit for returned events.";
        schema.parameters.push_back(maxEvents);

        AppActionParameter includeSnapshot;
        includeSnapshot.name = "include_snapshot";
        includeSnapshot.label = "Include Snapshot";
        includeSnapshot.type = AppActionParameterType::Boolean;
        includeSnapshot.required = false;
        includeSnapshot.defaultValue = "false";
        includeSnapshot.description = "Include a full session snapshot with the event response.";
        schema.parameters.push_back(includeSnapshot);

        AppActionParameter snapshotGridStartRow;
        snapshotGridStartRow.name = "snapshot_grid_start_row";
        snapshotGridStartRow.label = "Snapshot Grid Start Row";
        snapshotGridStartRow.type = AppActionParameterType::Integer;
        snapshotGridStartRow.required = false;
        snapshotGridStartRow.defaultValue = "0";
        snapshotGridStartRow.hasMinimum = true;
        snapshotGridStartRow.minimum = 0.0;
        snapshotGridStartRow.description = "Grid start row used when including a snapshot.";
        schema.parameters.push_back(snapshotGridStartRow);

        AppActionParameter snapshotGridRowCount;
        snapshotGridRowCount.name = "snapshot_grid_row_count";
        snapshotGridRowCount.label = "Snapshot Grid Row Count";
        snapshotGridRowCount.type = AppActionParameterType::Integer;
        snapshotGridRowCount.required = false;
        snapshotGridRowCount.defaultValue = "-1";
        snapshotGridRowCount.description = "Grid row count used when including a snapshot (-1 uses defaults).";
        schema.parameters.push_back(snapshotGridRowCount);
    } else if (actionId == "session.sync") {
        AppActionParameter checkpointName;
        checkpointName.name = "checkpoint_name";
        checkpointName.label = "Checkpoint Name";
        checkpointName.type = AppActionParameterType::Text;
        checkpointName.required = true;
        checkpointName.description = "Named sync checkpoint to resume from.";
        schema.parameters.push_back(checkpointName);

        AppActionParameter mode;
        mode.name = "mode";
        mode.label = "Mode";
        mode.type = AppActionParameterType::Choice;
        mode.required = false;
        mode.defaultValue = "delta_with_fallback";
        mode.choices = {"delta_only", "delta_with_fallback", "force_snapshot"};
        mode.description = "Controls snapshot behavior during sync.";
        schema.parameters.push_back(mode);

        AppActionParameter stalePolicy;
        stalePolicy.name = "stale_policy";
        stalePolicy.label = "Stale Policy";
        stalePolicy.type = AppActionParameterType::Choice;
        stalePolicy.required = false;
        stalePolicy.defaultValue = "snapshot_fallback";
        stalePolicy.choices = {"snapshot_fallback", "error", "ignore"};
        stalePolicy.description = "Behavior when checkpoint compatibility is stale.";
        schema.parameters.push_back(stalePolicy);

        AppActionParameter maxEvents;
        maxEvents.name = "max_events";
        maxEvents.label = "Max Events";
        maxEvents.type = AppActionParameterType::Integer;
        maxEvents.required = false;
        maxEvents.hasMinimum = true;
        maxEvents.minimum = 1.0;
        maxEvents.description = "Optional hard limit for returned events.";
        schema.parameters.push_back(maxEvents);

        AppActionParameter snapshotGridStartRow;
        snapshotGridStartRow.name = "snapshot_grid_start_row";
        snapshotGridStartRow.label = "Snapshot Grid Start Row";
        snapshotGridStartRow.type = AppActionParameterType::Integer;
        snapshotGridStartRow.required = false;
        snapshotGridStartRow.defaultValue = "0";
        snapshotGridStartRow.hasMinimum = true;
        snapshotGridStartRow.minimum = 0.0;
        snapshotGridStartRow.description = "Grid start row for optional snapshot payloads.";
        schema.parameters.push_back(snapshotGridStartRow);

        AppActionParameter snapshotGridRowCount;
        snapshotGridRowCount.name = "snapshot_grid_row_count";
        snapshotGridRowCount.label = "Snapshot Grid Row Count";
        snapshotGridRowCount.type = AppActionParameterType::Integer;
        snapshotGridRowCount.required = false;
        snapshotGridRowCount.defaultValue = "-1";
        snapshotGridRowCount.description = "Grid row count for optional snapshot payloads (-1 uses defaults).";
        schema.parameters.push_back(snapshotGridRowCount);

        AppActionParameter updateCheckpoint;
        updateCheckpoint.name = "update_checkpoint";
        updateCheckpoint.label = "Update Checkpoint";
        updateCheckpoint.type = AppActionParameterType::Boolean;
        updateCheckpoint.required = false;
        updateCheckpoint.defaultValue = "true";
        updateCheckpoint.description = "Advance checkpoint when the sync response is safe to commit.";
        schema.parameters.push_back(updateCheckpoint);
    
        AppActionParameter createIfMissing;
        createIfMissing.name = "create_if_missing";
        createIfMissing.label = "Create If Missing";
        createIfMissing.type = AppActionParameterType::Boolean;
        createIfMissing.required = false;
        createIfMissing.defaultValue = "false";
        createIfMissing.description = "Create the checkpoint when it does not exist yet.";
        schema.parameters.push_back(createIfMissing);
    } else if (actionId == "session.snapshot") {
        AppActionParameter gridStartRow;
        gridStartRow.name = "grid_start_row";
        gridStartRow.label = "Grid Start Row";
        gridStartRow.type = AppActionParameterType::Integer;
        gridStartRow.required = false;
        gridStartRow.defaultValue = "0";
        gridStartRow.hasMinimum = true;
        gridStartRow.minimum = 0.0;
        gridStartRow.description = "First row shown in the active pattern grid.";
        schema.parameters.push_back(gridStartRow);

        AppActionParameter gridRowCount;
        gridRowCount.name = "grid_row_count";
        gridRowCount.label = "Grid Row Count";
        gridRowCount.type = AppActionParameterType::Integer;
        gridRowCount.required = false;
        gridRowCount.defaultValue = "-1";
        gridRowCount.description = "Visible grid row count (-1 uses default layout settings).";
        schema.parameters.push_back(gridRowCount);
    } else if (actionId == "session.checkpoint.save") {
        AppActionParameter name;
        name.name = "name";
        name.label = "Name";
        name.type = AppActionParameterType::Text;
        name.required = true;
        name.description = "Stable checkpoint key used by the frontend.";
        schema.parameters.push_back(name);

        AppActionParameter eventSequence;
        eventSequence.name = "event_sequence";
        eventSequence.label = "Event Sequence";
        eventSequence.type = AppActionParameterType::Integer;
        eventSequence.required = false;
        eventSequence.hasMinimum = true;
        eventSequence.minimum = 0.0;
        eventSequence.description = "Last processed event sequence. Defaults to current last event sequence.";
        schema.parameters.push_back(eventSequence);

        AppActionParameter taskId;
        taskId.name = "task_id";
        taskId.label = "Task ID";
        taskId.type = AppActionParameterType::Integer;
        taskId.required = false;
        taskId.hasMinimum = true;
        taskId.minimum = 0.0;
        taskId.description = "Last processed task id. Defaults to the latest known task id.";
        schema.parameters.push_back(taskId);

        AppActionParameter maxCheckpoints;
        maxCheckpoints.name = "max_checkpoints";
        maxCheckpoints.label = "Max Checkpoints";
        maxCheckpoints.type = AppActionParameterType::Integer;
        maxCheckpoints.required = false;
        maxCheckpoints.defaultValue = "64";
        maxCheckpoints.hasMinimum = true;
        maxCheckpoints.minimum = 1.0;
        maxCheckpoints.description = "Maximum number of stored checkpoints.";
        schema.parameters.push_back(maxCheckpoints);
    } else if (actionId == "session.checkpoint.advance") {
        AppActionParameter name;
        name.name = "name";
        name.label = "Name";
        name.type = AppActionParameterType::Text;
        name.required = true;
        name.description = "Stable checkpoint key used by the frontend.";
        schema.parameters.push_back(name);

        AppActionParameter eventSequence;
        eventSequence.name = "event_sequence";
        eventSequence.label = "Event Sequence";
        eventSequence.type = AppActionParameterType::Integer;
        eventSequence.required = true;
        eventSequence.hasMinimum = true;
        eventSequence.minimum = 0.0;
        eventSequence.description = "New checkpoint event sequence to commit.";
        schema.parameters.push_back(eventSequence);

        AppActionParameter taskId;
        taskId.name = "task_id";
        taskId.label = "Task ID";
        taskId.type = AppActionParameterType::Integer;
        taskId.required = false;
        taskId.hasMinimum = true;
        taskId.minimum = 0.0;
        taskId.description = "Optional task id to persist with the checkpoint.";
        schema.parameters.push_back(taskId);

        AppActionParameter expectedEventSequence;
        expectedEventSequence.name = "expected_event_sequence";
        expectedEventSequence.label = "Expected Event Sequence";
        expectedEventSequence.type = AppActionParameterType::Integer;
        expectedEventSequence.required = false;
        expectedEventSequence.hasMinimum = true;
        expectedEventSequence.minimum = 0.0;
        expectedEventSequence.description = "Optional compare-and-swap guard against the current checkpoint event sequence.";
        schema.parameters.push_back(expectedEventSequence);

        AppActionParameter expectedTaskId;
        expectedTaskId.name = "expected_task_id";
        expectedTaskId.label = "Expected Task ID";
        expectedTaskId.type = AppActionParameterType::Integer;
        expectedTaskId.required = false;
        expectedTaskId.hasMinimum = true;
        expectedTaskId.minimum = 0.0;
        expectedTaskId.description = "Optional compare-and-swap guard against the current checkpoint task id.";
        schema.parameters.push_back(expectedTaskId);

        AppActionParameter maxCheckpoints;
        maxCheckpoints.name = "max_checkpoints";
        maxCheckpoints.label = "Max Checkpoints";
        maxCheckpoints.type = AppActionParameterType::Integer;
        maxCheckpoints.required = false;
        maxCheckpoints.defaultValue = "64";
        maxCheckpoints.hasMinimum = true;
        maxCheckpoints.minimum = 1.0;
        maxCheckpoints.description = "Maximum number of stored checkpoints.";
        schema.parameters.push_back(maxCheckpoints);
    } else if (actionId == "session.checkpoint.load" || actionId == "session.checkpoint.clear") {
        AppActionParameter name;
        name.name = "name";
        name.label = "Name";
        name.type = AppActionParameterType::Text;
        name.required = true;
        name.description = "Stable checkpoint key used by the frontend.";
        schema.parameters.push_back(name);
    } else if (actionId == "session.checkpoint.list") {
        // no parameters
    } else if (actionId == "export.stems") {
        AppActionParameter format;
        format.name = "format";
        format.label = "Format";
        format.type = AppActionParameterType::Choice;
        format.required = false;
        format.defaultValue = "wav";
        format.choices = {"wav", "mp3", "ogg"};
        format.description = "Stem audio format.";
        schema.parameters.push_back(format);
    } else if (actionId == "export.midi") {
        AppActionParameter ticks;
        ticks.name = "ticks_per_quarter";
        ticks.label = "Ticks Per Quarter";
        ticks.type = AppActionParameterType::Integer;
        ticks.required = false;
        ticks.defaultValue = "480";
        ticks.hasMinimum = true;
        ticks.minimum = 1.0;
        ticks.description = "MIDI resolution in ticks per quarter note.";
        schema.parameters.push_back(ticks);
    } else if (actionId == "import.midi") {
        AppActionParameter rowsPerBeat;
        rowsPerBeat.name = "rows_per_beat";
        rowsPerBeat.label = "Rows Per Beat";
        rowsPerBeat.type = AppActionParameterType::Integer;
        rowsPerBeat.required = false;
        rowsPerBeat.defaultValue = "4";
        rowsPerBeat.hasMinimum = true;
        rowsPerBeat.minimum = 1.0;
        rowsPerBeat.description = "Tracker rows per beat used by the imported song.";
        schema.parameters.push_back(rowsPerBeat);

        AppActionParameter patternRows;
        patternRows.name = "pattern_rows";
        patternRows.label = "Pattern Rows";
        patternRows.type = AppActionParameterType::Integer;
        patternRows.required = false;
        patternRows.defaultValue = "64";
        patternRows.hasMinimum = true;
        patternRows.minimum = 1.0;
        patternRows.description = "Pattern length used to split the imported arrangement.";
        schema.parameters.push_back(patternRows);

        AppActionParameter splitByTrack;
        splitByTrack.name = "split_by_track";
        splitByTrack.label = "Split By Track";
        splitByTrack.type = AppActionParameterType::Boolean;
        splitByTrack.required = false;
        splitByTrack.defaultValue = "true";
        splitByTrack.description = "When true, import lanes are split by MIDI track and channel.";
        schema.parameters.push_back(splitByTrack);

        AppActionParameter splitByProgram;
        splitByProgram.name = "split_by_program";
        splitByProgram.label = "Split By Program";
        splitByProgram.type = AppActionParameterType::Boolean;
        splitByProgram.required = false;
        splitByProgram.defaultValue = "true";
        splitByProgram.description = "When true, separate imported lanes by MIDI program so each program gets its own patch.";
        schema.parameters.push_back(splitByProgram);

        AppActionParameter preserveTempoMap;
        preserveTempoMap.name = "preserve_tempo_map";
        preserveTempoMap.label = "Preserve Tempo Map";
        preserveTempoMap.type = AppActionParameterType::Boolean;
        preserveTempoMap.required = false;
        preserveTempoMap.defaultValue = "true";
        preserveTempoMap.description = "When true, convert MIDI tempo changes into row timing so playback speed matches the source song.";
        schema.parameters.push_back(preserveTempoMap);
    } else if (actionId == "script.import") {
        AppActionParameter nameOverride;
        nameOverride.name = "name_override";
        nameOverride.label = "Name Override";
        nameOverride.type = AppActionParameterType::Text;
        nameOverride.required = false;
        nameOverride.description = "Optional instrument name override when importing a patch.";
        schema.parameters.push_back(nameOverride);
    }
    return schema;
}

AppActionValidationResult validateAppActionParameters(
    const std::string& actionId,
    const std::map<std::string, std::string>& parameters) {
    AppActionValidationResult result;
    const AppActionSchema schema = buildAppActionSchema(actionId);
    if (schema.actionId.empty() || (schema.label.empty() && schema.parameters.empty())) {
        result.error = "unknown action";
        return result;
    }

    for (const AppActionParameter& parameter : schema.parameters) {
        validateParameterValue(result, parameter, parameterValue(parameters, parameter.name));
    }

    result.ok = result.missingParameters.empty() && result.fieldErrors.empty();
    if (!result.ok) {
        result.error = result.missingParameters.empty()
            ? "invalid action parameters"
            : "missing required action parameters";
    }
    return result;
}

AppActionCommandBuild buildCommandForAppAction(
    const std::string& actionId,
    const std::map<std::string, std::string>& parameters) {
    AppActionCommandBuild result;
    const std::string commandTemplate = commandTemplateForAction(actionId);
    if (commandTemplate.empty()) {
        result.error = "action has no editor command template";
        return result;
    }

    std::ostringstream command;
    bool first = true;
    const std::vector<std::string> tokens = splitTemplate(commandTemplate);
    const std::vector<AppActionParameter> schemaParameters =
        parametersFromCommandTemplate(actionId.rfind("editor.", 0) == 0 ? actionId.substr(7) : actionId, commandTemplate);
    const AppActionValidationResult validation = validateAppActionParameters(actionId, parameters);
    if (!validation.ok) {
        result.error = validation.error;
        result.missingParameters = validation.missingParameters;
        return result;
    }

    std::size_t parameterIndex = 0;
    for (const std::string& token : tokens) {
        std::string value = token;
        if (isRequiredToken(token) || isOptionalToken(token)) {
            if (parameterIndex >= schemaParameters.size()) {
                result.error = "command template parameter mismatch";
                return result;
            }
            const AppActionParameter& parameter = schemaParameters[parameterIndex++];
            if (!hasParameter(parameters, parameter.name)) {
                if (parameter.required) {
                    result.missingParameters.push_back(parameter.name);
                }
                continue;
            }
            value = parameterValue(parameters, parameter.name);
        }

        if (!first) {
            command << " ";
        }
        command << value;
        first = false;
    }

    if (!result.missingParameters.empty()) {
        result.error = "missing required action parameters";
        return result;
    }

    result.ok = true;
    result.commandText = command.str();
    return result;
}

std::vector<AppActionState> buildApplicationActionStates(const ApplicationSession& session) {
    std::vector<AppActionState> states;
    bool hasFinishedTasks = false;
    for (const AppTaskSnapshot& task : session.taskManager().tasks()) {
        if (isAppTaskFinished(task.state)) {
            hasFinishedTasks = true;
            break;
        }
    }
    for (const AppAction& action : applicationActions()) {
        if (action.id == "project.save" && session.projectPath().empty()) {
            states.push_back(disabledState(action.id, "project has not been saved yet"));
        } else if ((action.id == "playback.play"
                       || action.id == "playback.play_pattern"
                       || action.id == "playback.play_song")
            && session.playback().snapshot().state == TransportState::Playing) {
            states.push_back(disabledState(action.id, "playback is already running"));
        } else if (action.id == "playback.pause" && session.playback().snapshot().state != TransportState::Playing) {
            states.push_back(disabledState(action.id, "playback is not running"));
        } else if (action.id == "audio.runtime.start" && session.audioRuntimeHealth().active) {
            states.push_back(disabledState(action.id, "audio runtime is already active"));
        } else if (action.id == "audio.runtime.stop" && !session.audioRuntimeHealth().active) {
            states.push_back(disabledState(action.id, "audio runtime is not active"));
        } else if (action.id == "task.cancel" && !session.taskManager().hasActiveTasks()) {
            states.push_back(disabledState(action.id, "no active tasks"));
        } else if (action.id == "task.clear_finished" && !hasFinishedTasks) {
            states.push_back(disabledState(action.id, "no finished tasks"));
        } else {
            states.push_back(enabledState(action.id));
        }
    }
    return states;
}

std::vector<AppActionEntry> buildApplicationActionPalette(
    const ApplicationSession& session,
    const std::string& query) {
    const std::string normalizedQuery = lowerCopy(query);
    const std::map<std::string, AppActionState> appStates = stateById(session);
    const std::vector<CommandPaletteEntry> editorEntries = buildEditorCommandPalette(
        session.editor(),
        effectiveEditorShortcuts(session.settings()),
        query);

    std::vector<AppActionEntry> entries;
    for (const AppAction& action : applicationActions()) {
        if (!actionMatches(
                action.id,
                action.label,
                action.category,
                action.defaultShortcut,
                action.description,
                normalizedQuery)) {
            continue;
        }

        AppActionEntry entry;
        entry.id = action.id;
        entry.label = action.label;
        entry.category = action.category;
        entry.shortcut = action.defaultShortcut;
        entry.description = action.description;
        entry.kind = action.kind;
        entry.mutatesProject = action.mutatesProject;
        entry.requiresPath = action.requiresPath;
        entry.requiresCommandText = action.requiresCommandText;
        entry.parameterCount = static_cast<int>(buildAppActionSchema(action.id).parameters.size());

        const auto stateIt = appStates.find(action.id);
        if (stateIt != appStates.end()) {
            entry.enabled = stateIt->second.enabled;
            entry.disabledReason = stateIt->second.disabledReason;
        }
        entries.push_back(entry);
    }

    for (const CommandPaletteEntry& editorEntry : editorEntries) {
        AppActionEntry entry;
        entry.id = "editor." + editorEntry.actionId;
        entry.label = editorEntry.label;
        entry.category = editorEntry.category;
        entry.shortcut = editorEntry.shortcut;
        entry.description = editorEntry.description;
        entry.kind = AppActionKind::Editor;
        entry.enabled = editorEntry.enabled;
        entry.disabledReason = editorEntry.disabledReason;
        entry.mutatesProject = editorEntry.mutatesProject;
        entry.requiresCommandText = commandTemplateNeedsText(editorEntry.command);
        entry.parameterCount = static_cast<int>(
            buildAppActionSchema("editor." + editorEntry.actionId).parameters.size());
        entries.push_back(entry);
    }

    std::stable_sort(entries.begin(), entries.end(), [](const AppActionEntry& left, const AppActionEntry& right) {
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

AppActionResult executeAppAction(ApplicationSession& session, const AppActionRequest& request) {
    AppActionResult result;
    result.actionId = request.actionId;
    result.hasMessageSeverity = true;
    result.messageSeverity = AppMessageSeverity::Info;

    if (request.actionId.rfind("editor.", 0) == 0) {
        const std::string editorActionId = request.actionId.substr(7);
        const EditorAction* action = findEditorAction(editorActionId);
        if (action == nullptr) {
            result.error = "unknown editor action";
            result.messageSeverity = AppMessageSeverity::Error;
            return result;
        }
        if (!isEditorActionEnabled(session.editor(), editorActionId)) {
            result.error = "editor action is disabled";
            result.messageSeverity = AppMessageSeverity::Error;
            return result;
        }
        std::string commandText = request.commandText;
        if (commandText.empty()) {
            const AppActionCommandBuild commandBuild = buildCommandForAppAction(
                request.actionId,
                request.parameters);
            if (!commandBuild.ok) {
                if (commandTemplateNeedsText(action->command)) {
                    if (request.parameters.empty()) {
                        result.error = "editor action requires concrete command text";
                    } else {
                        result.error = commandBuild.error.empty()
                            ? "invalid action parameters"
                            : commandBuild.error;
                    }
                    result.messageSeverity = AppMessageSeverity::Error;
                    return result;
                }
            } else {
                commandText = commandBuild.commandText;
            }
        }
        if (commandText.empty() && commandTemplateNeedsText(action->command)) {
            result.error = "editor action requires concrete command text";
            result.messageSeverity = AppMessageSeverity::Error;
            return result;
        }

        const EditorCommandResult commandResult = session.applyEditorCommand(
            commandText.empty() ? action->command : commandText);
        result.ok = commandResult.ok;
        result.message = commandResult.message;
        result.error = commandResult.error;
        result.projectChanged = commandResult.projectChanged;
        result.editorStateChanged = commandResult.editorStateChanged;
        result.messageSeverity = commandResult.ok ? session.lastMessage().severity : AppMessageSeverity::Error;
        return result;
    }

    const AppAction* action = findApplicationAction(request.actionId);
    if (action == nullptr) {
        result.error = "unknown application action";
        result.messageSeverity = AppMessageSeverity::Error;
        return result;
    }

    const std::vector<AppActionState> states = buildApplicationActionStates(session);
    const auto stateIt = std::find_if(states.begin(), states.end(), [&request](const AppActionState& state) {
        return state.id == request.actionId;
    });
    if (stateIt != states.end() && !stateIt->enabled) {
        result.error = stateIt->disabledReason;
        result.messageSeverity = AppMessageSeverity::Error;
        return result;
    }
    const std::string requestPath = request.path.empty() ? parameterValue(request.parameters, "path") : request.path;
    if (action->requiresPath && requestPath.empty()) {
        result.error = "action requires a path";
        result.messageSeverity = AppMessageSeverity::Error;
        return result;
    }
    std::map<std::string, std::string> parametersForValidation = request.parameters;
    if (action->requiresPath && !requestPath.empty() && !hasParameter(parametersForValidation, "path")) {
        parametersForValidation["path"] = requestPath;
    }
    if (!parametersForValidation.empty()) {
        const AppActionValidationResult validation = validateAppActionParameters(
            request.actionId,
            parametersForValidation);
        if (!validation.ok) {
            result.error = validation.error;
            result.messageSeverity = AppMessageSeverity::Error;
            return result;
        }
    }

    if (request.actionId == "project.new"
        || request.actionId == "project.open"
        || request.actionId == "project.close"
        || request.actionId == "application.quit"
        || request.actionId == "recovery.restore") {
        result.lifecyclePlan = planProjectLifecycleTransition(
            session,
            lifecycleActionForAppAction(request.actionId),
            request.unsavedChoice,
            requestPath,
            request.recoveryDirectory);
        result.requiresUnsavedDecision = result.lifecyclePlan.requiresUnsavedDecision && !result.lifecyclePlan.canProceed;
        result.requiresSaveAs = result.lifecyclePlan.requiresSaveAs;
        result.shouldOfferRecovery = result.lifecyclePlan.shouldOfferRecovery;
        if (!result.lifecyclePlan.canProceed) {
            result.message = result.lifecyclePlan.message;
            result.error = result.lifecyclePlan.error;
            result.messageSeverity = result.error.empty() ? AppMessageSeverity::Info : AppMessageSeverity::Error;
            return result;
        }
        if (result.lifecyclePlan.shouldSaveBeforeProceeding) {
            const AppOperationResult saveResult = session.saveProjectFile();
            if (!saveResult.ok) {
                result.ok = false;
                result.error = saveResult.error;
                result.message = saveResult.message;
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
        }
    }

    if (request.actionId == "project.new") {
        copyOperationResult(result, session.newProject(makeDemoSong()), true);
    } else if (request.actionId == "project.open") {
        copyOperationResult(result, session.loadProjectFile(requestPath), true);
        result.shouldOfferRecovery = result.lifecyclePlan.shouldOfferRecovery;
    } else if (request.actionId == "project.save") {
        copyOperationResult(result, session.saveProjectFile(), false);
    } else if (request.actionId == "project.save_as") {
        copyOperationResult(result, session.saveProjectFileAs(requestPath), false);
    } else if (request.actionId == "project.close") {
        copyOperationResult(result, session.newProject(makeDemoSong()), true);
        if (result.ok) {
            result.message = "Closed project";
        }
    } else if (request.actionId == "application.quit") {
        result.ok = true;
        result.message = "Ready to quit";
    } else if (request.actionId == "playback.play" || request.actionId == "playback.play_song") {
        session.playback().clearLoop();
        session.playback().seekRows(0.0);
        session.playback().play();
        result.ok = true;
        result.message = "Song playback started";
        result.editorStateChanged = true;
    } else if (request.actionId == "playback.play_pattern") {
        Song& song = session.song();
        if (song.patterns.empty()) {
            result.error = "no patterns available";
            result.messageSeverity = AppMessageSeverity::Error;
            return result;
        }

        const int activePattern = std::clamp(
            session.editor().cursor().pattern,
            0,
            static_cast<int>(song.patterns.size()) - 1);
        std::optional<std::pair<int, int>> range = firstOrderRangeForPattern(song, activePattern);
        bool appendedToOrder = false;
        if (!range.has_value()) {
            try {
                session.editor().appendOrder(activePattern);
                appendedToOrder = true;
            } catch (const std::exception& error) {
                result.error = error.what();
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
            range = firstOrderRangeForPattern(song, activePattern);
        }
        if (!range.has_value() || range->second <= range->first) {
            result.error = "unable to locate active pattern in arrangement order";
            result.messageSeverity = AppMessageSeverity::Error;
            return result;
        }

        session.playback().setLoopRows(static_cast<double>(range->first), static_cast<double>(range->second));
        session.playback().seekRows(static_cast<double>(range->first));
        session.playback().play();
        result.ok = true;
        result.projectChanged = appendedToOrder;
        result.editorStateChanged = true;
        if (appendedToOrder) {
            result.message = "Pattern playback started (pattern added to order)";
        } else {
            result.message = "Pattern playback started";
        }
    } else if (request.actionId == "playback.pause") {
        session.playback().pause();
        result.ok = true;
        result.message = "Playback paused";
        result.editorStateChanged = true;
    } else if (request.actionId == "playback.stop") {
        session.playback().stop();
        result.ok = true;
        result.message = "Playback stopped";
        result.editorStateChanged = true;
    } else if (request.actionId == "audio.runtime.configure") {
        AudioRuntimeSettings runtime = audioRuntimeSettingsFromPreferences(session.settings().audioRuntime);
        const std::string backend = parameterValue(request.parameters, "backend");
        if (!backend.empty()) {
            runtime.backend = audioBackendFromName(backend);
        }
        const std::string deviceId = parameterValue(request.parameters, "device_id");
        if (!deviceId.empty()) {
            runtime.deviceId = deviceId;
        }
        const std::string sampleRate = parameterValue(request.parameters, "sample_rate");
        if (!sampleRate.empty()) {
            int parsed = 0;
            if (!parseInteger(sampleRate, parsed)) {
                result.error = "sample_rate must be an integer";
                return result;
            }
            runtime.sampleRate = parsed;
        }
        const std::string bufferFrames = parameterValue(request.parameters, "buffer_frames");
        if (!bufferFrames.empty()) {
            int parsed = 0;
            if (!parseInteger(bufferFrames, parsed)) {
                result.error = "buffer_frames must be an integer";
                return result;
            }
            runtime.bufferFrames = parsed;
        }
        const std::string periods = parameterValue(request.parameters, "periods");
        if (!periods.empty()) {
            int parsed = 0;
            if (!parseInteger(periods, parsed)) {
                result.error = "periods must be an integer";
                return result;
            }
            runtime.periods = parsed;
        }
        const std::string realtimePriority = parameterValue(request.parameters, "realtime_priority");
        if (!realtimePriority.empty()) {
            bool parsed = false;
            if (!parseBoolean(realtimePriority, parsed)) {
                result.error = "realtime_priority must be true or false";
                return result;
            }
            runtime.realtimePriority = parsed;
        }
        const std::string connectOutputs = parameterValue(request.parameters, "connect_outputs");
        if (!connectOutputs.empty()) {
            bool parsed = false;
            if (!parseBoolean(connectOutputs, parsed)) {
                result.error = "connect_outputs must be true or false";
                return result;
            }
            runtime.connectSystemOutputs = parsed;
        }
        const AppOperationResult configured = session.configureAudioRuntimeWithTask(runtime);
        result.ok = configured.ok;
        result.message = configured.message;
        result.error = configured.error;
        result.editorStateChanged = true;
    } else if (request.actionId == "audio.runtime.start") {
        copyOperationResult(result, session.startAudioRuntimeWithTask(), false);
        result.hasAudioRuntime = true;
        result.audioRuntime = session.audioRuntimeHealth();
        result.editorStateChanged = result.ok;
    } else if (request.actionId == "audio.runtime.stop") {
        copyOperationResult(result, session.stopAudioRuntimeWithTask(), false);
        result.hasAudioRuntime = true;
        result.audioRuntime = session.audioRuntimeHealth();
        result.editorStateChanged = result.ok;
    } else if (request.actionId == "audio.runtime.status") {
        result.ok = true;
        result.message = "Audio runtime status";
        result.hasAudioRuntime = true;
        result.audioRuntime = session.audioRuntimeHealth();
    } else if (request.actionId == "audio.runtime.devices") {
        result.ok = true;
        result.message = "Audio devices";
        const std::string backendFilter = lowerCopy(parameterValue(request.parameters, "backend"));
        bool onlyAvailable = false;
        const std::string onlyAvailableText = parameterValue(request.parameters, "only_available");
        if (!onlyAvailableText.empty()) {
            if (!parseBoolean(onlyAvailableText, onlyAvailable)) {
                result.error = "only_available must be true or false";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
        }

        for (const AudioDeviceInfo& device : session.audioRuntimeDevices()) {
            if (!backendFilter.empty()
                && backendFilter != "auto"
                && backendFilter != lowerCopy(audioBackendName(device.backend))) {
                continue;
            }
            if (onlyAvailable && !device.available) {
                continue;
            }
            result.audioDevices.push_back(device);
        }
        result.hasAudioDevices = true;
        result.hasAudioRuntime = true;
        result.audioRuntime = session.audioRuntimeHealth();
    } else if (request.actionId == "audio.runtime.render_test") {
        int frameCount = session.settings().audioRuntime.bufferFrames;
        int blockCount = 4;
        const std::string frameCountText = parameterValue(request.parameters, "frame_count");
        if (!frameCountText.empty()) {
            if (!parseInteger(frameCountText, frameCount)) {
                result.error = "frame_count must be an integer";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
        }
        const std::string blockCountText = parameterValue(request.parameters, "block_count");
        if (!blockCountText.empty()) {
            if (!parseInteger(blockCountText, blockCount)) {
                result.error = "block_count must be an integer";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
        }
        const AudioRuntimeRenderTestResult renderTest = session.renderAudioRuntimeTestWithTask(frameCount, blockCount);
        result.ok = renderTest.ok;
        result.message = renderTest.message;
        result.error = renderTest.error;
        result.hasAudioRuntime = true;
        result.audioRuntime = session.audioRuntimeHealth();
        result.hasAudioRenderTest = true;
        result.audioRenderFrameCount = renderTest.frameCount;
        result.audioRenderBlockCount = renderTest.blockCount;
        result.audioRenderProcessedFrames = renderTest.processedFrames;
        result.audioRenderProcessedBlocks = renderTest.processedBlocks;
        result.audioRenderUnderrunsBefore = renderTest.underrunsBefore;
        result.audioRenderUnderrunsAfter = renderTest.underrunsAfter;
        result.editorStateChanged = result.ok;
    } else if (request.actionId == "audio.runtime.simulate_underrun") {
        const AppOperationResult simulated = session.simulateAudioUnderrunWithTask(
            parameterValue(request.parameters, "detail"));
        copyOperationResult(result, simulated, false);
        result.hasAudioRuntime = true;
        result.audioRuntime = session.audioRuntimeHealth();
        result.editorStateChanged = result.ok;
    } else if (request.actionId == "task.cancel") {
        int taskId = 0;
        if (!parseInteger(parameterValue(request.parameters, "task_id"), taskId)) {
            result.error = "task_id must be an integer";
            result.messageSeverity = AppMessageSeverity::Error;
            return result;
        }
        copyOperationResult(result, session.cancelTask(taskId, parameterValue(request.parameters, "message")), false);
    } else if (request.actionId == "task.clear_finished") {
        copyOperationResult(result, session.clearFinishedTasks(), false);
    } else if (request.actionId == "session.snapshot") {
        int gridStartRow = 0;
        const std::string gridStartRowText = parameterValue(request.parameters, "grid_start_row");
        if (!gridStartRowText.empty()) {
            if (!parseInteger(gridStartRowText, gridStartRow) || gridStartRow < 0) {
                result.error = "grid_start_row must be a non-negative integer";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
        }

        int gridRowCount = -1;
        const std::string gridRowCountText = parameterValue(request.parameters, "grid_row_count");
        if (!gridRowCountText.empty()) {
            if (!parseInteger(gridRowCountText, gridRowCount)) {
                result.error = "grid_row_count must be an integer";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
            if (gridRowCount == 0 || gridRowCount < -1) {
                result.error = "grid_row_count must be -1 or a positive integer";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
        }

        result.ok = true;
        result.message = "Session snapshot";
        result.hasSessionSnapshot = true;
        result.sessionSnapshot = session.snapshot(gridStartRow, gridRowCount);
        result.hasLastEventSequence = true;
        result.lastEventSequence = session.lastEventSequence();
    } else if (request.actionId == "session.events") {
        std::uint64_t since = 0;
        const std::string sinceText = parameterValue(request.parameters, "since");
        if (!sinceText.empty()) {
            int parsed = 0;
            if (!parseInteger(sinceText, parsed) || parsed < 0) {
                result.error = "since must be a non-negative integer";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
            since = static_cast<std::uint64_t>(parsed);
        }

        bool drain = false;
        const std::string drainText = parameterValue(request.parameters, "drain");
        if (!drainText.empty()) {
            if (!parseBoolean(drainText, drain)) {
                result.error = "drain must be true or false";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
        }

        int maxEvents = 0;
        const std::string maxEventsText = parameterValue(request.parameters, "max_events");
        if (!maxEventsText.empty()) {
            if (!parseInteger(maxEventsText, maxEvents) || maxEvents <= 0) {
                result.error = "max_events must be a positive integer";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
        }

        bool includeSnapshot = false;
        const std::string includeSnapshotText = parameterValue(request.parameters, "include_snapshot");
        if (!includeSnapshotText.empty()) {
            if (!parseBoolean(includeSnapshotText, includeSnapshot)) {
                result.error = "include_snapshot must be true or false";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
        }

        int snapshotGridStartRow = 0;
        const std::string snapshotGridStartRowText = parameterValue(request.parameters, "snapshot_grid_start_row");
        if (!snapshotGridStartRowText.empty()) {
            if (!parseInteger(snapshotGridStartRowText, snapshotGridStartRow) || snapshotGridStartRow < 0) {
                result.error = "snapshot_grid_start_row must be a non-negative integer";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
        }

        int snapshotGridRowCount = -1;
        const std::string snapshotGridRowCountText = parameterValue(request.parameters, "snapshot_grid_row_count");
        if (!snapshotGridRowCountText.empty()) {
            if (!parseInteger(snapshotGridRowCountText, snapshotGridRowCount)) {
                result.error = "snapshot_grid_row_count must be an integer";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
            if (snapshotGridRowCount == 0 || snapshotGridRowCount < -1) {
                result.error = "snapshot_grid_row_count must be -1 or a positive integer";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
        }

        result.ok = true;
        result.message = "Session events";
        result.hasEvents = true;
        result.events = drain ? session.drainEvents() : session.eventsSince(since);
        const int totalBeforeLimit = static_cast<int>(result.events.size());
        if (maxEvents > 0 && static_cast<int>(result.events.size()) > maxEvents) {
            result.events.resize(static_cast<std::size_t>(maxEvents));
        }
        result.hasEventDeltaSummary = true;
        result.eventDeltaSummary = summarizeEvents(
            result.events,
            totalBeforeLimit,
            totalBeforeLimit - static_cast<int>(result.events.size()));
        if (includeSnapshot || result.eventDeltaSummary.truncated) {
            result.hasSessionSnapshot = true;
            result.sessionSnapshot = session.snapshot(snapshotGridStartRow, snapshotGridRowCount);
        }
        result.hasLastEventSequence = true;
        result.lastEventSequence = session.lastEventSequence();
        populateEventCursor(result, since, drain);
    } else if (request.actionId == "session.sync") {
        const std::string checkpointName = parameterValue(request.parameters, "checkpoint_name");
        if (checkpointName.empty()) {
            result.error = "checkpoint_name is required";
            result.messageSeverity = AppMessageSeverity::Error;
            return result;
        }

        std::string mode = lowerCopy(parameterValue(request.parameters, "mode"));
        if (mode.empty()) {
            mode = "delta_with_fallback";
        }
        if (mode != "delta_only" && mode != "delta_with_fallback" && mode != "force_snapshot") {
            result.error = "mode must be delta_only, delta_with_fallback, or force_snapshot";
            result.messageSeverity = AppMessageSeverity::Error;
            return result;
        }

        std::string stalePolicy = lowerCopy(parameterValue(request.parameters, "stale_policy"));
        if (stalePolicy.empty()) {
            stalePolicy = "snapshot_fallback";
        }
        if (stalePolicy != "snapshot_fallback" && stalePolicy != "error" && stalePolicy != "ignore") {
            result.error = "stale_policy must be snapshot_fallback, error, or ignore";
            result.messageSeverity = AppMessageSeverity::Error;
            return result;
        }

        int maxEvents = 0;
        const std::string maxEventsText = parameterValue(request.parameters, "max_events");
        if (!maxEventsText.empty()) {
            if (!parseInteger(maxEventsText, maxEvents) || maxEvents <= 0) {
                result.error = "max_events must be a positive integer";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
        }

        int snapshotGridStartRow = 0;
        const std::string snapshotGridStartRowText = parameterValue(request.parameters, "snapshot_grid_start_row");
        if (!snapshotGridStartRowText.empty()) {
            if (!parseInteger(snapshotGridStartRowText, snapshotGridStartRow) || snapshotGridStartRow < 0) {
                result.error = "snapshot_grid_start_row must be a non-negative integer";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
        }

        int snapshotGridRowCount = -1;
        const std::string snapshotGridRowCountText = parameterValue(request.parameters, "snapshot_grid_row_count");
        if (!snapshotGridRowCountText.empty()) {
            if (!parseInteger(snapshotGridRowCountText, snapshotGridRowCount)) {
                result.error = "snapshot_grid_row_count must be an integer";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
            if (snapshotGridRowCount == 0 || snapshotGridRowCount < -1) {
                result.error = "snapshot_grid_row_count must be -1 or a positive integer";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
        }

        bool updateCheckpoint = true;
        const std::string updateCheckpointText = parameterValue(request.parameters, "update_checkpoint");
        if (!updateCheckpointText.empty()) {
            if (!parseBoolean(updateCheckpointText, updateCheckpoint)) {
                result.error = "update_checkpoint must be true or false";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
        }

        bool createIfMissing = false;
        const std::string createIfMissingText = parameterValue(request.parameters, "create_if_missing");
        if (!createIfMissingText.empty()) {
            if (!parseBoolean(createIfMissingText, createIfMissing)) {
                result.error = "create_if_missing must be true or false";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
        }

        result.hasSyncCheckpointUpdateStatus = true;
        const SyncCheckpoint* checkpoint = findSyncCheckpoint(session.settings(), checkpointName);
        if (checkpoint == nullptr && createIfMissing) {
            SyncCheckpoint created;
            created.name = checkpointName;
            created.eventSequence = session.lastEventSequence();
            const std::vector<AppTaskSnapshot> tasks = session.taskManager().tasks();
            if (!tasks.empty()) {
                created.taskId = tasks.back().id;
            }
            populateSyncCheckpointIdentity(created, session);
            const AppOperationResult saved = session.saveSyncCheckpoint(created);
            if (!saved.ok) {
                result.syncCheckpointUpdateStatus = SyncCheckpointUpdateStatus::Failed;
                result.syncCheckpointUpdateReason = saved.error;
                result.error = saved.error;
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
            checkpoint = findSyncCheckpoint(session.settings(), checkpointName);
        }
        if (checkpoint == nullptr) {
            result.hasSyncCheckpointCompatibility = true;
            result.syncCheckpointCompatibility = SyncCheckpointCompatibility::Missing;
            result.syncCheckpointUpdateStatus = SyncCheckpointUpdateStatus::Failed;
            result.syncCheckpointUpdateReason = "sync checkpoint not found";
            result.error = "sync checkpoint not found";
            result.messageSeverity = AppMessageSeverity::Error;
            return result;
        }
        result.hasSyncCheckpoint = true;
        result.syncCheckpoint = *checkpoint;
        result.hasSyncCheckpointCompatibility = true;
        std::string staleReason;
        result.syncCheckpointCompatibility = inspectSyncCheckpointCompatibility(*checkpoint, session, staleReason);
        if (result.syncCheckpointCompatibility == SyncCheckpointCompatibility::StalePath
            || result.syncCheckpointCompatibility == SyncCheckpointCompatibility::StaleFingerprint) {
            result.syncCheckpointStale = true;
            result.syncCheckpointStaleReason = staleReason;
        }
        if (result.syncCheckpointStale && stalePolicy == "error") {
            result.syncCheckpointUpdateStatus = SyncCheckpointUpdateStatus::SkippedStalePolicy;
            result.syncCheckpointUpdateReason = "stale checkpoint blocked by stale_policy=error";
            result.error = "sync checkpoint is stale: " + staleReason;
            result.messageSeverity = AppMessageSeverity::Error;
            return result;
        }

        result.ok = true;
        result.message = "Session sync";
        result.hasEvents = true;
        if (!result.syncCheckpointStale || stalePolicy == "ignore") {
            result.events = session.eventsSince(checkpoint->eventSequence);
        } else {
            result.events.clear();
        }

        const int totalBeforeLimit = static_cast<int>(result.events.size());
        if (maxEvents > 0 && static_cast<int>(result.events.size()) > maxEvents) {
            result.events.resize(static_cast<std::size_t>(maxEvents));
        }
        result.hasEventDeltaSummary = true;
        result.eventDeltaSummary = summarizeEvents(
            result.events,
            totalBeforeLimit,
            totalBeforeLimit - static_cast<int>(result.events.size()));
        result.hasLastEventSequence = true;
        result.lastEventSequence = session.lastEventSequence();

        const bool includeSnapshot = (result.syncCheckpointStale && stalePolicy == "snapshot_fallback")
            || mode == "force_snapshot"
            || (mode == "delta_with_fallback" && result.eventDeltaSummary.truncated);
        if (includeSnapshot) {
            result.hasSessionSnapshot = true;
            result.sessionSnapshot = session.snapshot(snapshotGridStartRow, snapshotGridRowCount);
        }
        populateEventCursor(result, checkpoint->eventSequence, false);

        const bool safeToAdvance = result.syncCheckpointStale
            || !result.eventDeltaSummary.truncated
            || mode != "delta_only";
        result.hasSuggestedSyncCheckpoint = true;
        result.suggestedSyncCheckpoint = *checkpoint;
        result.suggestedSyncCheckpointSafeToCommit = safeToAdvance;
        if (safeToAdvance) {
            result.suggestedSyncCheckpoint.eventSequence = result.lastEventSequence;
            const std::vector<AppTaskSnapshot> tasks = session.taskManager().tasks();
            if (!tasks.empty()) {
                result.suggestedSyncCheckpoint.taskId = tasks.back().id;
            }
            populateSyncCheckpointIdentity(result.suggestedSyncCheckpoint, session);
        }

        if (updateCheckpoint) {
            if (safeToAdvance) {
                const AppOperationResult saved = session.saveSyncCheckpoint(result.suggestedSyncCheckpoint);
                if (saved.ok) {
                    result.syncCheckpointUpdateStatus = SyncCheckpointUpdateStatus::Advanced;
                    const SyncCheckpoint* updated = findSyncCheckpoint(session.settings(), checkpointName);
                    if (updated != nullptr) {
                        result.syncCheckpoint = *updated;
                    }
                } else {
                    result.syncCheckpointUpdateStatus = SyncCheckpointUpdateStatus::Failed;
                    result.syncCheckpointUpdateReason = saved.error;
                }
            } else {
                result.syncCheckpointUpdateStatus = SyncCheckpointUpdateStatus::SkippedUnsafeDelta;
                result.syncCheckpointUpdateReason =
                    "delta_only sync was truncated; checkpoint not advanced to avoid dropping unseen events";
            }
        } else {
            result.syncCheckpointUpdateStatus = SyncCheckpointUpdateStatus::SkippedUpdateDisabled;
            result.syncCheckpointUpdateReason = "update_checkpoint=false";
        }
    } else if (request.actionId == "session.checkpoint.list") {
        result.ok = true;
        result.message = "Listed sync checkpoints";
        result.hasSyncCheckpoints = true;
        result.syncCheckpoints = session.settings().syncCheckpoints;
    } else if (request.actionId == "session.checkpoint.save") {
        const std::string name = parameterValue(request.parameters, "name");
        if (name.empty()) {
            result.error = "name is required";
            result.messageSeverity = AppMessageSeverity::Error;
            return result;
        }

        std::uint64_t eventSequence = session.lastEventSequence();
        const std::string eventSequenceText = parameterValue(request.parameters, "event_sequence");
        if (!eventSequenceText.empty()) {
            int parsed = 0;
            if (!parseInteger(eventSequenceText, parsed) || parsed < 0) {
                result.error = "event_sequence must be a non-negative integer";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
            eventSequence = static_cast<std::uint64_t>(parsed);
        }

        int taskId = 0;
        const std::vector<AppTaskSnapshot> tasks = session.taskManager().tasks();
        if (!tasks.empty()) {
            taskId = tasks.back().id;
        }
        const std::string taskIdText = parameterValue(request.parameters, "task_id");
        if (!taskIdText.empty()) {
            if (!parseInteger(taskIdText, taskId) || taskId < 0) {
                result.error = "task_id must be a non-negative integer";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
        }

        int maxCheckpoints = 64;
        const std::string maxCheckpointsText = parameterValue(request.parameters, "max_checkpoints");
        if (!maxCheckpointsText.empty()) {
            if (!parseInteger(maxCheckpointsText, maxCheckpoints) || maxCheckpoints <= 0) {
                result.error = "max_checkpoints must be a positive integer";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
        }

        SyncCheckpoint checkpoint;
        checkpoint.name = name;
        checkpoint.eventSequence = eventSequence;
        checkpoint.taskId = taskId;
        populateSyncCheckpointIdentity(checkpoint, session);
        copyOperationResult(result, session.saveSyncCheckpoint(checkpoint, maxCheckpoints), false);
        if (result.ok) {
            const SyncCheckpoint* stored = findSyncCheckpoint(session.settings(), name);
            if (stored != nullptr) {
                result.hasSyncCheckpoint = true;
                result.syncCheckpoint = *stored;
                result.hasSyncCheckpointCompatibility = true;
                result.syncCheckpointCompatibility = SyncCheckpointCompatibility::Ok;
            }
        }
    } else if (request.actionId == "session.checkpoint.advance") {
        const std::string name = parameterValue(request.parameters, "name");
        if (name.empty()) {
            result.error = "name is required";
            result.messageSeverity = AppMessageSeverity::Error;
            return result;
        }

        int eventSequenceValue = 0;
        if (!parseInteger(parameterValue(request.parameters, "event_sequence"), eventSequenceValue) || eventSequenceValue < 0) {
            result.error = "event_sequence must be a non-negative integer";
            result.messageSeverity = AppMessageSeverity::Error;
            return result;
        }
        const std::uint64_t eventSequence = static_cast<std::uint64_t>(eventSequenceValue);

        int taskId = 0;
        const std::vector<AppTaskSnapshot> tasks = session.taskManager().tasks();
        if (!tasks.empty()) {
            taskId = tasks.back().id;
        }
        const std::string taskIdText = parameterValue(request.parameters, "task_id");
        if (!taskIdText.empty()) {
            if (!parseInteger(taskIdText, taskId) || taskId < 0) {
                result.error = "task_id must be a non-negative integer";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
        }

        bool hasExpectedEventSequence = false;
        int expectedEventSequence = 0;
        const std::string expectedEventSequenceText = parameterValue(request.parameters, "expected_event_sequence");
        if (!expectedEventSequenceText.empty()) {
            if (!parseInteger(expectedEventSequenceText, expectedEventSequence) || expectedEventSequence < 0) {
                result.error = "expected_event_sequence must be a non-negative integer";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
            hasExpectedEventSequence = true;
        }

        bool hasExpectedTaskId = false;
        int expectedTaskId = 0;
        const std::string expectedTaskIdText = parameterValue(request.parameters, "expected_task_id");
        if (!expectedTaskIdText.empty()) {
            if (!parseInteger(expectedTaskIdText, expectedTaskId) || expectedTaskId < 0) {
                result.error = "expected_task_id must be a non-negative integer";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
            hasExpectedTaskId = true;
        }

        int maxCheckpoints = 64;
        const std::string maxCheckpointsText = parameterValue(request.parameters, "max_checkpoints");
        if (!maxCheckpointsText.empty()) {
            if (!parseInteger(maxCheckpointsText, maxCheckpoints) || maxCheckpoints <= 0) {
                result.error = "max_checkpoints must be a positive integer";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
        }

        const SyncCheckpoint* current = findSyncCheckpoint(session.settings(), name);
        if (current == nullptr) {
            result.hasSyncCheckpointCompatibility = true;
            result.syncCheckpointCompatibility = SyncCheckpointCompatibility::Missing;
            result.hasSyncCheckpointUpdateStatus = true;
            result.syncCheckpointUpdateStatus = SyncCheckpointUpdateStatus::Failed;
            result.syncCheckpointUpdateReason = "sync checkpoint not found";
            result.error = "sync checkpoint not found";
            result.messageSeverity = AppMessageSeverity::Error;
            return result;
        }

        result.hasSyncCheckpoint = true;
        result.syncCheckpoint = *current;
        result.hasSyncCheckpointCompatibility = true;
        std::string staleReason;
        result.syncCheckpointCompatibility = inspectSyncCheckpointCompatibility(*current, session, staleReason);
        if (result.syncCheckpointCompatibility == SyncCheckpointCompatibility::StalePath
            || result.syncCheckpointCompatibility == SyncCheckpointCompatibility::StaleFingerprint) {
            result.syncCheckpointStale = true;
            result.syncCheckpointStaleReason = staleReason;
        }

        if (hasExpectedEventSequence && current->eventSequence != static_cast<std::uint64_t>(expectedEventSequence)) {
            result.hasSyncCheckpointUpdateStatus = true;
            result.syncCheckpointUpdateStatus = SyncCheckpointUpdateStatus::Failed;
            result.syncCheckpointUpdateReason = "expected_event_sequence mismatch";
            result.error = "sync checkpoint advance conflict: expected_event_sequence mismatch";
            result.messageSeverity = AppMessageSeverity::Error;
            return result;
        }
        if (hasExpectedTaskId && current->taskId != expectedTaskId) {
            result.hasSyncCheckpointUpdateStatus = true;
            result.syncCheckpointUpdateStatus = SyncCheckpointUpdateStatus::Failed;
            result.syncCheckpointUpdateReason = "expected_task_id mismatch";
            result.error = "sync checkpoint advance conflict: expected_task_id mismatch";
            result.messageSeverity = AppMessageSeverity::Error;
            return result;
        }

        SyncCheckpoint advanced = *current;
        advanced.eventSequence = eventSequence;
        advanced.taskId = taskId;
        populateSyncCheckpointIdentity(advanced, session);
        const AppOperationResult saved = session.saveSyncCheckpoint(advanced, maxCheckpoints);
        if (!saved.ok) {
            result.hasSyncCheckpointUpdateStatus = true;
            result.syncCheckpointUpdateStatus = SyncCheckpointUpdateStatus::Failed;
            result.syncCheckpointUpdateReason = saved.error;
            result.error = saved.error;
            result.messageSeverity = AppMessageSeverity::Error;
            return result;
        }

        const SyncCheckpoint* updated = findSyncCheckpoint(session.settings(), name);
        result.ok = true;
        result.message = "Advanced sync checkpoint";
        result.hasSyncCheckpointUpdateStatus = true;
        result.syncCheckpointUpdateStatus = SyncCheckpointUpdateStatus::Advanced;
        result.hasSyncCheckpointCompatibility = true;
        result.syncCheckpointCompatibility = SyncCheckpointCompatibility::Ok;
        if (updated != nullptr) {
            result.hasSyncCheckpoint = true;
            result.syncCheckpoint = *updated;
        }
    } else if (request.actionId == "session.checkpoint.load") {
        const std::string name = parameterValue(request.parameters, "name");
        if (name.empty()) {
            result.error = "name is required";
            result.messageSeverity = AppMessageSeverity::Error;
            return result;
        }
        const SyncCheckpoint* checkpoint = findSyncCheckpoint(session.settings(), name);
        if (checkpoint == nullptr) {
            result.hasSyncCheckpointCompatibility = true;
            result.syncCheckpointCompatibility = SyncCheckpointCompatibility::Missing;
            result.error = "sync checkpoint not found";
            result.messageSeverity = AppMessageSeverity::Error;
            return result;
        }
        result.ok = true;
        result.message = "Loaded sync checkpoint";
        result.hasSyncCheckpoint = true;
        result.syncCheckpoint = *checkpoint;
        result.hasSyncCheckpointCompatibility = true;
        std::string staleReason;
        result.syncCheckpointCompatibility = inspectSyncCheckpointCompatibility(*checkpoint, session, staleReason);
        if (result.syncCheckpointCompatibility == SyncCheckpointCompatibility::StalePath
            || result.syncCheckpointCompatibility == SyncCheckpointCompatibility::StaleFingerprint) {
            result.syncCheckpointStale = true;
            result.syncCheckpointStaleReason = staleReason;
        }
    } else if (request.actionId == "session.checkpoint.clear") {
        copyOperationResult(result, session.clearSyncCheckpoint(parameterValue(request.parameters, "name")), false);
    } else if (request.actionId == "export.mixdown") {
        ExportRequest exportRequest = mixdownExportRequest(requestPath);
        const ExportResult exported = session.exportProjectWithTask(exportRequest);
        result.ok = exported.ok;
        result.message = exported.message;
        result.error = exported.error;
        result.hasExportResult = true;
        result.exportResult = exported;
    } else if (request.actionId == "export.stems") {
        ExportFormat format = ExportFormat::Wav;
        const std::string formatText = parameterValue(request.parameters, "format");
        if (!formatText.empty() && !parseExportFormatText(formatText, format)) {
            result.error = "format must be wav, mp3, or ogg";
            result.messageSeverity = AppMessageSeverity::Error;
            return result;
        }
        const ExportRequest exportRequest = stemExportRequest(requestPath, format);
        const ExportResult exported = session.exportProjectWithTask(exportRequest);
        result.ok = exported.ok;
        result.message = exported.message;
        result.error = exported.error;
        result.hasExportResult = true;
        result.exportResult = exported;
    } else if (request.actionId == "export.midi") {
        int ticksPerQuarter = 480;
        const std::string ticksText = parameterValue(request.parameters, "ticks_per_quarter");
        if (!ticksText.empty()) {
            if (!parseInteger(ticksText, ticksPerQuarter) || ticksPerQuarter <= 0) {
                result.error = "ticks_per_quarter must be a positive integer";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
        }
        const ExportRequest exportRequest = midiExportRequest(requestPath, ticksPerQuarter);
        const ExportResult exported = session.exportProjectWithTask(exportRequest);
        result.ok = exported.ok;
        result.message = exported.message;
        result.error = exported.error;
        result.hasExportResult = true;
        result.exportResult = exported;
    } else if (request.actionId == "import.midi") {
        MidiImportOptions options;
        const std::string rowsPerBeatText = parameterValue(request.parameters, "rows_per_beat");
        if (!rowsPerBeatText.empty()) {
            if (!parseInteger(rowsPerBeatText, options.rowsPerBeat) || options.rowsPerBeat <= 0) {
                result.error = "rows_per_beat must be a positive integer";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
        }
        const std::string patternRowsText = parameterValue(request.parameters, "pattern_rows");
        if (!patternRowsText.empty()) {
            if (!parseInteger(patternRowsText, options.patternRows) || options.patternRows <= 0) {
                result.error = "pattern_rows must be a positive integer";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
        }
        const std::string splitByTrackText = parameterValue(request.parameters, "split_by_track");
        if (!splitByTrackText.empty()) {
            bool parsed = true;
            if (!parseBoolean(splitByTrackText, parsed)) {
                result.error = "split_by_track must be true or false";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
            options.splitByTrack = parsed;
        }
        const std::string splitByProgramText = parameterValue(request.parameters, "split_by_program");
        if (!splitByProgramText.empty()) {
            bool parsed = true;
            if (!parseBoolean(splitByProgramText, parsed)) {
                result.error = "split_by_program must be true or false";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
            options.splitByProgram = parsed;
        }
        const std::string preserveTempoMapText = parameterValue(request.parameters, "preserve_tempo_map");
        if (!preserveTempoMapText.empty()) {
            bool parsed = true;
            if (!parseBoolean(preserveTempoMapText, parsed)) {
                result.error = "preserve_tempo_map must be true or false";
                result.messageSeverity = AppMessageSeverity::Error;
                return result;
            }
            options.preserveTempoMap = parsed;
        }

        MidiImportReport importReport;
        const AppOperationResult imported = session.importMidiFile(requestPath, options, &importReport);
        copyOperationResult(result, imported, true);
        if (result.ok) {
            result.hasMidiImportReport = true;
            result.midiImportReport = std::move(importReport);
        }
    } else if (request.actionId == "script.import") {
        const ScriptImportResult imported = session.importScriptArtifact(
            requestPath,
            parameterValue(request.parameters, "name_override"));
        result.ok = imported.ok;
        result.message = imported.message;
        result.error = imported.error;
        result.projectChanged = imported.ok && imported.type != ScriptArtifactType::Unknown;
        result.editorStateChanged = imported.ok;
        result.hasScriptImportResult = true;
        result.scriptImportResult = imported;
    } else if (request.actionId == "preview.cursor") {
        const AuditionResult audition = session.auditionCursorStep();
        result.ok = audition.ok;
        result.message = audition.message;
        result.error = audition.error;
        result.editorStateChanged = true;
    } else if (request.actionId == "recovery.save") {
        const RecoveryResult recovery = session.saveRecoverySnapshot(requestPath);
        result.ok = recovery.ok;
        result.message = recovery.message;
        result.error = recovery.error;
    } else if (request.actionId == "recovery.restore") {
        const RecoveryResult recovery = session.restoreRecoverySnapshot(requestPath);
        result.ok = recovery.ok;
        result.message = recovery.message;
        result.error = recovery.error;
        result.projectChanged = recovery.ok;
        result.editorStateChanged = recovery.ok;
    } else if (request.actionId == "recovery.clear") {
        const RecoveryResult recovery = session.clearRecoverySnapshot(requestPath);
        result.ok = recovery.ok;
        result.message = recovery.message;
        result.error = recovery.error;
    }

    if (!result.ok) {
        result.messageSeverity = AppMessageSeverity::Error;
    } else if (request.actionId == "application.quit") {
        result.messageSeverity = AppMessageSeverity::Info;
    } else if (request.actionId == "session.events"
        || request.actionId == "session.snapshot"
        || request.actionId == "session.sync"
        || request.actionId == "session.checkpoint.advance"
        || request.actionId == "session.checkpoint.load"
        || request.actionId == "session.checkpoint.list") {
        result.messageSeverity = AppMessageSeverity::Info;
    } else {
        result.messageSeverity = session.lastMessage().severity;
    }

    return result;
}

std::string renderApplicationActionPalette(const std::vector<AppActionEntry>& entries) {
    std::ostringstream out;
    out << "Application actions\n";
    out << std::left
        << std::setw(14) << "Category"
        << std::setw(24) << "Label"
        << std::setw(18) << "Shortcut"
        << std::setw(10) << "State"
        << "ID\n";

    for (const AppActionEntry& entry : entries) {
        out << std::left
            << std::setw(14) << entry.category
            << std::setw(24) << entry.label
            << std::setw(18) << (entry.shortcut.empty() ? "-" : entry.shortcut)
            << std::setw(10) << (entry.enabled ? "enabled" : "disabled")
            << entry.id;
        if (entry.requiresPath) {
            out << " path";
        }
        if (entry.requiresCommandText) {
            out << " command";
        }
        if (entry.parameterCount > 0) {
            out << " params=" << entry.parameterCount;
        }
        if (!entry.disabledReason.empty()) {
            out << " (" << entry.disabledReason << ")";
        }
        out << "\n";
    }
    return out.str();
}

std::string renderAppActionSchema(const AppActionSchema& schema) {
    std::ostringstream out;
    out << "Action schema: " << schema.actionId << "\n";
    out << "kind: " << appActionKindName(schema.kind) << "\n";
    if (!schema.label.empty()) {
        out << "label: " << schema.label << "\n";
    }
    if (!schema.commandTemplate.empty()) {
        out << "command: " << schema.commandTemplate << "\n";
    }
    for (const AppActionParameter& parameter : schema.parameters) {
        out << "- " << parameter.name
            << " type=" << appActionParameterTypeName(parameter.type)
            << " required=" << (parameter.required ? "yes" : "no");
        if (!parameter.defaultValue.empty()) {
            out << " default=" << parameter.defaultValue;
        }
        if (parameter.hasMinimum || parameter.hasMaximum) {
            out << " range=";
            out << (parameter.hasMinimum ? std::to_string(parameter.minimum) : "-inf");
            out << "..";
            out << (parameter.hasMaximum ? std::to_string(parameter.maximum) : "inf");
        }
        if (!parameter.choices.empty()) {
            out << " choices=";
            for (std::size_t index = 0; index < parameter.choices.size(); ++index) {
                if (index > 0) {
                    out << ",";
                }
                out << parameter.choices[index];
            }
        }
        out << "\n";
    }
    return out.str();
}

std::string renderApplicationActionResult(const AppActionResult& result) {
    std::ostringstream out;
    out << "Action result: " << result.actionId << "\n";
    out << "ok: " << (result.ok ? "yes" : "no") << "\n";
    if (result.hasMessageSeverity) {
        out << "severity: " << appMessageSeverityName(result.messageSeverity) << "\n";
    }
    if (!result.message.empty()) {
        out << "message: " << result.message << "\n";
    }
    if (!result.error.empty()) {
        out << "error: " << result.error << "\n";
    }
    if (result.hasAudioRuntime) {
        out << "audio: backend=" << audioBackendName(result.audioRuntime.backend)
            << " device=" << (result.audioRuntime.deviceId.empty() ? "-" : result.audioRuntime.deviceId)
            << " active=" << (result.audioRuntime.active ? "yes" : "no")
            << " latency_ms=" << result.audioRuntime.estimatedLatencyMs
            << " underruns=" << result.audioRuntime.underrunCount << "\n";
    }
    if (result.hasAudioDevices) {
        out << "audio_devices: " << result.audioDevices.size() << "\n";
        for (const AudioDeviceInfo& device : result.audioDevices) {
            out << "- " << audioBackendName(device.backend)
                << " id=" << device.id
                << " name=" << device.name
                << " available=" << (device.available ? "yes" : "no");
            if (!device.status.empty()) {
                out << " status=" << device.status;
            }
            out << "\n";
        }
    }
    if (result.hasAudioRenderTest) {
        out << "audio_render_test: frame_count=" << result.audioRenderFrameCount
            << " block_count=" << result.audioRenderBlockCount
            << " processed_frames=" << result.audioRenderProcessedFrames
            << " processed_blocks=" << result.audioRenderProcessedBlocks
            << " underruns_before=" << result.audioRenderUnderrunsBefore
            << " underruns_after=" << result.audioRenderUnderrunsAfter << "\n";
    }
    if (result.hasSessionSnapshot) {
        out << "session_snapshot: project_path="
            << (result.sessionSnapshot.hasProjectPath ? result.sessionSnapshot.projectPath : "-")
            << " dirty=" << (result.sessionSnapshot.dirty ? "yes" : "no")
            << " patterns=" << result.sessionSnapshot.editor.patterns.size()
            << " tracks=" << result.sessionSnapshot.editor.tracks.size()
            << " instruments=" << result.sessionSnapshot.editor.instruments.size()
            << " tasks=" << result.sessionSnapshot.tasks.size()
            << " active_tasks=" << result.sessionSnapshot.activeTaskCount << "\n";
    }
    if (result.hasSyncCheckpoint) {
        out << "sync_checkpoint: name=" << result.syncCheckpoint.name
            << " event_sequence=" << result.syncCheckpoint.eventSequence
            << " task_id=" << result.syncCheckpoint.taskId << "\n";
    }
    if (result.hasSuggestedSyncCheckpoint) {
        out << "suggested_sync_checkpoint: name=" << result.suggestedSyncCheckpoint.name
            << " event_sequence=" << result.suggestedSyncCheckpoint.eventSequence
            << " task_id=" << result.suggestedSyncCheckpoint.taskId
            << " safe_to_commit=" << (result.suggestedSyncCheckpointSafeToCommit ? "yes" : "no") << "\n";
    }
    if (result.hasSyncCheckpointCompatibility) {
        out << "sync_checkpoint_compatibility: "
            << syncCheckpointCompatibilityName(result.syncCheckpointCompatibility) << "\n";
    }
    if (result.syncCheckpointStale) {
        out << "sync_checkpoint_stale: yes";
        if (!result.syncCheckpointStaleReason.empty()) {
            out << " reason=" << result.syncCheckpointStaleReason;
        }
        out << "\n";
    }
    if (result.hasSyncCheckpointUpdateStatus) {
        out << "sync_checkpoint_update: " << syncCheckpointUpdateStatusName(result.syncCheckpointUpdateStatus);
        if (!result.syncCheckpointUpdateReason.empty()) {
            out << " reason=" << result.syncCheckpointUpdateReason;
        }
        out << "\n";
    }
    if (result.hasSyncCheckpoints) {
        out << "sync_checkpoints: " << result.syncCheckpoints.size() << "\n";
        for (const SyncCheckpoint& checkpoint : result.syncCheckpoints) {
            out << "- name=" << checkpoint.name
                << " event_sequence=" << checkpoint.eventSequence
                << " task_id=" << checkpoint.taskId;
            if (!checkpoint.projectPath.empty()) {
                out << " project_path=" << checkpoint.projectPath;
            }
            out << "\n";
        }
    }
    if (result.hasExportResult) {
        out << "export_result: target=" << exportTargetName(result.exportResult.target)
            << " files=" << result.exportResult.files.size()
            << " ok=" << (result.exportResult.ok ? "yes" : "no");
        if (!result.exportResult.error.empty()) {
            out << " error=" << result.exportResult.error;
        }
        out << "\n";
        for (const ExportedFile& file : result.exportResult.files) {
            out << "- file=" << file.path
                << " sample_rate=" << file.sampleRate
                << " frames=" << file.frameCount << "\n";
        }
    }
    if (result.hasScriptImportResult) {
        out << "script_import_result: type=" << scriptArtifactTypeName(result.scriptImportResult.type)
            << " path=" << result.scriptImportResult.path
            << " instrument=" << result.scriptImportResult.importedInstrument
            << " commands=" << result.scriptImportResult.appliedCommandCount << "\n";
    }
    if (result.hasMidiImportReport) {
        out << "midi_import_report:"
            << " format=" << result.midiImportReport.midiFormat
            << " ppq=" << result.midiImportReport.ticksPerQuarterNote
            << " notes=" << result.midiImportReport.importedNoteCount
            << " tracks=" << result.midiImportReport.importedTrackCount
            << " mappings=" << result.midiImportReport.trackMappings.size() << "\n";
        for (const MidiImportTrackMapping& mapping : result.midiImportReport.trackMappings) {
            out << "- track " << mapping.trackIndex
                << " \"" << mapping.trackName << "\""
                << " -> instrument " << mapping.instrumentIndex
                << " \"" << mapping.instrumentName << "\""
                << " src_track=" << mapping.midiSourceTrack
                << " channel=" << (mapping.midiChannel + 1)
                << " program=" << (mapping.dominantProgram + 1) << "\n";
        }
    }
    if (result.hasEvents) {
        out << "events: " << result.events.size() << "\n";
        for (const AppEvent& event : result.events) {
            out << "- #" << event.sequence
                << " type=" << appEventTypeName(event.type)
                << " dirty=" << (event.dirty ? "yes" : "no");
            if (event.taskId > 0) {
                out << " task=" << event.taskId;
            }
            if (!event.path.empty()) {
                out << " path=" << event.path;
            }
            if (!event.message.empty()) {
                out << " message=" << event.message;
            }
            out << "\n";
        }
    }
    if (result.hasEventDeltaSummary) {
        const AppActionResult::EventDeltaSummary& delta = result.eventDeltaSummary;
        out << "event_delta: total=" << delta.total
            << " returned=" << delta.returned
            << " dropped=" << delta.dropped
            << " truncated=" << (delta.truncated ? "yes" : "no")
            << " domains="
            << "project:" << delta.project << ","
            << "settings:" << delta.settings << ","
            << "editor:" << delta.editor << ","
            << "diagnostics:" << delta.diagnostics << ","
            << "playback:" << delta.playback << ","
            << "audio:" << delta.audio << ","
            << "message:" << delta.message << ","
            << "tasks:" << delta.tasks << ","
            << "script:" << delta.script << ","
            << "recovery:" << delta.recovery << ","
            << "other:" << delta.other << "\n";
    }
    if (result.hasEventCursor) {
        out << "event_cursor: requested_since=" << result.eventCursor.requestedSince
            << " recommended_since=" << result.eventCursor.recommendedSince
            << " truncated=" << (result.eventCursor.truncated ? "yes" : "no")
            << " includes_snapshot=" << (result.eventCursor.includesSnapshot ? "yes" : "no")
            << " drain=" << (result.eventCursor.drain ? "yes" : "no");
        if (result.eventCursor.hasReturnedRange) {
            out << " returned_range=" << result.eventCursor.returnedFrom << ".." << result.eventCursor.returnedTo;
        }
        out << "\n";
    }
    if (result.hasLastEventSequence) {
        out << "last_event_sequence: " << result.lastEventSequence << "\n";
    }
    return out.str();
}

std::string serializeApplicationActionResult(const AppActionResult& result) {
    std::ostringstream out;
    out << "{";
    out << "\"action_id\":\"" << escapeJson(result.actionId) << "\",";
    out << "\"ok\":" << (result.ok ? "true" : "false") << ",";
    out << "\"message\":\"" << escapeJson(result.message) << "\",";
    out << "\"error\":\"" << escapeJson(result.error) << "\",";
    out << "\"severity\":\"";
    out << (result.hasMessageSeverity ? appMessageSeverityName(result.messageSeverity) : "unknown");
    out << "\",";
    out << "\"project_changed\":" << (result.projectChanged ? "true" : "false") << ",";
    out << "\"editor_state_changed\":" << (result.editorStateChanged ? "true" : "false") << ",";
    out << "\"audio_runtime\":{";
    out << "\"present\":" << (result.hasAudioRuntime ? "true" : "false");
    if (result.hasAudioRuntime) {
        out << ",\"backend\":\"" << audioBackendName(result.audioRuntime.backend) << "\"";
        out << ",\"device_id\":\"" << escapeJson(result.audioRuntime.deviceId) << "\"";
        out << ",\"active\":" << (result.audioRuntime.active ? "true" : "false");
        out << ",\"latency_ms\":" << result.audioRuntime.estimatedLatencyMs;
        out << ",\"underruns\":" << result.audioRuntime.underrunCount;
    }
    out << "},";
    out << "\"audio_devices\":[";
    for (std::size_t index = 0; index < result.audioDevices.size(); ++index) {
        const AudioDeviceInfo& device = result.audioDevices[index];
        if (index > 0) {
            out << ",";
        }
        out << "{";
        out << "\"backend\":\"" << audioBackendName(device.backend) << "\",";
        out << "\"id\":\"" << escapeJson(device.id) << "\",";
        out << "\"name\":\"" << escapeJson(device.name) << "\",";
        out << "\"available\":" << (device.available ? "true" : "false") << ",";
        out << "\"status\":\"" << escapeJson(device.status) << "\"";
        out << "}";
    }
    out << "],";
    out << "\"audio_render_test\":{";
    out << "\"present\":" << (result.hasAudioRenderTest ? "true" : "false");
    if (result.hasAudioRenderTest) {
        out << ",\"frame_count\":" << result.audioRenderFrameCount;
        out << ",\"block_count\":" << result.audioRenderBlockCount;
        out << ",\"processed_frames\":" << result.audioRenderProcessedFrames;
        out << ",\"processed_blocks\":" << result.audioRenderProcessedBlocks;
        out << ",\"underruns_before\":" << result.audioRenderUnderrunsBefore;
        out << ",\"underruns_after\":" << result.audioRenderUnderrunsAfter;
    }
    out << "},";
    out << "\"session_snapshot\":{";
    out << "\"present\":" << (result.hasSessionSnapshot ? "true" : "false");
    if (result.hasSessionSnapshot) {
        const AppSessionSnapshot& snapshot = result.sessionSnapshot;
        out << ",\"project_path\":\"" << escapeJson(snapshot.projectPath) << "\"";
        out << ",\"has_project_path\":" << (snapshot.hasProjectPath ? "true" : "false");
        out << ",\"dirty\":" << (snapshot.dirty ? "true" : "false");
        out << ",\"last_message\":{";
        out << "\"severity\":\"" << appMessageSeverityName(snapshot.lastMessage.severity) << "\",";
        out << "\"text\":\"" << escapeJson(snapshot.lastMessage.text) << "\"";
        out << "}";
        out << ",\"diagnostics\":{";
        out << "\"has_errors\":" << (snapshot.hasDiagnosticErrors ? "true" : "false") << ",";
        out << "\"error_count\":" << snapshot.diagnosticErrorCount << ",";
        out << "\"warning_count\":" << snapshot.diagnosticWarningCount << ",";
        out << "\"total\":" << snapshot.diagnostics.size();
        out << "}";
        out << ",\"editor\":{";
        out << "\"patterns\":" << snapshot.editor.patterns.size() << ",";
        out << "\"order_slots\":" << snapshot.editor.order.size() << ",";
        out << "\"tracks\":" << snapshot.editor.tracks.size() << ",";
        out << "\"instruments\":" << snapshot.editor.instruments.size() << ",";
        out << "\"active_pattern\":" << snapshot.editor.status.activePattern << ",";
        out << "\"cursor_row\":" << snapshot.editor.status.cursorRow << ",";
        out << "\"cursor_track\":" << snapshot.editor.status.cursorTrack << ",";
        out << "\"selection_rows\":" << snapshot.editor.status.selectionRows << ",";
        out << "\"selection_tracks\":" << snapshot.editor.status.selectionTracks << ",";
        out << "\"grid_start_row\":" << snapshot.editor.activeGrid.startRow << ",";
        out << "\"grid_row_count\":" << snapshot.editor.activeGrid.rowCount << ",";
        out << "\"can_undo\":" << (snapshot.editor.status.canUndo ? "true" : "false") << ",";
        out << "\"can_redo\":" << (snapshot.editor.status.canRedo ? "true" : "false");
        out << "}";
        out << ",\"playback\":{";
        out << "\"state\":\"" << transportStateName(snapshot.playback.state) << "\",";
        out << "\"sample_rate\":" << snapshot.playback.sampleRate << ",";
        out << "\"follow_cursor\":" << (snapshot.playback.followCursor ? "true" : "false") << ",";
        out << "\"preview_active\":" << (snapshot.playback.previewActive ? "true" : "false") << ",";
        out << "\"absolute_row\":" << snapshot.playback.position.absoluteRow << ",";
        out << "\"seconds\":" << snapshot.playback.position.seconds << ",";
        out << "\"order_index\":" << snapshot.playback.position.orderIndex << ",";
        out << "\"pattern\":" << snapshot.playback.position.pattern << ",";
        out << "\"pattern_row\":" << snapshot.playback.position.patternRow;
        out << "}";
        out << ",\"audio\":{";
        out << "\"configured\":" << (snapshot.audio.configured ? "true" : "false") << ",";
        out << "\"active\":" << (snapshot.audio.active ? "true" : "false") << ",";
        out << "\"backend\":\"" << audioBackendName(snapshot.audio.backend) << "\",";
        out << "\"device_id\":\"" << escapeJson(snapshot.audio.deviceId) << "\",";
        out << "\"sample_rate\":" << snapshot.audio.sampleRate << ",";
        out << "\"buffer_frames\":" << snapshot.audio.bufferFrames << ",";
        out << "\"periods\":" << snapshot.audio.periods << ",";
        out << "\"latency_ms\":" << snapshot.audio.estimatedLatencyMs << ",";
        out << "\"processed_blocks\":" << snapshot.audio.processedBlocks << ",";
        out << "\"processed_frames\":" << snapshot.audio.processedFrames << ",";
        out << "\"underruns\":" << snapshot.audio.underrunCount << ",";
        out << "\"last_error\":\"" << escapeJson(snapshot.audio.lastError) << "\"";
        out << "}";
        out << ",\"tasks\":{";
        out << "\"has_active\":" << (snapshot.hasActiveTasks ? "true" : "false") << ",";
        out << "\"active_count\":" << snapshot.activeTaskCount << ",";
        out << "\"total\":" << snapshot.tasks.size() << ",";
        out << "\"items\":[";
        for (std::size_t index = 0; index < snapshot.tasks.size(); ++index) {
            const AppTaskSnapshot& task = snapshot.tasks[index];
            if (index > 0) {
                out << ",";
            }
            out << "{";
            out << "\"id\":" << task.id << ",";
            out << "\"kind\":\"" << appTaskKindName(task.kind) << "\",";
            out << "\"state\":\"" << appTaskStateName(task.state) << "\",";
            out << "\"title\":\"" << escapeJson(task.title) << "\",";
            out << "\"detail\":\"" << escapeJson(task.detail) << "\",";
            out << "\"progress\":{";
            out << "\"current\":" << task.progress.current << ",";
            out << "\"total\":" << task.progress.total << ",";
            out << "\"fraction\":" << task.progress.fraction << ",";
            out << "\"label\":\"" << escapeJson(task.progress.label) << "\"";
            out << "},";
            out << "\"message\":\"" << escapeJson(task.message) << "\",";
            out << "\"error\":\"" << escapeJson(task.error) << "\",";
            out << "\"outputs\":" << task.outputFiles.size();
            out << "}";
        }
        out << "]";
        out << "}";
    }
    out << "},";
    out << "\"sync_checkpoint\":{";
    out << "\"present\":" << (result.hasSyncCheckpoint ? "true" : "false");
    if (result.hasSyncCheckpoint) {
        out << ",\"name\":\"" << escapeJson(result.syncCheckpoint.name) << "\"";
        out << ",\"event_sequence\":" << result.syncCheckpoint.eventSequence;
        out << ",\"task_id\":" << result.syncCheckpoint.taskId;
        out << ",\"project_path\":\"" << escapeJson(result.syncCheckpoint.projectPath) << "\"";
        out << ",\"project_fingerprint\":\"" << escapeJson(result.syncCheckpoint.projectFingerprint) << "\"";
    }
    out << "},";
    out << "\"suggested_sync_checkpoint\":{";
    out << "\"present\":" << (result.hasSuggestedSyncCheckpoint ? "true" : "false");
    if (result.hasSuggestedSyncCheckpoint) {
        out << ",\"safe_to_commit\":" << (result.suggestedSyncCheckpointSafeToCommit ? "true" : "false");
        out << ",\"name\":\"" << escapeJson(result.suggestedSyncCheckpoint.name) << "\"";
        out << ",\"event_sequence\":" << result.suggestedSyncCheckpoint.eventSequence;
        out << ",\"task_id\":" << result.suggestedSyncCheckpoint.taskId;
        out << ",\"project_path\":\"" << escapeJson(result.suggestedSyncCheckpoint.projectPath) << "\"";
        out << ",\"project_fingerprint\":\"" << escapeJson(result.suggestedSyncCheckpoint.projectFingerprint) << "\"";
    }
    out << "},";
    out << "\"sync_checkpoint_compatibility\":{";
    out << "\"present\":" << (result.hasSyncCheckpointCompatibility ? "true" : "false");
    if (result.hasSyncCheckpointCompatibility) {
        out << ",\"status\":\"" << syncCheckpointCompatibilityName(result.syncCheckpointCompatibility) << "\"";
    }
    out << "},";
    out << "\"sync_checkpoint_stale\":{";
    out << "\"present\":" << (result.syncCheckpointStale ? "true" : "false");
    if (result.syncCheckpointStale) {
        out << ",\"reason\":\"" << escapeJson(result.syncCheckpointStaleReason) << "\"";
    }
    out << "},";
    out << "\"sync_checkpoint_update\":{";
    out << "\"present\":" << (result.hasSyncCheckpointUpdateStatus ? "true" : "false");
    if (result.hasSyncCheckpointUpdateStatus) {
        out << ",\"status\":\"" << syncCheckpointUpdateStatusName(result.syncCheckpointUpdateStatus) << "\"";
        out << ",\"reason\":\"" << escapeJson(result.syncCheckpointUpdateReason) << "\"";
    }
    out << "},";
    out << "\"sync_checkpoints\":{";
    out << "\"present\":" << (result.hasSyncCheckpoints ? "true" : "false");
    if (result.hasSyncCheckpoints) {
        out << ",\"items\":[";
        for (std::size_t index = 0; index < result.syncCheckpoints.size(); ++index) {
            if (index > 0) {
                out << ",";
            }
            const SyncCheckpoint& checkpoint = result.syncCheckpoints[index];
            out << "{";
            out << "\"name\":\"" << escapeJson(checkpoint.name) << "\",";
            out << "\"event_sequence\":" << checkpoint.eventSequence << ",";
            out << "\"task_id\":" << checkpoint.taskId << ",";
            out << "\"project_path\":\"" << escapeJson(checkpoint.projectPath) << "\",";
            out << "\"project_fingerprint\":\"" << escapeJson(checkpoint.projectFingerprint) << "\"";
            out << "}";
        }
        out << "]";
    }
    out << "},";
    out << "\"export_result\":{";
    out << "\"present\":" << (result.hasExportResult ? "true" : "false");
    if (result.hasExportResult) {
        out << ",\"ok\":" << (result.exportResult.ok ? "true" : "false");
        out << ",\"target\":\"" << exportTargetName(result.exportResult.target) << "\"";
        out << ",\"message\":\"" << escapeJson(result.exportResult.message) << "\"";
        out << ",\"error\":\"" << escapeJson(result.exportResult.error) << "\"";
        out << ",\"files\":[";
        for (std::size_t index = 0; index < result.exportResult.files.size(); ++index) {
            if (index > 0) {
                out << ",";
            }
            const ExportedFile& file = result.exportResult.files[index];
            out << "{";
            out << "\"path\":\"" << escapeJson(file.path) << "\",";
            out << "\"sample_rate\":" << file.sampleRate << ",";
            out << "\"frame_count\":" << file.frameCount;
            out << "}";
        }
        out << "]";
    }
    out << "},";
    out << "\"script_import_result\":{";
    out << "\"present\":" << (result.hasScriptImportResult ? "true" : "false");
    if (result.hasScriptImportResult) {
        out << ",\"ok\":" << (result.scriptImportResult.ok ? "true" : "false");
        out << ",\"type\":\"" << scriptArtifactTypeName(result.scriptImportResult.type) << "\"";
        out << ",\"path\":\"" << escapeJson(result.scriptImportResult.path) << "\"";
        out << ",\"message\":\"" << escapeJson(result.scriptImportResult.message) << "\"";
        out << ",\"error\":\"" << escapeJson(result.scriptImportResult.error) << "\"";
        out << ",\"imported_instrument\":" << result.scriptImportResult.importedInstrument;
        out << ",\"applied_command_count\":" << result.scriptImportResult.appliedCommandCount;
    }
    out << "},";
    out << "\"midi_import_report\":{";
    out << "\"present\":" << (result.hasMidiImportReport ? "true" : "false");
    if (result.hasMidiImportReport) {
        out << ",\"format\":" << result.midiImportReport.midiFormat;
        out << ",\"ticks_per_quarter\":" << result.midiImportReport.ticksPerQuarterNote;
        out << ",\"imported_track_count\":" << result.midiImportReport.importedTrackCount;
        out << ",\"imported_note_count\":" << result.midiImportReport.importedNoteCount;
        out << ",\"track_mappings\":[";
        for (std::size_t index = 0; index < result.midiImportReport.trackMappings.size(); ++index) {
            if (index > 0) {
                out << ",";
            }
            const MidiImportTrackMapping& mapping = result.midiImportReport.trackMappings[index];
            out << "{";
            out << "\"track_index\":" << mapping.trackIndex << ",";
            out << "\"track_name\":\"" << escapeJson(mapping.trackName) << "\",";
            out << "\"instrument_index\":" << mapping.instrumentIndex << ",";
            out << "\"instrument_name\":\"" << escapeJson(mapping.instrumentName) << "\",";
            out << "\"midi_source_track\":" << mapping.midiSourceTrack << ",";
            out << "\"midi_channel\":" << mapping.midiChannel << ",";
            out << "\"dominant_program\":" << mapping.dominantProgram;
            out << "}";
        }
        out << "],";
        out << "\"warnings\":[";
        for (std::size_t index = 0; index < result.midiImportReport.warnings.size(); ++index) {
            if (index > 0) {
                out << ",";
            }
            out << "\"" << escapeJson(result.midiImportReport.warnings[index].message) << "\"";
        }
        out << "]";
    }
    out << "},";
    out << "\"event_delta\":{";
    out << "\"present\":" << (result.hasEventDeltaSummary ? "true" : "false");
    if (result.hasEventDeltaSummary) {
        const AppActionResult::EventDeltaSummary& delta = result.eventDeltaSummary;
        out << ",\"total\":" << delta.total;
        out << ",\"returned\":" << delta.returned;
        out << ",\"dropped\":" << delta.dropped;
        out << ",\"truncated\":" << (delta.truncated ? "true" : "false");
        out << ",\"domains\":{";
        out << "\"project\":" << delta.project << ",";
        out << "\"settings\":" << delta.settings << ",";
        out << "\"editor\":" << delta.editor << ",";
        out << "\"diagnostics\":" << delta.diagnostics << ",";
        out << "\"playback\":" << delta.playback << ",";
        out << "\"audio\":" << delta.audio << ",";
        out << "\"message\":" << delta.message << ",";
        out << "\"tasks\":" << delta.tasks << ",";
        out << "\"script\":" << delta.script << ",";
        out << "\"recovery\":" << delta.recovery << ",";
        out << "\"other\":" << delta.other;
        out << "}";
    }
    out << "},";
    out << "\"event_cursor\":{";
    out << "\"present\":" << (result.hasEventCursor ? "true" : "false");
    if (result.hasEventCursor) {
        out << ",\"requested_since\":" << result.eventCursor.requestedSince;
        out << ",\"recommended_since\":" << result.eventCursor.recommendedSince;
        out << ",\"truncated\":" << (result.eventCursor.truncated ? "true" : "false");
        out << ",\"includes_snapshot\":" << (result.eventCursor.includesSnapshot ? "true" : "false");
        out << ",\"drain\":" << (result.eventCursor.drain ? "true" : "false");
        out << ",\"returned_range\":{";
        out << "\"present\":" << (result.eventCursor.hasReturnedRange ? "true" : "false");
        if (result.eventCursor.hasReturnedRange) {
            out << ",\"from\":" << result.eventCursor.returnedFrom;
            out << ",\"to\":" << result.eventCursor.returnedTo;
        }
        out << "}";
    }
    out << "},";
    out << "\"events\":{";
    out << "\"present\":" << (result.hasEvents ? "true" : "false") << ",";
    out << "\"last_sequence\":" << (result.hasLastEventSequence ? std::to_string(result.lastEventSequence) : "0") << ",";
    out << "\"items\":[";
    for (std::size_t index = 0; index < result.events.size(); ++index) {
        const AppEvent& event = result.events[index];
        if (index > 0) {
            out << ",";
        }
        out << "{";
        out << "\"sequence\":" << event.sequence << ",";
        out << "\"type\":\"" << appEventTypeName(event.type) << "\",";
        out << "\"message\":\"" << escapeJson(event.message) << "\",";
        out << "\"path\":\"" << escapeJson(event.path) << "\",";
        out << "\"task_id\":" << event.taskId << ",";
        out << "\"dirty\":" << (event.dirty ? "true" : "false");
        out << "}";
    }
    out << "]";
    out << "}";
    out << "}";
    return out.str();
}

} // namespace arachno
