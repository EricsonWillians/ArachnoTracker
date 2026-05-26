#include "AppAsync.h"

#include <exception>
#include <utility>

namespace arachno {

AppAsyncJobContext::AppAsyncJobContext(
    AppAsyncTaskRunner& runner,
    AppTaskId taskId,
    std::shared_ptr<std::atomic<bool>> cancelFlag)
    : runner_(runner), taskId_(taskId), cancelFlag_(std::move(cancelFlag)) {}

bool AppAsyncJobContext::cancellationRequested() const {
    return cancelFlag_ && cancelFlag_->load();
}

bool AppAsyncJobContext::update(int currentWork, const std::string& label) {
    return runner_.updateTask(taskId_, currentWork, label);
}

bool AppAsyncJobContext::addOutputFile(const std::string& path) {
    return runner_.addTaskOutputFile(taskId_, path);
}

bool AppAsyncJobContext::finish(const std::string& message) {
    return runner_.finishTask(taskId_, message);
}

bool AppAsyncJobContext::fail(const std::string& error) {
    return runner_.failTask(taskId_, error);
}

bool AppAsyncJobContext::cancel(const std::string& message) {
    return runner_.cancelTask(taskId_, message);
}

AppAsyncTaskRunner::~AppAsyncTaskRunner() {
    waitAll();
}

AppTaskId AppAsyncTaskRunner::startTask(
    AppTaskKind kind,
    const std::string& title,
    int totalWork,
    const std::string& detail,
    Work work) {
    std::shared_ptr<Job> job = std::make_shared<Job>();
    job->cancelFlag = std::make_shared<std::atomic<bool>>(false);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        job->taskId = tasks_.startTask(kind, title, totalWork, detail);
        jobs_.push_back(job);
    }

    job->thread = std::thread([this, job, work = std::move(work)]() mutable {
        AppAsyncJobContext context(*this, job->taskId, job->cancelFlag);
        try {
            work(context);
            if (!taskFinished(job->taskId)) {
                if (context.cancellationRequested()) {
                    cancelTask(job->taskId, "Canceled");
                } else {
                    finishTask(job->taskId, "Completed");
                }
            }
        } catch (const std::exception& error) {
            failTask(job->taskId, error.what());
        } catch (...) {
            failTask(job->taskId, "task failed with an unknown error");
        }
    });

    return job->taskId;
}

bool AppAsyncTaskRunner::requestCancel(AppTaskId taskId) {
    const std::shared_ptr<Job> job = findJob(taskId);
    if (!job) {
        return false;
    }
    job->cancelFlag->store(true);
    return true;
}

bool AppAsyncTaskRunner::wait(AppTaskId taskId) {
    const std::shared_ptr<Job> job = findJob(taskId);
    if (!job) {
        return false;
    }
    if (job->thread.joinable()) {
        job->thread.join();
    }
    return true;
}

void AppAsyncTaskRunner::waitAll() {
    std::vector<std::shared_ptr<Job>> jobs;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        jobs = jobs_;
    }
    for (const std::shared_ptr<Job>& job : jobs) {
        if (job && job->thread.joinable()) {
            job->thread.join();
        }
    }
}

std::vector<AppTaskSnapshot> AppAsyncTaskRunner::tasks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.tasks();
}

AppTaskSnapshot AppAsyncTaskRunner::taskSnapshot(AppTaskId taskId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const AppTaskSnapshot* task = tasks_.findTask(taskId);
    return task == nullptr ? AppTaskSnapshot {} : *task;
}

bool AppAsyncTaskRunner::hasActiveTasks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.hasActiveTasks();
}

bool AppAsyncTaskRunner::updateTask(AppTaskId taskId, int currentWork, const std::string& label) {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.updateTask(taskId, currentWork, label);
}

bool AppAsyncTaskRunner::addTaskOutputFile(AppTaskId taskId, const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.addTaskOutputFile(taskId, path);
}

bool AppAsyncTaskRunner::finishTask(AppTaskId taskId, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.finishTask(taskId, message);
}

bool AppAsyncTaskRunner::failTask(AppTaskId taskId, const std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.failTask(taskId, error);
}

bool AppAsyncTaskRunner::cancelTask(AppTaskId taskId, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.cancelTask(taskId, message);
}

bool AppAsyncTaskRunner::taskFinished(AppTaskId taskId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const AppTaskSnapshot* task = tasks_.findTask(taskId);
    return task != nullptr && isAppTaskFinished(task->state);
}

std::shared_ptr<AppAsyncTaskRunner::Job> AppAsyncTaskRunner::findJob(AppTaskId taskId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const std::shared_ptr<Job>& job : jobs_) {
        if (job && job->taskId == taskId) {
            return job;
        }
    }
    return nullptr;
}

} // namespace arachno
