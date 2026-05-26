#pragma once

#include <string>
#include <vector>

namespace arachno {

using AppTaskId = int;

enum class AppTaskKind {
    Export,
    Render,
    Audio,
    Script,
    FileIO,
    Recovery,
    PluginScan,
    Other
};

enum class AppTaskState {
    Queued,
    Running,
    Succeeded,
    Failed,
    Canceled
};

struct AppTaskProgress {
    int current = 0;
    int total = 0;
    double fraction = 0.0;
    std::string label;
};

struct AppTaskSnapshot {
    AppTaskId id = 0;
    AppTaskKind kind = AppTaskKind::Other;
    AppTaskState state = AppTaskState::Queued;
    std::string title;
    std::string detail;
    AppTaskProgress progress;
    std::string message;
    std::string error;
    std::vector<std::string> outputFiles;
};

class AppTaskManager {
public:
    AppTaskId startTask(AppTaskKind kind, const std::string& title, int totalWork = 0, const std::string& detail = "");
    bool updateTask(AppTaskId id, int currentWork, const std::string& label = "");
    bool addTaskOutputFile(AppTaskId id, const std::string& path);
    bool finishTask(AppTaskId id, const std::string& message = "");
    bool failTask(AppTaskId id, const std::string& error);
    bool cancelTask(AppTaskId id, const std::string& message = "Canceled");
    bool clearFinishedTasks();

    const AppTaskSnapshot* findTask(AppTaskId id) const;
    std::vector<AppTaskSnapshot> tasks() const;
    std::vector<AppTaskSnapshot> activeTasks() const;
    int activeTaskCount() const;
    bool hasActiveTasks() const;

private:
    AppTaskSnapshot* findMutableTask(AppTaskId id);

    AppTaskId nextId_ = 1;
    std::vector<AppTaskSnapshot> tasks_;
};

const char* appTaskKindName(AppTaskKind kind);
const char* appTaskStateName(AppTaskState state);
bool isAppTaskFinished(AppTaskState state);

} // namespace arachno
