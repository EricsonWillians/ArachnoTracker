#include "ApplicationSession.h"

#include <algorithm>
#include <exception>
#include <utility>
#include <vector>

#include "PatchIO.h"
#include "ProjectIO.h"

namespace arachno {

ApplicationSession::ApplicationSession(Song song, int sampleRate, AppSettings settings)
    : playback_(sampleRate), settings_(std::move(settings)) {
    validateAppSettings(settings_);
    playback_.setFollowCursor(settings_.layout.followPlayback);
    replaceSong(std::move(song), "", false);
    (void)configureAudioRuntime(audioRuntimeSettingsFromPreferences(settings_.audioRuntime));
    setMessage(AppMessageSeverity::Info, "Project ready");
    emitEvent(AppEventType::SessionReady, lastMessage_.text);
}

AppOperationResult ApplicationSession::newProject(Song song) {
    try {
        replaceSong(std::move(song), "", true);
        setMessage(AppMessageSeverity::Info, "Created new project");
        emitEvent(AppEventType::ProjectChanged, lastMessage_.text);
        return {true, lastMessage_.text, ""};
    } catch (const std::exception& error) {
        setMessage(AppMessageSeverity::Error, error.what());
        return {false, "", error.what()};
    }
}

AppOperationResult ApplicationSession::loadProjectFile(const std::string& path) {
    try {
        Song loaded = loadProject(path);
        replaceSong(std::move(loaded), path, false);
        addRecentProject(settings_, path);
        setMessage(AppMessageSeverity::Info, "Loaded project");
        emitEvent(AppEventType::ProjectLoaded, lastMessage_.text, path);
        emitEvent(AppEventType::SettingsChanged, "Updated recent projects", path);
        return {true, lastMessage_.text, ""};
    } catch (const std::exception& error) {
        setMessage(AppMessageSeverity::Error, error.what());
        return {false, "", error.what()};
    }
}

AppOperationResult ApplicationSession::importMidiFile(
    const std::string& path,
    const MidiImportOptions& options,
    MidiImportReport* importedReport) {
    try {
        const MidiImportReport imported = arachno::importMidiFile(path, options);
        if (importedReport != nullptr) {
            *importedReport = imported;
        }
        replaceSong(imported.song, "", true);
        setMessage(AppMessageSeverity::Info, "Imported MIDI file");
        emitEvent(AppEventType::ProjectChanged, lastMessage_.text, path);
        return {true, lastMessage_.text, ""};
    } catch (const std::exception& error) {
        setMessage(AppMessageSeverity::Error, error.what());
        return {false, "", error.what()};
    }
}

AppOperationResult ApplicationSession::saveProjectFile() {
    if (projectPath_.empty()) {
        const std::string error = "project path is not set";
        setMessage(AppMessageSeverity::Error, error);
        return {false, "", error};
    }
    return saveProjectFileAs(projectPath_);
}

AppOperationResult ApplicationSession::saveProjectFileAs(const std::string& path) {
    try {
        saveProject(song_, path);
        projectPath_ = path;
        addRecentProject(settings_, path);
        dirty_ = false;
        refreshDiagnostics();
        setMessage(AppMessageSeverity::Info, "Saved project");
        emitEvent(AppEventType::ProjectSaved, lastMessage_.text, path);
        emitEvent(AppEventType::SettingsChanged, "Updated recent projects", path);
        return {true, lastMessage_.text, ""};
    } catch (const std::exception& error) {
        setMessage(AppMessageSeverity::Error, error.what());
        return {false, "", error.what()};
    }
}

RecoveryResult ApplicationSession::saveRecoverySnapshot(const std::string& path) const {
    return saveRecoveryFile(song_, path);
}

RecoveryResult ApplicationSession::restoreRecoverySnapshot(const std::string& path) {
    RecoveryResult result;
    result.path = path;
    try {
        Song recovered = loadRecoveryFile(path);
        replaceSong(std::move(recovered), projectPath_, true);
        result.ok = true;
        result.message = "Restored recovery file";
        setMessage(AppMessageSeverity::Info, result.message);
        emitEvent(AppEventType::RecoveryChanged, result.message, path);
    } catch (const std::exception& error) {
        result.error = error.what();
        setMessage(AppMessageSeverity::Error, result.error);
    }
    return result;
}

RecoveryResult ApplicationSession::clearRecoverySnapshot(const std::string& path) const {
    return clearRecoveryFile(path);
}

AppOperationResult ApplicationSession::loadSettingsFile(const std::string& path) {
    try {
        const AppSettings previousSettings = settings_;
        settings_ = loadAppSettings(path);
        playback_.setFollowCursor(settings_.layout.followPlayback);
        const AudioRuntimeValidation audio = configureAudioRuntime(
            audioRuntimeSettingsFromPreferences(settings_.audioRuntime));
        if (!audio.ok) {
            settings_ = previousSettings;
            playback_.setFollowCursor(settings_.layout.followPlayback);
            (void)configureAudioRuntime(audioRuntimeSettingsFromPreferences(settings_.audioRuntime));
            setMessage(AppMessageSeverity::Error, audio.error);
            return {false, "", audio.error};
        }
        setMessage(AppMessageSeverity::Info, "Loaded settings");
        emitEvent(AppEventType::SettingsChanged, lastMessage_.text, path);
        emitEvent(AppEventType::PlaybackChanged, "Updated follow-cursor setting");
        return {true, lastMessage_.text, ""};
    } catch (const std::exception& error) {
        setMessage(AppMessageSeverity::Error, error.what());
        return {false, "", error.what()};
    }
}

AppOperationResult ApplicationSession::saveSettingsFile(const std::string& path) const {
    try {
        saveAppSettings(settings_, path);
        return {true, "Saved settings", ""};
    } catch (const std::exception& error) {
        return {false, "", error.what()};
    }
}

AppOperationResult ApplicationSession::updateSettings(const AppSettings& settings) {
    try {
        const AppSettings previousSettings = settings_;
        validateAppSettings(settings);
        settings_ = settings;
        playback_.setFollowCursor(settings_.layout.followPlayback);
        const AudioRuntimeValidation audio = configureAudioRuntime(
            audioRuntimeSettingsFromPreferences(settings_.audioRuntime));
        if (!audio.ok) {
            settings_ = previousSettings;
            playback_.setFollowCursor(settings_.layout.followPlayback);
            (void)configureAudioRuntime(audioRuntimeSettingsFromPreferences(settings_.audioRuntime));
            setMessage(AppMessageSeverity::Error, audio.error);
            return {false, "", audio.error};
        }
        setMessage(AppMessageSeverity::Info, "Updated settings");
        emitEvent(AppEventType::SettingsChanged, lastMessage_.text);
        emitEvent(AppEventType::PlaybackChanged, "Updated follow-cursor setting");
        return {true, lastMessage_.text, ""};
    } catch (const std::exception& error) {
        setMessage(AppMessageSeverity::Error, error.what());
        return {false, "", error.what()};
    }
}

AppOperationResult ApplicationSession::saveSyncCheckpoint(const SyncCheckpoint& checkpoint, int maxCheckpoints) {
    if (checkpoint.name.empty()) {
        const std::string error = "sync checkpoint name is required";
        setMessage(AppMessageSeverity::Error, error);
        return {false, "", error};
    }
    if (checkpoint.taskId < 0) {
        const std::string error = "sync checkpoint task id must not be negative";
        setMessage(AppMessageSeverity::Error, error);
        return {false, "", error};
    }
    if (maxCheckpoints <= 0) {
        const std::string error = "max checkpoints must be positive";
        setMessage(AppMessageSeverity::Error, error);
        return {false, "", error};
    }

    if (!upsertSyncCheckpoint(settings_, checkpoint, maxCheckpoints)) {
        const std::string error = "failed to save sync checkpoint";
        setMessage(AppMessageSeverity::Error, error);
        return {false, "", error};
    }

    setMessage(AppMessageSeverity::Info, "Saved sync checkpoint");
    emitEvent(AppEventType::SettingsChanged, "Saved sync checkpoint", checkpoint.name);
    return {true, "Saved sync checkpoint", ""};
}

AppOperationResult ApplicationSession::clearSyncCheckpoint(const std::string& name) {
    if (name.empty()) {
        const std::string error = "sync checkpoint name is required";
        setMessage(AppMessageSeverity::Error, error);
        return {false, "", error};
    }
    if (!removeSyncCheckpoint(settings_, name)) {
        const std::string error = "sync checkpoint not found";
        setMessage(AppMessageSeverity::Error, error);
        return {false, "", error};
    }
    setMessage(AppMessageSeverity::Info, "Cleared sync checkpoint");
    emitEvent(AppEventType::SettingsChanged, "Cleared sync checkpoint", name);
    return {true, "Cleared sync checkpoint", ""};
}

EditorCommandResult ApplicationSession::applyEditorCommand(const std::string& command) {
    EditorCommandResult result = editor_->tryApplyCommand(command);
    if (result.ok) {
        if (result.projectChanged) {
            dirty_ = true;
            refreshDiagnostics();
            emitEvent(AppEventType::ProjectChanged, result.message);
        }
        if (result.message != "noop") {
            setMessage(AppMessageSeverity::Info, result.message);
        }
        playback_.setSong(&song_);
        emitEvent(AppEventType::EditorChanged, result.message);
        emitEvent(AppEventType::PlaybackChanged, "Updated playback song binding");
    } else {
        setMessage(AppMessageSeverity::Error, result.error);
    }
    return result;
}

AudioRuntimeValidation ApplicationSession::configureAudioRuntime(const AudioRuntimeSettings& settings) {
    const AudioRuntimeValidation result = audioRuntime_.configure(settings);
    if (result.ok) {
        settings_.audioRuntime = audioRuntimePreferencesFromSettings(result.normalized);
        emitEvent(
            AppEventType::AudioRuntimeChanged,
            "Configured audio runtime",
            result.device.id);
    } else {
        emitEvent(AppEventType::AudioRuntimeChanged, result.error);
    }
    return result;
}

AppOperationResult ApplicationSession::configureAudioRuntimeWithTask(const AudioRuntimeSettings& settings) {
    const AppTaskId task = taskManager_.startTask(
        AppTaskKind::Audio,
        "Configure audio runtime",
        1,
        audioBackendName(settings.backend));
    taskManager_.updateTask(task, 0, "Validating audio runtime settings");
    emitEvent(AppEventType::TaskStarted, "Configuring audio runtime", audioBackendName(settings.backend), task);

    const AudioRuntimeValidation validation = configureAudioRuntime(settings);
    if (!validation.ok) {
        taskManager_.failTask(task, validation.error);
        emitEvent(AppEventType::TaskFinished, validation.error, settings.deviceId, task);
        return {false, "", validation.error};
    }

    taskManager_.updateTask(task, 1, "Configured audio runtime");
    taskManager_.finishTask(task, "Configured audio runtime");
    emitEvent(AppEventType::TaskFinished, "Configured audio runtime", validation.device.id, task);
    return {true, "Configured audio runtime", ""};
}

AppOperationResult ApplicationSession::startAudioRuntime() {
    if (!audioRuntime_.health().configured) {
        const AudioRuntimeValidation configured = configureAudioRuntime(
            audioRuntimeSettingsFromPreferences(settings_.audioRuntime));
        if (!configured.ok) {
            setMessage(AppMessageSeverity::Error, configured.error);
            return {false, "", configured.error};
        }
    }
    if (!audioRuntime_.start()) {
        const std::string error = audioRuntime_.health().lastError;
        setMessage(AppMessageSeverity::Error, error);
        emitEvent(AppEventType::AudioRuntimeChanged, error);
        return {false, "", error};
    }
    setMessage(AppMessageSeverity::Info, "Started audio runtime");
    emitEvent(AppEventType::AudioRuntimeChanged, lastMessage_.text, audioRuntime_.health().deviceId);
    return {true, lastMessage_.text, ""};
}

AppOperationResult ApplicationSession::startAudioRuntimeWithTask() {
    const AppTaskId task = taskManager_.startTask(
        AppTaskKind::Audio,
        "Start audio runtime",
        1,
        settings_.audioRuntime.deviceId);
    taskManager_.updateTask(task, 0, "Starting audio runtime");
    emitEvent(AppEventType::TaskStarted, "Starting audio runtime", settings_.audioRuntime.deviceId, task);

    const AppOperationResult operation = startAudioRuntime();
    if (!operation.ok) {
        taskManager_.failTask(task, operation.error);
        emitEvent(AppEventType::TaskFinished, operation.error, settings_.audioRuntime.deviceId, task);
        return operation;
    }

    taskManager_.updateTask(task, 1, operation.message);
    taskManager_.finishTask(task, operation.message);
    emitEvent(AppEventType::TaskFinished, operation.message, settings_.audioRuntime.deviceId, task);
    return operation;
}

AppOperationResult ApplicationSession::stopAudioRuntime() {
    audioRuntime_.stop();
    setMessage(AppMessageSeverity::Info, "Stopped audio runtime");
    emitEvent(AppEventType::AudioRuntimeChanged, lastMessage_.text, audioRuntime_.health().deviceId);
    return {true, lastMessage_.text, ""};
}

AppOperationResult ApplicationSession::stopAudioRuntimeWithTask() {
    const AppTaskId task = taskManager_.startTask(
        AppTaskKind::Audio,
        "Stop audio runtime",
        1,
        settings_.audioRuntime.deviceId);
    taskManager_.updateTask(task, 0, "Stopping audio runtime");
    emitEvent(AppEventType::TaskStarted, "Stopping audio runtime", settings_.audioRuntime.deviceId, task);

    const AppOperationResult operation = stopAudioRuntime();
    if (!operation.ok) {
        taskManager_.failTask(task, operation.error);
        emitEvent(AppEventType::TaskFinished, operation.error, settings_.audioRuntime.deviceId, task);
        return operation;
    }

    taskManager_.updateTask(task, 1, operation.message);
    taskManager_.finishTask(task, operation.message);
    emitEvent(AppEventType::TaskFinished, operation.message, settings_.audioRuntime.deviceId, task);
    return operation;
}

AppOperationResult ApplicationSession::simulateAudioUnderrun(const std::string& detail) {
    if (!audioRuntime_.health().configured) {
        const std::string error = "audio runtime is not configured";
        setMessage(AppMessageSeverity::Error, error);
        emitEvent(AppEventType::AudioRuntimeChanged, error);
        return {false, "", error};
    }
    const std::string reason = detail.empty() ? "simulated audio underrun" : detail;
    audioRuntime_.reportUnderrun(reason);
    setMessage(AppMessageSeverity::Warning, "Simulated audio underrun");
    emitEvent(AppEventType::AudioRuntimeChanged, lastMessage_.text, audioRuntime_.health().deviceId);
    return {true, lastMessage_.text, ""};
}

AppOperationResult ApplicationSession::simulateAudioUnderrunWithTask(const std::string& detail) {
    const AppTaskId task = taskManager_.startTask(
        AppTaskKind::Audio,
        "Simulate audio underrun",
        1,
        settings_.audioRuntime.deviceId);
    taskManager_.updateTask(task, 0, "Injecting underrun diagnostic");
    emitEvent(AppEventType::TaskStarted, "Simulating audio underrun", settings_.audioRuntime.deviceId, task);

    const AppOperationResult operation = simulateAudioUnderrun(detail);
    if (!operation.ok) {
        taskManager_.failTask(task, operation.error);
        emitEvent(AppEventType::TaskFinished, operation.error, settings_.audioRuntime.deviceId, task);
        return operation;
    }

    taskManager_.updateTask(task, 1, operation.message);
    taskManager_.finishTask(task, operation.message);
    emitEvent(AppEventType::TaskFinished, operation.message, settings_.audioRuntime.deviceId, task);
    return operation;
}

AudioRuntimeRenderTestResult ApplicationSession::renderAudioRuntimeTestWithTask(int frameCount, int blockCount) {
    AudioRuntimeRenderTestResult result;
    result.frameCount = frameCount;
    result.blockCount = blockCount;

    const AppTaskId task = taskManager_.startTask(
        AppTaskKind::Audio,
        "Audio render test",
        std::max(1, blockCount),
        settings_.audioRuntime.deviceId);
    taskManager_.updateTask(task, 0, "Preparing runtime render test");
    emitEvent(AppEventType::TaskStarted, "Running audio render test", settings_.audioRuntime.deviceId, task);

    if (frameCount <= 0) {
        result.error = "frame count must be positive";
        taskManager_.failTask(task, result.error);
        emitEvent(AppEventType::TaskFinished, result.error, settings_.audioRuntime.deviceId, task);
        return result;
    }
    if (blockCount <= 0) {
        result.error = "block count must be positive";
        taskManager_.failTask(task, result.error);
        emitEvent(AppEventType::TaskFinished, result.error, settings_.audioRuntime.deviceId, task);
        return result;
    }

    result.runtimeWasActive = audioRuntime_.health().active;
    const int underrunsBefore = audioRuntime_.health().underrunCount;
    result.underrunsBefore = underrunsBefore;

    bool startedForTest = false;
    if (!audioRuntime_.health().active) {
        const AppOperationResult start = startAudioRuntime();
        if (!start.ok) {
            result.error = start.error;
            taskManager_.failTask(task, result.error);
            emitEvent(AppEventType::TaskFinished, result.error, settings_.audioRuntime.deviceId, task);
            return result;
        }
        startedForTest = true;
    }

    std::vector<float> left(static_cast<std::size_t>(frameCount), 0.0f);
    std::vector<float> right(static_cast<std::size_t>(frameCount), 0.0f);
    for (int block = 0; block < blockCount; ++block) {
        const AudioRuntimeProcessResult processed = renderAudioRuntimeBlock(
            left.data(),
            right.data(),
            frameCount);
        if (!processed.ok) {
            result.error = processed.error;
            taskManager_.failTask(task, result.error);
            emitEvent(AppEventType::TaskFinished, result.error, settings_.audioRuntime.deviceId, task);
            if (startedForTest) {
                (void)stopAudioRuntime();
            }
            return result;
        }
        result.processedBlocks += 1;
        result.processedFrames += processed.frames;
        taskManager_.updateTask(task, block + 1, "Rendered test block " + std::to_string(block + 1));
        emitEvent(
            AppEventType::TaskUpdated,
            "Rendered test block " + std::to_string(block + 1),
            settings_.audioRuntime.deviceId,
            task);
    }

    if (startedForTest) {
        (void)stopAudioRuntime();
    }
    result.underrunsAfter = audioRuntime_.health().underrunCount;
    result.ok = true;
    result.message = "Audio render test completed";
    taskManager_.finishTask(task, result.message);
    emitEvent(AppEventType::TaskFinished, result.message, settings_.audioRuntime.deviceId, task);
    return result;
}

AudioRuntimeProcessResult ApplicationSession::renderAudioRuntimeBlock(float* left, float* right, int frameCount) {
    AudioRuntimeProcessResult result = audioRuntime_.renderBlock(playback_, left, right, frameCount);
    if (!result.ok) {
        emitEvent(AppEventType::AudioRuntimeChanged, result.error, audioRuntime_.health().deviceId);
    } else if (result.underrun) {
        emitEvent(AppEventType::AudioRuntimeChanged, "Audio underrun", audioRuntime_.health().deviceId);
    }
    return result;
}

ScriptImportResult ApplicationSession::importScriptArtifact(const std::string& path, const std::string& nameOverride) {
    const ScriptArtifactType type = scriptArtifactTypeFromPath(path);
    if (type == ScriptArtifactType::Project) {
        return importScriptProject(path);
    }
    if (type == ScriptArtifactType::Patch) {
        return importScriptPatch(path, nameOverride);
    }
    if (type == ScriptArtifactType::CommandFile) {
        return applyScriptCommandFile(path);
    }

    ScriptImportResult result;
    result.type = type;
    result.path = path;
    result.error = "unsupported script artifact type";
    setMessage(AppMessageSeverity::Error, result.error);
    return result;
}

ScriptImportResult ApplicationSession::importScriptProject(const std::string& path) {
    ScriptImportResult result;
    result.type = ScriptArtifactType::Project;
    result.path = path;
    try {
        Song loaded = loadProject(path);
        replaceSong(std::move(loaded), path, false);
        addRecentProject(settings_, path);
        result.ok = true;
        result.message = "Imported script project";
        setMessage(AppMessageSeverity::Info, result.message);
        emitEvent(AppEventType::ScriptImported, result.message, path);
        emitEvent(AppEventType::SettingsChanged, "Updated recent projects", path);
    } catch (const std::exception& error) {
        result.error = error.what();
        setMessage(AppMessageSeverity::Error, result.error);
    }
    return result;
}

ScriptImportResult ApplicationSession::importScriptPatch(const std::string& path, const std::string& nameOverride) {
    ScriptImportResult result;
    result.type = ScriptArtifactType::Patch;
    result.path = path;
    try {
        SynthPatch patch = loadPatch(path);
        if (!nameOverride.empty()) {
            patch.name = nameOverride;
        }

        Instrument instrument;
        instrument.id = static_cast<int>(song_.instruments.size());
        instrument.patch = patch;
        song_.instruments.push_back(instrument);

        result.ok = true;
        result.importedInstrument = instrument.id;
        result.message = "Imported script patch";
        dirty_ = true;
        refreshDiagnostics();
        playback_.setSong(&song_);
        setMessage(AppMessageSeverity::Info, result.message);
        emitEvent(AppEventType::ScriptImported, result.message, path);
        emitEvent(AppEventType::ProjectChanged, result.message);
        emitEvent(AppEventType::PlaybackChanged, "Updated playback song binding");
    } catch (const std::exception& error) {
        result.error = error.what();
        setMessage(AppMessageSeverity::Error, result.error);
    }
    return result;
}

ScriptImportResult ApplicationSession::applyScriptCommandFile(const std::string& path) {
    ScriptImportResult result;
    result.type = ScriptArtifactType::CommandFile;
    result.path = path;
    try {
        const std::vector<std::string> commands = loadScriptCommandFile(path);
        for (std::size_t index = 0; index < commands.size(); ++index) {
            const EditorCommandResult commandResult = applyEditorCommand(commands[index]);
            if (!commandResult.ok) {
                result.error = "script command " + std::to_string(index + 1) + ": " + commandResult.error;
                setMessage(AppMessageSeverity::Error, result.error);
                return result;
            }
            ++result.appliedCommandCount;
        }
        result.ok = true;
        result.message = "Applied script command file";
        setMessage(AppMessageSeverity::Info, result.message);
        emitEvent(AppEventType::ScriptImported, result.message, path);
    } catch (const std::exception& error) {
        result.error = error.what();
        setMessage(AppMessageSeverity::Error, result.error);
    }
    return result;
}

ExportResult ApplicationSession::exportProject(const ExportRequest& request) const {
    return runExportWorkflow(song_, request);
}

ExportResult ApplicationSession::exportProjectWithTask(const ExportRequest& request) {
    const ExportPreflight preflight = preflightExport(song_, request);
    const int totalWork = preflight.ok ? preflight.totalWork : 1;
    const AppTaskId task = taskManager_.startTask(
        AppTaskKind::Export,
        std::string("Export ") + exportTargetName(request.target),
        totalWork,
        request.outputPath);
    taskManager_.updateTask(task, 0, "Starting export");
    emitEvent(AppEventType::TaskStarted, "Starting export", request.outputPath, task);

    const ExportResult result = runExportWorkflow(
        song_,
        request,
        [this, task](const ExportProgress& progress) {
            taskManager_.updateTask(task, progress.current, progress.label);
            emitEvent(AppEventType::TaskUpdated, progress.label, progress.path, task);
        });
    if (result.ok) {
        for (const ExportedFile& file : result.files) {
            taskManager_.addTaskOutputFile(task, file.path);
        }
        taskManager_.updateTask(task, totalWork, result.message);
        taskManager_.finishTask(task, result.message);
        emitEvent(AppEventType::TaskFinished, result.message, request.outputPath, task);
    } else {
        taskManager_.failTask(task, result.error);
        emitEvent(AppEventType::TaskFinished, result.error, request.outputPath, task);
    }
    return result;
}

AppOperationResult ApplicationSession::cancelTask(AppTaskId taskId, const std::string& message) {
    const AppTaskSnapshot* task = taskManager_.findTask(taskId);
    if (task == nullptr) {
        const std::string error = "task not found";
        setMessage(AppMessageSeverity::Error, error);
        return {false, "", error};
    }
    if (isAppTaskFinished(task->state)) {
        const std::string error = "task is already finished";
        setMessage(AppMessageSeverity::Error, error);
        return {false, "", error};
    }
    if (!taskManager_.cancelTask(taskId, message.empty() ? "Canceled" : message)) {
        const std::string error = "failed to cancel task";
        setMessage(AppMessageSeverity::Error, error);
        return {false, "", error};
    }
    setMessage(AppMessageSeverity::Warning, "Canceled task");
    emitEvent(AppEventType::TaskFinished, "Canceled task", task->detail, taskId);
    return {true, "Canceled task", ""};
}

AppOperationResult ApplicationSession::clearFinishedTasks() {
    if (!taskManager_.clearFinishedTasks()) {
        setMessage(AppMessageSeverity::Info, "No finished tasks to clear");
        return {true, "No finished tasks to clear", ""};
    }
    setMessage(AppMessageSeverity::Info, "Cleared finished tasks");
    emitEvent(AppEventType::TaskUpdated, "Cleared finished tasks");
    return {true, "Cleared finished tasks", ""};
}

bool ApplicationSession::previewCursorStep() {
    return auditionCursorStep().ok;
}

AuditionResult ApplicationSession::auditionCursorStep() {
    const EditorCursor& cursor = editor_->cursor();
    AuditionResult result = playback_.auditionStep(cursor.pattern, cursor.row, cursor.track);
    if (result.ok) {
        setMessage(AppMessageSeverity::Info, result.message);
    } else {
        setMessage(AppMessageSeverity::Warning, result.error);
    }
    emitEvent(AppEventType::PlaybackChanged, result.ok ? result.message : result.error);
    return result;
}

AppSessionSnapshot ApplicationSession::snapshot(int gridStartRow, int gridRowCount) const {
    AppSessionSnapshot result;
    result.projectPath = projectPath_;
    result.hasProjectPath = !projectPath_.empty();
    result.dirty = dirty_;
    result.settings = settings_;
    result.shortcutConflicts = validateEditorShortcuts(effectiveEditorShortcuts(settings_));
    result.lastMessage = lastMessage_;
    result.diagnostics = diagnostics_;
    result.hasDiagnosticErrors = hasErrors(diagnostics_);
    result.diagnosticErrorCount = countDiagnostics(diagnostics_, DiagnosticSeverity::Error);
    result.diagnosticWarningCount = countDiagnostics(diagnostics_, DiagnosticSeverity::Warning);
    result.editor = buildEditorViewModel(song_, *editor_, gridStartRow, gridRowCount);
    result.playback = playback_.snapshot();
    result.audio = audioRuntime_.health();
    result.tasks = taskManager_.tasks();
    result.hasActiveTasks = taskManager_.hasActiveTasks();
    result.activeTaskCount = taskManager_.activeTaskCount();
    return result;
}

std::vector<AppEvent> ApplicationSession::eventsSince(std::uint64_t sequence) const {
    return events_.eventsSince(sequence);
}

std::vector<AppEvent> ApplicationSession::drainEvents() {
    return events_.drainEvents();
}

std::uint64_t ApplicationSession::lastEventSequence() const {
    return events_.lastSequence();
}

void ApplicationSession::replaceSong(Song song, const std::string& projectPath, bool dirty) {
    song_ = std::move(song);
    editor_ = std::make_unique<PatternEditorSession>(song_);
    playback_.stop();
    playback_.setSong(&song_);
    projectPath_ = projectPath;
    dirty_ = dirty;
    refreshDiagnostics();
    emitEvent(AppEventType::ProjectChanged, "Replaced project", projectPath_);
    emitEvent(AppEventType::PlaybackChanged, "Reset playback session");
}

void ApplicationSession::refreshDiagnostics() {
    diagnostics_ = validateProject(song_);
    emitEvent(AppEventType::DiagnosticsChanged, "Updated diagnostics");
}

void ApplicationSession::setMessage(AppMessageSeverity severity, const std::string& text) {
    lastMessage_.severity = severity;
    lastMessage_.text = text;
    emitEvent(AppEventType::MessageChanged, text);
}

void ApplicationSession::emitEvent(
    AppEventType type,
    const std::string& message,
    const std::string& path,
    AppTaskId taskId) {
    events_.record(type, message, path, taskId, dirty_);
}

const char* appMessageSeverityName(AppMessageSeverity severity) {
    switch (severity) {
        case AppMessageSeverity::Info:
            return "info";
        case AppMessageSeverity::Warning:
            return "warning";
        case AppMessageSeverity::Error:
            return "error";
    }
    return "unknown";
}

} // namespace arachno
