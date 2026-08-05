#include "AppSettings.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace arachno {

namespace {
bool parseBool(int value) {
    return value != 0;
}

void expectKey(const std::string& actual, const std::string& expected, int line) {
    if (actual != expected) {
        throw std::runtime_error("settings line " + std::to_string(line) + ": expected " + expected + ", got " + actual);
    }
}

bool validAudioFormat(const std::string& format) {
    return format == "wav" || format == "mp3" || format == "ogg";
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool validAudioBackend(const std::string& backend) {
    const std::string normalized = lowercase(backend);
    return normalized == "auto"
        || normalized == "jack"
        || normalized == "pipewire"
        || normalized == "pw"
        || normalized == "alsa"
        || normalized == "dummy";
}

bool validSyncCheckpointName(const std::string& name) {
    return !name.empty();
}
} // namespace

void addRecentProject(AppSettings& settings, const std::string& path, int maxRecent) {
    if (path.empty() || maxRecent <= 0) {
        return;
    }

    settings.recentProjects.erase(
        std::remove(settings.recentProjects.begin(), settings.recentProjects.end(), path),
        settings.recentProjects.end());
    settings.recentProjects.insert(settings.recentProjects.begin(), path);
    if (static_cast<int>(settings.recentProjects.size()) > maxRecent) {
        settings.recentProjects.resize(static_cast<std::size_t>(maxRecent));
    }
}

std::vector<ShortcutBinding> effectiveEditorShortcuts(const AppSettings& settings) {
    std::vector<ShortcutBinding> bindings = defaultEditorShortcuts();
    for (const ShortcutBinding& override : settings.shortcutOverrides) {
        bindings.erase(
            std::remove_if(bindings.begin(), bindings.end(), [&](const ShortcutBinding& binding) {
                return binding.actionId == override.actionId;
            }),
            bindings.end());
        if (!normalizeShortcut(override.shortcut).empty()) {
            bindings.push_back({normalizeShortcut(override.shortcut), override.actionId});
        }
    }
    validateEditorShortcuts(bindings);
    return bindings;
}

void validateAppSettings(const AppSettings& settings) {
    if (!validAudioFormat(settings.exportDefaults.audioFormat)) {
        throw std::invalid_argument("unsupported default audio format: " + settings.exportDefaults.audioFormat);
    }
    if (settings.exportDefaults.sampleRate <= 0) {
        throw std::invalid_argument("default sample rate must be positive");
    }
    if (settings.layout.gridVisibleRows <= 0) {
        throw std::invalid_argument("grid visible rows must be positive");
    }
    if (!validAudioBackend(settings.audioRuntime.backend)) {
        throw std::invalid_argument("unsupported audio backend: " + settings.audioRuntime.backend);
    }
    if (settings.audioRuntime.sampleRate <= 0) {
        throw std::invalid_argument("audio runtime sample rate must be positive");
    }
    if (settings.audioRuntime.bufferFrames <= 0) {
        throw std::invalid_argument("audio runtime buffer frames must be positive");
    }
    if (settings.audioRuntime.periods <= 0) {
        throw std::invalid_argument("audio runtime periods must be positive");
    }
    for (const SyncCheckpoint& checkpoint : settings.syncCheckpoints) {
        if (!validSyncCheckpointName(checkpoint.name)) {
            throw std::invalid_argument("sync checkpoint name must not be empty");
        }
        if (checkpoint.taskId < 0) {
            throw std::invalid_argument("sync checkpoint task id must not be negative");
        }
        if (checkpoint.projectFingerprint.empty() && !checkpoint.projectPath.empty()) {
            throw std::invalid_argument("sync checkpoint fingerprint must be set when project path is set");
        }
    }
    (void)effectiveEditorShortcuts(settings);
}

const SyncCheckpoint* findSyncCheckpoint(const AppSettings& settings, const std::string& name) {
    const auto it = std::find_if(settings.syncCheckpoints.begin(), settings.syncCheckpoints.end(), [&](const SyncCheckpoint& checkpoint) {
        return checkpoint.name == name;
    });
    return it == settings.syncCheckpoints.end() ? nullptr : &(*it);
}

bool upsertSyncCheckpoint(AppSettings& settings, const SyncCheckpoint& checkpoint, int maxCheckpoints) {
    if (!validSyncCheckpointName(checkpoint.name) || checkpoint.taskId < 0 || maxCheckpoints <= 0) {
        return false;
    }
    auto it = std::find_if(settings.syncCheckpoints.begin(), settings.syncCheckpoints.end(), [&](const SyncCheckpoint& existing) {
        return existing.name == checkpoint.name;
    });
    if (it != settings.syncCheckpoints.end()) {
        *it = checkpoint;
        return true;
    }
    settings.syncCheckpoints.push_back(checkpoint);
    if (static_cast<int>(settings.syncCheckpoints.size()) > maxCheckpoints) {
        settings.syncCheckpoints.erase(settings.syncCheckpoints.begin());
    }
    return true;
}

bool removeSyncCheckpoint(AppSettings& settings, const std::string& name) {
    const std::size_t before = settings.syncCheckpoints.size();
    settings.syncCheckpoints.erase(
        std::remove_if(settings.syncCheckpoints.begin(), settings.syncCheckpoints.end(), [&](const SyncCheckpoint& checkpoint) {
            return checkpoint.name == name;
        }),
        settings.syncCheckpoints.end());
    return settings.syncCheckpoints.size() != before;
}

AudioRuntimeSettings audioRuntimeSettingsFromPreferences(const AudioRuntimePreferences& preferences) {
    AudioRuntimeSettings settings;
    settings.backend = audioBackendFromName(preferences.backend);
    settings.deviceId = preferences.deviceId;
    settings.sampleRate = preferences.sampleRate;
    settings.bufferFrames = preferences.bufferFrames;
    settings.periods = preferences.periods;
    settings.realtimePriority = preferences.realtimePriority;
    settings.connectSystemOutputs = preferences.connectSystemOutputs;
    return settings;
}

AudioRuntimePreferences audioRuntimePreferencesFromSettings(const AudioRuntimeSettings& settings) {
    AudioRuntimePreferences preferences;
    preferences.backend = audioBackendName(settings.backend);
    preferences.deviceId = settings.deviceId;
    preferences.sampleRate = settings.sampleRate;
    preferences.bufferFrames = settings.bufferFrames;
    preferences.periods = settings.periods;
    preferences.realtimePriority = settings.realtimePriority;
    preferences.connectSystemOutputs = settings.connectSystemOutputs;
    return preferences;
}

void saveAppSettings(const AppSettings& settings, const std::string& path) {
    validateAppSettings(settings);

    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("could not open settings file for writing: " + path);
    }

