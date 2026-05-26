#pragma once

#include <string>
#include <vector>

namespace arachno {

enum class TrackerFileKind {
    Unknown,
    Project,
    Patch,
    Settings,
    CommandFile
};

struct FileCompatibilityReport {
    TrackerFileKind kind = TrackerFileKind::Unknown;
    std::string path;
    bool exists = false;
    bool readable = false;
    bool compatible = false;
    bool loadable = false;
    int version = -1;
    int supportedVersion = -1;
    std::string error;
    std::vector<std::string> warnings;
};

TrackerFileKind trackerFileKindFromPath(const std::string& path);
const char* trackerFileKindName(TrackerFileKind kind);
FileCompatibilityReport inspectProjectFile(const std::string& path);
FileCompatibilityReport inspectPatchFile(const std::string& path);
FileCompatibilityReport inspectSettingsFile(const std::string& path);
FileCompatibilityReport inspectTrackerFile(const std::string& path);
std::string formatFileCompatibilityReport(const FileCompatibilityReport& report);

} // namespace arachno
