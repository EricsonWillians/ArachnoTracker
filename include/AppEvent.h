#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "AppTask.h"

namespace arachno {

enum class AppEventType {
    SessionReady,
    ProjectChanged,
    ProjectLoaded,
    ProjectSaved,
    SettingsChanged,
    EditorChanged,
    DiagnosticsChanged,
    PlaybackChanged,
    AudioRuntimeChanged,
    MessageChanged,
    TaskStarted,
    TaskUpdated,
    TaskFinished,
    ScriptImported,
    RecoveryChanged
};

struct AppEvent {
    std::uint64_t sequence = 0;
    AppEventType type = AppEventType::SessionReady;
    std::string message;
    std::string path;
    AppTaskId taskId = 0;
    bool dirty = false;
};

class AppEventLog {
public:
    AppEvent record(AppEventType type, const std::string& message = "", const std::string& path = "", AppTaskId taskId = 0, bool dirty = false);
    std::vector<AppEvent> eventsSince(std::uint64_t sequence) const;
    std::vector<AppEvent> drainEvents();
    std::uint64_t lastSequence() const { return nextSequence_ == 1 ? 0 : nextSequence_ - 1; }
    bool empty() const { return events_.empty(); }

private:
    std::uint64_t nextSequence_ = 1;
    std::vector<AppEvent> events_;
};

const char* appEventTypeName(AppEventType type);

} // namespace arachno
