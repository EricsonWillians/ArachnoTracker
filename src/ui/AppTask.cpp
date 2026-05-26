#include "AppTask.h"

#include <algorithm>

namespace arachno {

namespace {
double progressFraction(int current, int total) {
    if (total <= 0) {
        return 0.0;
    }
    const double raw = static_cast<double>(current) / static_cast<double>(total);
    return std::max(0.0, std::min(1.0, raw));
}
} // namespace

AppTaskId AppTaskManager::startTask(
    AppTaskKind kind,
    const std::string& title,
    int totalWork,
    const std::string& detail) {
    AppTaskSnapshot task;
    task.id = nextId_++;
    task.kind = kind;
    task.state = AppTaskState::Running;
    task.title = title;
    task.detail = detail;
    task.progress.total = std::max(0, totalWork);
    task.progress.fraction = progressFraction(0, task.progress.total);
    tasks_.push_back(task);
    return task.id;
}

bool AppTaskManager::updateTask(AppTaskId id, int currentWork, const std::string& label) {
    AppTaskSnapshot* task = findMutableTask(id);
    if (task == nullptr || isAppTaskFinished(task->state)) {
        return false;
    }
    task->state = AppTaskState::Running;
    task->progress.current = std::max(0, currentWork);
    task->progress.fraction = progressFraction(task->progress.current, task->progress.total);
    task->progress.label = label;
    return true;
}

bool AppTaskManager::addTaskOutputFile(AppTaskId id, const std::string& path) {
    AppTaskSnapshot* task = findMutableTask(id);
    if (task == nullptr || path.empty()) {
        return false;
    }
    task->outputFiles.push_back(path);
    return true;
}

bool AppTaskManager::finishTask(AppTaskId id, const std::string& message) {
    AppTaskSnapshot* task = findMutableTask(id);
    if (task == nullptr || isAppTaskFinished(task->state)) {
        return false;
    }
    task->state = AppTaskState::Succeeded;
    task->message = message;
    if (task->progress.total > 0) {
        task->progress.current = task->progress.total;
        task->progress.fraction = 1.0;
    }
    return true;
}

bool AppTaskManager::failTask(AppTaskId id, const std::string& error) {
    AppTaskSnapshot* task = findMutableTask(id);
    if (task == nullptr || isAppTaskFinished(task->state)) {
        return false;
    }
    task->state = AppTaskState::Failed;
    task->error = error;
    return true;
}

bool AppTaskManager::cancelTask(AppTaskId id, const std::string& message) {
    AppTaskSnapshot* task = findMutableTask(id);
    if (task == nullptr || isAppTaskFinished(task->state)) {
        return false;
    }
    task->state = AppTaskState::Canceled;
    task->message = message;
    return true;
}

bool AppTaskManager::clearFinishedTasks() {
    const std::size_t before = tasks_.size();
    tasks_.erase(
        std::remove_if(tasks_.begin(), tasks_.end(), [](const AppTaskSnapshot& task) {
            return isAppTaskFinished(task.state);
        }),
        tasks_.end());
    return before != tasks_.size();
}

const AppTaskSnapshot* AppTaskManager::findTask(AppTaskId id) const {
    const auto it = std::find_if(tasks_.begin(), tasks_.end(), [id](const AppTaskSnapshot& task) {
        return task.id == id;
    });
    return it == tasks_.end() ? nullptr : &(*it);
}

std::vector<AppTaskSnapshot> AppTaskManager::tasks() const {
    return tasks_;
}

std::vector<AppTaskSnapshot> AppTaskManager::activeTasks() const {
    std::vector<AppTaskSnapshot> active;
    for (const AppTaskSnapshot& task : tasks_) {
        if (!isAppTaskFinished(task.state)) {
            active.push_back(task);
        }
    }
    return active;
}

int AppTaskManager::activeTaskCount() const {
    return static_cast<int>(activeTasks().size());
}

bool AppTaskManager::hasActiveTasks() const {
    return activeTaskCount() > 0;
}

AppTaskSnapshot* AppTaskManager::findMutableTask(AppTaskId id) {
    const auto it = std::find_if(tasks_.begin(), tasks_.end(), [id](const AppTaskSnapshot& task) {
        return task.id == id;
    });
    return it == tasks_.end() ? nullptr : &(*it);
}

const char* appTaskKindName(AppTaskKind kind) {
    switch (kind) {
        case AppTaskKind::Export:
            return "export";
        case AppTaskKind::Render:
            return "render";
        case AppTaskKind::Audio:
            return "audio";
        case AppTaskKind::Script:
            return "script";
        case AppTaskKind::FileIO:
            return "file-io";
        case AppTaskKind::Recovery:
            return "recovery";
        case AppTaskKind::PluginScan:
            return "plugin-scan";
        case AppTaskKind::Other:
            return "other";
    }
    return "other";
}

const char* appTaskStateName(AppTaskState state) {
    switch (state) {
        case AppTaskState::Queued:
            return "queued";
        case AppTaskState::Running:
            return "running";
        case AppTaskState::Succeeded:
            return "succeeded";
        case AppTaskState::Failed:
            return "failed";
        case AppTaskState::Canceled:
            return "canceled";
    }
    return "unknown";
}

bool isAppTaskFinished(AppTaskState state) {
    return state == AppTaskState::Succeeded
        || state == AppTaskState::Failed
        || state == AppTaskState::Canceled;
}

} // namespace arachno
