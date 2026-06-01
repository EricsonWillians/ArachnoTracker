#pragma once

#include <memory>
#include <cstdint>
#include <string>
#include <vector>

#include "AppEvent.h"
#include "AppSettings.h"
#include "AppTask.h"
#include "AudioRuntime.h"
#include "AutoSave.h"
#include "EditorViewModel.h"
#include "ExportWorkflow.h"
#include "MidiImporter.h"
#include "PatternEditor.h"
#include "ProjectDiagnostics.h"
#include "RealtimePlayback.h"
#include "ScriptIntegration.h"
#include "Tracker.h"

namespace arachno {

enum class AppMessageSeverity {
    Info,
    Warning,
    Error
};

struct AppMessage {
    AppMessageSeverity severity = AppMessageSeverity::Info;
    std::string text;
};

struct AppOperationResult {
    bool ok = false;
    std::string message;
    std::string error;
};

struct AudioRuntimeRenderTestResult {
    bool ok = false;
    std::string message;
    std::string error;
    int frameCount = 0;
    int blockCount = 0;
    int processedFrames = 0;
    int processedBlocks = 0;
    int underrunsBefore = 0;
    int underrunsAfter = 0;
    bool runtimeWasActive = false;
};

struct AppSessionSnapshot {
    std::string projectPath;
    bool hasProjectPath = false;
    bool dirty = false;
    AppSettings settings;
    std::vector<ShortcutConflict> shortcutConflicts;
    AppMessage lastMessage;
    std::vector<ProjectDiagnostic> diagnostics;
    bool hasDiagnosticErrors = false;
    int diagnosticErrorCount = 0;
    int diagnosticWarningCount = 0;
    EditorViewModel editor;
    PlaybackSnapshot playback;
    AudioRuntimeHealth audio;
    std::vector<AppTaskSnapshot> tasks;
    bool hasActiveTasks = false;
    int activeTaskCount = 0;
};

class ApplicationSession {
public:
    explicit ApplicationSession(
        Song song = makeBlankSong(),
        int sampleRate = 48000,
        AppSettings settings = AppSettings {});

    Song& song() { return song_; }
    const Song& song() const { return song_; }
    PatternEditorSession& editor() { return *editor_; }
    const PatternEditorSession& editor() const { return *editor_; }
    RealtimePlaybackSession& playback() { return playback_; }
    const RealtimePlaybackSession& playback() const { return playback_; }

    bool dirty() const { return dirty_; }
    const std::string& projectPath() const { return projectPath_; }
    const AppSettings& settings() const { return settings_; }
    const std::vector<ProjectDiagnostic>& diagnostics() const { return diagnostics_; }
    const AppMessage& lastMessage() const { return lastMessage_; }
    AppTaskManager& taskManager() { return taskManager_; }
    const AppTaskManager& taskManager() const { return taskManager_; }

    AppOperationResult newProject(Song song);
    AppOperationResult loadProjectFile(const std::string& path);
    AppOperationResult importMidiFile(
        const std::string& path,
        const MidiImportOptions& options = MidiImportOptions {},
        MidiImportReport* importedReport = nullptr);
    AppOperationResult saveProjectFile();
    AppOperationResult saveProjectFileAs(const std::string& path);
    RecoveryResult saveRecoverySnapshot(const std::string& path) const;
    RecoveryResult restoreRecoverySnapshot(const std::string& path);
    RecoveryResult clearRecoverySnapshot(const std::string& path) const;
    AppOperationResult loadSettingsFile(const std::string& path);
    AppOperationResult saveSettingsFile(const std::string& path) const;
    AppOperationResult updateSettings(const AppSettings& settings);
    AppOperationResult saveSyncCheckpoint(const SyncCheckpoint& checkpoint, int maxCheckpoints = 64);
    AppOperationResult clearSyncCheckpoint(const std::string& name);
    AudioRuntimeValidation configureAudioRuntime(const AudioRuntimeSettings& settings);
    AppOperationResult configureAudioRuntimeWithTask(const AudioRuntimeSettings& settings);
    AppOperationResult startAudioRuntime();
    AppOperationResult startAudioRuntimeWithTask();
    AppOperationResult stopAudioRuntime();
    AppOperationResult stopAudioRuntimeWithTask();
    AppOperationResult simulateAudioUnderrun(const std::string& detail = "");
    AppOperationResult simulateAudioUnderrunWithTask(const std::string& detail = "");
    AudioRuntimeRenderTestResult renderAudioRuntimeTestWithTask(int frameCount, int blockCount = 4);
    AudioRuntimeProcessResult renderAudioRuntimeBlock(float* left, float* right, int frameCount);
    const AudioRuntimeHealth& audioRuntimeHealth() const { return audioRuntime_.health(); }
    const std::vector<AudioDeviceInfo>& audioRuntimeDevices() const { return audioRuntime_.devices(); }
    EditorCommandResult applyEditorCommand(const std::string& command);
    ScriptImportResult importScriptArtifact(const std::string& path, const std::string& nameOverride = "");
    ScriptImportResult importScriptProject(const std::string& path);
    ScriptImportResult importScriptPatch(const std::string& path, const std::string& nameOverride = "");
    ScriptImportResult applyScriptCommandFile(const std::string& path);
    ExportResult exportProject(const ExportRequest& request) const;
    ExportResult exportProjectWithTask(const ExportRequest& request);
    AppOperationResult cancelTask(AppTaskId taskId, const std::string& message = "Canceled");
    AppOperationResult clearFinishedTasks();
    AuditionResult auditionCursorStep();
    bool previewCursorStep();

    AppSessionSnapshot snapshot(int gridStartRow = 0, int gridRowCount = -1) const;
    std::vector<AppEvent> eventsSince(std::uint64_t sequence) const;
    std::vector<AppEvent> drainEvents();
    std::uint64_t lastEventSequence() const;

private:
    void replaceSong(Song song, const std::string& projectPath, bool dirty);
    void refreshDiagnostics();
    void setMessage(AppMessageSeverity severity, const std::string& text);
    void emitEvent(AppEventType type, const std::string& message = "", const std::string& path = "", AppTaskId taskId = 0);

    Song song_;
    std::unique_ptr<PatternEditorSession> editor_;
    RealtimePlaybackSession playback_;
    AudioRuntimeSession audioRuntime_;
    AppSettings settings_;
    std::string projectPath_;
    bool dirty_ = false;
    AppMessage lastMessage_;
    std::vector<ProjectDiagnostic> diagnostics_;
    AppTaskManager taskManager_;
    AppEventLog events_;
};

const char* appMessageSeverityName(AppMessageSeverity severity);

} // namespace arachno