    out << "arachno_settings " << appSettingsFileVersion << "\n";
    out << "default_export_dir " << std::quoted(settings.exportDefaults.defaultDirectory) << "\n";
    out << "default_audio_format " << settings.exportDefaults.audioFormat << "\n";
    out << "default_sample_rate " << settings.exportDefaults.sampleRate << "\n";
    out << "render_stems " << (settings.exportDefaults.renderStems ? 1 : 0) << "\n";
    out << "grid_visible_rows " << settings.layout.gridVisibleRows << "\n";
    out << "show_inspector " << (settings.layout.showInspector ? 1 : 0) << "\n";
    out << "show_browser " << (settings.layout.showBrowser ? 1 : 0) << "\n";
    out << "follow_playback " << (settings.layout.followPlayback ? 1 : 0) << "\n";
    out << "audio_backend " << settings.audioRuntime.backend << "\n";
    out << "audio_device " << std::quoted(settings.audioRuntime.deviceId) << "\n";
    out << "audio_sample_rate " << settings.audioRuntime.sampleRate << "\n";
    out << "audio_buffer_frames " << settings.audioRuntime.bufferFrames << "\n";
    out << "audio_periods " << settings.audioRuntime.periods << "\n";
    out << "audio_realtime_priority " << (settings.audioRuntime.realtimePriority ? 1 : 0) << "\n";
    out << "audio_connect_outputs " << (settings.audioRuntime.connectSystemOutputs ? 1 : 0) << "\n";
    out << "recent_count " << settings.recentProjects.size() << "\n";
    for (const std::string& recent : settings.recentProjects) {
        out << "recent " << std::quoted(recent) << "\n";
    }
    out << "shortcut_count " << settings.shortcutOverrides.size() << "\n";
    for (const ShortcutBinding& binding : settings.shortcutOverrides) {
        out << "shortcut " << std::quoted(normalizeShortcut(binding.shortcut)) << " " << std::quoted(binding.actionId) << "\n";
    }
    out << "checkpoint_count " << settings.syncCheckpoints.size() << "\n";
    for (const SyncCheckpoint& checkpoint : settings.syncCheckpoints) {
        out << "checkpoint "
            << std::quoted(checkpoint.name)
            << " "
            << static_cast<unsigned long long>(checkpoint.eventSequence)
            << " "
            << checkpoint.taskId
            << " "
            << std::quoted(checkpoint.projectPath)
            << " "
            << std::quoted(checkpoint.projectFingerprint)
            << "\n";
    }
    out << "last_patch_dir " << std::quoted(settings.browser.lastPatchDirectory) << "\n";
    out << "end_settings\n";
}

