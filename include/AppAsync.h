#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "AppTask.h"

namespace arachno {

class AppAsyncTaskRunner;

class AppAsyncJobContext {
public:
    AppAsyncJobContext(AppAsyncTaskRunner& runner, AppTaskId taskId, std::shared_ptr<std::atomic<bool>> cancelFlag);

    AppTaskId taskId() const { return taskId_; }
    bool cancellationRequested() const;
    bool update(int currentWork, const std::string& label = "");
    bool addOutputFile(const std::string& path);
    bool finish(const std::string& message = "");
    bool fail(const std::string& error);
    bool cancel(const std::string& message = "Canceled");

private:
    AppAsyncTaskRunner& runner_;
    AppTaskId taskId_ = 0;
    std::shared_ptr<std::atomic<bool>> cancelFlag_;
};

class AppAsyncTaskRunner {
public:
    using Work = std::function<void(AppAsyncJobContext&)>;

    AppAsyncTaskRunner() = default;
    ~AppAsyncTaskRunner();

    AppAsyncTaskRunner(const AppAsyncTaskRunner&) = delete;
    AppAsyncTaskRunner& operator=(const AppAsyncTaskRunner&) = delete;

    AppTaskId startTask(
        AppTaskKind kind,
        const std::string& title,
        int totalWork,
        const std::string& detail,
        Work work);
    bool requestCancel(AppTaskId taskId);
    bool wait(AppTaskId taskId);
    void waitAll();

    std::vector<AppTaskSnapshot> tasks() const;
    AppTaskSnapshot taskSnapshot(AppTaskId taskId) const;
    bool hasActiveTasks() const;

private:
    struct Job {
        AppTaskId taskId = 0;
        std::shared_ptr<std::atomic<bool>> cancelFlag;
        std::thread thread;
    };

    friend class AppAsyncJobContext;

    bool updateTask(AppTaskId taskId, int currentWork, const std::string& label);
    bool addTaskOutputFile(AppTaskId taskId, const std::string& path);
    bool finishTask(AppTaskId taskId, const std::string& message);
    bool failTask(AppTaskId taskId, const std::string& error);
    bool cancelTask(AppTaskId taskId, const std::string& message);
    bool taskFinished(AppTaskId taskId) const;
    std::shared_ptr<Job> findJob(AppTaskId taskId) const;

    mutable std::mutex mutex_;
    AppTaskManager tasks_;
    std::vector<std::shared_ptr<Job>> jobs_;
};

} // namespace arachno
