#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "AudioRuntime.h"
#include "EditorShortcuts.h"

namespace arachno {

constexpr int appSettingsFileVersion = 4;

struct ExportPreferences {
    std::string defaultDirectory;
    std::string audioFormat = "wav";
    int sampleRate = 48000;
    bool renderStems = false;
};

struct UiLayoutPreferences {
    int gridVisibleRows = 32;
    bool showInspector = true;
    bool showBrowser = true;
    bool followPlayback = true;
};

struct AudioRuntimePreferences {
    std::string backend = "auto";
    std::string deviceId;
    int sampleRate = 48000;
    int bufferFrames = 256;
    int periods = 2;
    bool realtimePriority = true;
    bool connectSystemOutputs = true;
};

struct SyncCheckpoint {
    std::string name;
    std::uint64_t eventSequence = 0;
    int taskId = 0;
    std::string projectPath;
    std::string projectFingerprint;
};

struct AppSettings {
    std::vector<std::string> recentProjects;
    ExportPreferences exportDefaults;
    UiLayoutPreferences layout;
    AudioRuntimePreferences audioRuntime;
    std::vector<ShortcutBinding> shortcutOverrides;
    std::vector<SyncCheckpoint> syncCheckpoints;
};

void addRecentProject(AppSettings& settings, const std::string& path, int maxRecent = 10);
std::vector<ShortcutBinding> effectiveEditorShortcuts(const AppSettings& settings);
void validateAppSettings(const AppSettings& settings);
const SyncCheckpoint* findSyncCheckpoint(const AppSettings& settings, const std::string& name);
bool upsertSyncCheckpoint(AppSettings& settings, const SyncCheckpoint& checkpoint, int maxCheckpoints = 64);
bool removeSyncCheckpoint(AppSettings& settings, const std::string& name);
AudioRuntimeSettings audioRuntimeSettingsFromPreferences(const AudioRuntimePreferences& preferences);
AudioRuntimePreferences audioRuntimePreferencesFromSettings(const AudioRuntimeSettings& settings);
void saveAppSettings(const AppSettings& settings, const std::string& path);
AppSettings loadAppSettings(const std::string& path);

} // namespace arachno