AppSettings loadAppSettings(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("could not open settings file: " + path);
    }

    AppSettings settings;
    std::string key;
    int line = 1;
    int version = 0;
    in >> key >> version;
    expectKey(key, "arachno_settings", line);
    if (version > appSettingsFileVersion) {
        throw std::runtime_error("unsupported settings version: " + std::to_string(version));
    }

    ++line;
    in >> key >> std::quoted(settings.exportDefaults.defaultDirectory);
    expectKey(key, "default_export_dir", line);
    ++line;
    in >> key >> settings.exportDefaults.audioFormat;
    expectKey(key, "default_audio_format", line);
    ++line;
    in >> key >> settings.exportDefaults.sampleRate;
    expectKey(key, "default_sample_rate", line);
    int boolValue = 0;
    ++line;
    in >> key >> boolValue;
    expectKey(key, "render_stems", line);
    settings.exportDefaults.renderStems = parseBool(boolValue);
    ++line;
    in >> key >> settings.layout.gridVisibleRows;
    expectKey(key, "grid_visible_rows", line);
    ++line;
    in >> key >> boolValue;
    expectKey(key, "show_inspector", line);
    settings.layout.showInspector = parseBool(boolValue);
    ++line;
    in >> key >> boolValue;
    expectKey(key, "show_browser", line);
    settings.layout.showBrowser = parseBool(boolValue);
    ++line;
    in >> key >> boolValue;
    expectKey(key, "follow_playback", line);
    settings.layout.followPlayback = parseBool(boolValue);

    if (version >= 2) {
        ++line;
        in >> key >> settings.audioRuntime.backend;
        expectKey(key, "audio_backend", line);
        ++line;
        in >> key >> std::quoted(settings.audioRuntime.deviceId);
        expectKey(key, "audio_device", line);
        ++line;
        in >> key >> settings.audioRuntime.sampleRate;
        expectKey(key, "audio_sample_rate", line);
        ++line;
        in >> key >> settings.audioRuntime.bufferFrames;
        expectKey(key, "audio_buffer_frames", line);
        ++line;
        in >> key >> settings.audioRuntime.periods;
        expectKey(key, "audio_periods", line);
        ++line;
        in >> key >> boolValue;
        expectKey(key, "audio_realtime_priority", line);
        settings.audioRuntime.realtimePriority = parseBool(boolValue);
        ++line;
        in >> key >> boolValue;
        expectKey(key, "audio_connect_outputs", line);
        settings.audioRuntime.connectSystemOutputs = parseBool(boolValue);
    } else {
        settings.audioRuntime = AudioRuntimePreferences {};
    }

    int count = 0;
    ++line;
    in >> key >> count;
    expectKey(key, "recent_count", line);
    if (count < 0) {
        throw std::runtime_error("settings recent_count must not be negative");
    }
    for (int index = 0; index < count; ++index) {
        std::string recent;
        ++line;
        in >> key >> std::quoted(recent);
        expectKey(key, "recent", line);
        settings.recentProjects.push_back(recent);
    }

    ++line;
    in >> key >> count;
    expectKey(key, "shortcut_count", line);
    if (count < 0) {
        throw std::runtime_error("settings shortcut_count must not be negative");
    }
    for (int index = 0; index < count; ++index) {
        ShortcutBinding binding;
        ++line;
        in >> key >> std::quoted(binding.shortcut) >> std::quoted(binding.actionId);
        expectKey(key, "shortcut", line);
        binding.shortcut = normalizeShortcut(binding.shortcut);
        settings.shortcutOverrides.push_back(binding);
    }

    if (version >= 3) {
        ++line;
        in >> key >> count;
        expectKey(key, "checkpoint_count", line);
        if (count < 0) {
            throw std::runtime_error("settings checkpoint_count must not be negative");
        }
        for (int index = 0; index < count; ++index) {
            SyncCheckpoint checkpoint;
            unsigned long long eventSequence = 0;
            ++line;
            in >> key >> std::quoted(checkpoint.name) >> eventSequence >> checkpoint.taskId;
            expectKey(key, "checkpoint", line);
            checkpoint.eventSequence = static_cast<std::uint64_t>(eventSequence);
            if (version >= 4) {
                in >> std::quoted(checkpoint.projectPath) >> std::quoted(checkpoint.projectFingerprint);
            } else {
                checkpoint.projectPath.clear();
                checkpoint.projectFingerprint.clear();
            }
            settings.syncCheckpoints.push_back(checkpoint);
        }
    } else {
        settings.syncCheckpoints.clear();
    }

    if (version >= 5) {
        ++line;
        in >> key >> std::quoted(settings.browser.lastPatchDirectory);
        expectKey(key, "last_patch_dir", line);
    }

    ++line;
    in >> key;
    expectKey(key, "end_settings", line);
    validateAppSettings(settings);
    return settings;
}

} // namespace arachno
