#include "AppEvent.h"

namespace arachno {

AppEvent AppEventLog::record(
    AppEventType type,
    const std::string& message,
    const std::string& path,
    AppTaskId taskId,
    bool dirty) {
    AppEvent event;
    event.sequence = nextSequence_++;
    event.type = type;
    event.message = message;
    event.path = path;
    event.taskId = taskId;
    event.dirty = dirty;
    events_.push_back(event);
    return event;
}

std::vector<AppEvent> AppEventLog::eventsSince(std::uint64_t sequence) const {
    std::vector<AppEvent> result;
    for (const AppEvent& event : events_) {
        if (event.sequence > sequence) {
            result.push_back(event);
        }
    }
    return result;
}

std::vector<AppEvent> AppEventLog::drainEvents() {
    std::vector<AppEvent> result = events_;
    events_.clear();
    return result;
}

const char* appEventTypeName(AppEventType type) {
    switch (type) {
        case AppEventType::SessionReady:
            return "session-ready";
        case AppEventType::ProjectChanged:
            return "project-changed";
        case AppEventType::ProjectLoaded:
            return "project-loaded";
        case AppEventType::ProjectSaved:
            return "project-saved";
        case AppEventType::SettingsChanged:
            return "settings-changed";
        case AppEventType::EditorChanged:
            return "editor-changed";
        case AppEventType::DiagnosticsChanged:
            return "diagnostics-changed";
        case AppEventType::PlaybackChanged:
            return "playback-changed";
        case AppEventType::AudioRuntimeChanged:
            return "audio-runtime-changed";
        case AppEventType::MessageChanged:
            return "message-changed";
        case AppEventType::TaskStarted:
            return "task-started";
        case AppEventType::TaskUpdated:
            return "task-updated";
        case AppEventType::TaskFinished:
            return "task-finished";
        case AppEventType::ScriptImported:
            return "script-imported";
        case AppEventType::RecoveryChanged:
            return "recovery-changed";
    }
    return "unknown";
}

} // namespace arachno
