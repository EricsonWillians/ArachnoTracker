#pragma once

#include <map>
#include <cstdint>
#include <string>
#include <vector>

#include "ApplicationSession.h"
#include "EditorCommandPalette.h"
#include "ProjectLifecycle.h"

namespace arachno {

enum class AppActionKind {
    Application,
    Editor
};

enum class SyncCheckpointCompatibility {
    NotChecked,
    Ok,
    Missing,
    StalePath,
    StaleFingerprint
};

enum class SyncCheckpointUpdateStatus {
    NotChecked,
    Advanced,
    SkippedUpdateDisabled,
    SkippedUnsafeDelta,
    SkippedStalePolicy,
    Failed
};

enum class AppActionParameterType {
    Text,
    Integer,
    Number,
    Boolean,
    NoteName,
    FilePath,
    Choice
};

struct AppActionParameter {
    std::string name;
    std::string label;
    AppActionParameterType type = AppActionParameterType::Text;
    bool required = true;
    std::string defaultValue;
    std::vector<std::string> choices;
    bool hasMinimum = false;
    bool hasMaximum = false;
    double minimum = 0.0;
    double maximum = 0.0;
    std::string description;
};

struct AppActionSchema {
    std::string actionId;
    std::string label;
    std::string commandTemplate;
    AppActionKind kind = AppActionKind::Application;
    std::vector<AppActionParameter> parameters;
    bool requiresPath = false;
    bool requiresCommandText = false;
};

struct AppActionCommandBuild {
    bool ok = false;
    std::string commandText;
    std::string error;
    std::vector<std::string> missingParameters;
};

struct AppActionParameterError {
    std::string name;
    std::string message;
};

struct AppActionValidationResult {
    bool ok = false;
    std::string error;
    std::vector<std::string> missingParameters;
    std::vector<AppActionParameterError> fieldErrors;
};

struct AppAction {
    std::string id;
    std::string label;
    std::string category;
    std::string description;
    std::string defaultShortcut;
    AppActionKind kind = AppActionKind::Application;
    bool mutatesProject = false;
    bool requiresPath = false;
    bool requiresCommandText = false;
    int parameterCount = 0;
};

struct AppActionState {
    std::string id;
    bool enabled = true;
    std::string disabledReason;
};

struct AppActionEntry {
    std::string id;
    std::string label;
    std::string category;
    std::string shortcut;
    std::string description;
    AppActionKind kind = AppActionKind::Application;
    bool enabled = true;
    std::string disabledReason;
    bool mutatesProject = false;
    bool requiresPath = false;
    bool requiresCommandText = false;
    int parameterCount = 0;
};

struct AppActionRequest {
    std::string actionId;
    std::string path;
    std::string commandText;
    std::map<std::string, std::string> parameters;
    std::string recoveryDirectory;
    UnsavedChangesChoice unsavedChoice = UnsavedChangesChoice::NotNeeded;
};

struct AppActionResult {
    bool ok = false;
    std::string actionId;
    std::string message;
    std::string error;
    bool projectChanged = false;
    bool editorStateChanged = false;
    bool requiresUnsavedDecision = false;
    bool requiresSaveAs = false;
    bool shouldOfferRecovery = false;
    bool hasMessageSeverity = false;
    AppMessageSeverity messageSeverity = AppMessageSeverity::Info;
    bool hasAudioRuntime = false;
    AudioRuntimeHealth audioRuntime;
    bool hasAudioDevices = false;
    std::vector<AudioDeviceInfo> audioDevices;
    bool hasAudioRenderTest = false;
    int audioRenderFrameCount = 0;
    int audioRenderBlockCount = 0;
    int audioRenderProcessedFrames = 0;
    int audioRenderProcessedBlocks = 0;
    int audioRenderUnderrunsBefore = 0;
    int audioRenderUnderrunsAfter = 0;
    bool hasSessionSnapshot = false;
    AppSessionSnapshot sessionSnapshot;
    bool hasExportResult = false;
    ExportResult exportResult;
    bool hasScriptImportResult = false;
    ScriptImportResult scriptImportResult;
    bool hasMidiImportReport = false;
    MidiImportReport midiImportReport;
    bool hasSyncCheckpoint = false;
    SyncCheckpoint syncCheckpoint;
    bool hasSuggestedSyncCheckpoint = false;
    SyncCheckpoint suggestedSyncCheckpoint;
    bool suggestedSyncCheckpointSafeToCommit = false;
    bool hasSyncCheckpoints = false;
    std::vector<SyncCheckpoint> syncCheckpoints;
    bool hasSyncCheckpointCompatibility = false;
    SyncCheckpointCompatibility syncCheckpointCompatibility = SyncCheckpointCompatibility::NotChecked;
    bool syncCheckpointStale = false;
    std::string syncCheckpointStaleReason;
    bool hasSyncCheckpointUpdateStatus = false;
    SyncCheckpointUpdateStatus syncCheckpointUpdateStatus = SyncCheckpointUpdateStatus::NotChecked;
    std::string syncCheckpointUpdateReason;
    struct EventDeltaSummary {
        int total = 0;
        int returned = 0;
        int dropped = 0;
        bool truncated = false;
        int project = 0;
        int settings = 0;
        int editor = 0;
        int diagnostics = 0;
        int playback = 0;
        int audio = 0;
        int message = 0;
        int tasks = 0;
        int script = 0;
        int recovery = 0;
        int other = 0;
    };
    bool hasEventDeltaSummary = false;
    EventDeltaSummary eventDeltaSummary;
    struct EventCursor {
        std::uint64_t requestedSince = 0;
        std::uint64_t recommendedSince = 0;
        bool hasReturnedRange = false;
        std::uint64_t returnedFrom = 0;
        std::uint64_t returnedTo = 0;
        bool includesSnapshot = false;
        bool truncated = false;
        bool drain = false;
    };
    bool hasEventCursor = false;
    EventCursor eventCursor;
    bool hasEvents = false;
    std::vector<AppEvent> events;
    bool hasLastEventSequence = false;
    std::uint64_t lastEventSequence = 0;
    ProjectLifecyclePlan lifecyclePlan;
};

const char* appActionKindName(AppActionKind kind);
const char* syncCheckpointCompatibilityName(SyncCheckpointCompatibility compatibility);
const char* syncCheckpointUpdateStatusName(SyncCheckpointUpdateStatus status);
const char* appActionParameterTypeName(AppActionParameterType type);
const std::vector<AppAction>& applicationActions();
const AppAction* findApplicationAction(const std::string& id);
AppActionSchema buildAppActionSchema(const std::string& actionId);
AppActionValidationResult validateAppActionParameters(
    const std::string& actionId,
    const std::map<std::string, std::string>& parameters);
AppActionCommandBuild buildCommandForAppAction(
    const std::string& actionId,
    const std::map<std::string, std::string>& parameters);
std::vector<AppActionState> buildApplicationActionStates(const ApplicationSession& session);
std::vector<AppActionEntry> buildApplicationActionPalette(
    const ApplicationSession& session,
    const std::string& query = "");
AppActionResult executeAppAction(ApplicationSession& session, const AppActionRequest& request);
std::string renderApplicationActionPalette(const std::vector<AppActionEntry>& entries);
std::string renderAppActionSchema(const AppActionSchema& schema);
std::string renderApplicationActionResult(const AppActionResult& result);
std::string serializeApplicationActionResult(const AppActionResult& result);

} // namespace arachno
