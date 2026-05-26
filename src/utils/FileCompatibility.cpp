#include "FileCompatibility.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "AppSettings.h"
#include "PatchIO.h"
#include "ProjectIO.h"
#include "ScriptIntegration.h"

namespace arachno {

namespace {
std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

FileCompatibilityReport beginReport(
    const std::string& path,
    TrackerFileKind kind,
    int supportedVersion) {
    FileCompatibilityReport report;
    report.path = path;
    report.kind = kind;
    report.supportedVersion = supportedVersion;
    report.exists = std::filesystem::exists(path);
    if (!report.exists) {
        report.error = "file does not exist";
        return report;
    }
    std::ifstream in(path);
    report.readable = static_cast<bool>(in);
    if (!report.readable) {
        report.error = "file is not readable";
        return report;
    }
    return report;
}

bool readHeaderVersion(
    FileCompatibilityReport& report,
    const std::string& expectedHeader) {
    std::ifstream in(report.path);
    std::string header;
    int version = -1;
    if (!(in >> header >> version)) {
        report.error = "file header is incomplete";
        return false;
    }
    if (header != expectedHeader) {
        report.error = "expected header '" + expectedHeader + "', got '" + header + "'";
        return false;
    }
    report.version = version;
    report.compatible = version <= report.supportedVersion;
    if (!report.compatible) {
        report.error = "file version " + std::to_string(version)
            + " is newer than supported version "
            + std::to_string(report.supportedVersion);
        return false;
    }
    if (version < report.supportedVersion) {
        report.warnings.push_back("file was written by an older format version");
    }
    return true;
}
} // namespace

TrackerFileKind trackerFileKindFromPath(const std::string& path) {
    const std::string extension = lowerCopy(std::filesystem::path(path).extension().string());
    if (extension == ".arachno") {
        return TrackerFileKind::Project;
    }
    if (extension == ".arachnopatch") {
        return TrackerFileKind::Patch;
    }
    if (extension == ".arachnosettings" || extension == ".conf") {
        return TrackerFileKind::Settings;
    }
    if (extension == ".arachno-edit") {
        return TrackerFileKind::CommandFile;
    }
    return TrackerFileKind::Unknown;
}

const char* trackerFileKindName(TrackerFileKind kind) {
    switch (kind) {
        case TrackerFileKind::Unknown:
            return "unknown";
        case TrackerFileKind::Project:
            return "project";
        case TrackerFileKind::Patch:
            return "patch";
        case TrackerFileKind::Settings:
            return "settings";
        case TrackerFileKind::CommandFile:
            return "command-file";
    }
    return "unknown";
}

FileCompatibilityReport inspectProjectFile(const std::string& path) {
    FileCompatibilityReport report = beginReport(path, TrackerFileKind::Project, projectFileVersion);
    if (!report.readable || !readHeaderVersion(report, "arachno_project")) {
        return report;
    }

    try {
        (void)loadProject(path);
        report.loadable = true;
    } catch (const std::exception& error) {
        report.error = error.what();
    }
    return report;
}

FileCompatibilityReport inspectPatchFile(const std::string& path) {
    FileCompatibilityReport report = beginReport(path, TrackerFileKind::Patch, patchFileVersion);
    if (!report.readable || !readHeaderVersion(report, "arachno_patch")) {
        return report;
    }

    try {
        (void)loadPatch(path);
        report.loadable = true;
    } catch (const std::exception& error) {
        report.error = error.what();
    }
    return report;
}

FileCompatibilityReport inspectSettingsFile(const std::string& path) {
    FileCompatibilityReport report = beginReport(path, TrackerFileKind::Settings, appSettingsFileVersion);
    if (!report.readable || !readHeaderVersion(report, "arachno_settings")) {
        return report;
    }

    try {
        (void)loadAppSettings(path);
        report.loadable = true;
    } catch (const std::exception& error) {
        report.error = error.what();
    }
    return report;
}

FileCompatibilityReport inspectTrackerFile(const std::string& path) {
    switch (trackerFileKindFromPath(path)) {
        case TrackerFileKind::Project:
            return inspectProjectFile(path);
        case TrackerFileKind::Patch:
            return inspectPatchFile(path);
        case TrackerFileKind::Settings:
            return inspectSettingsFile(path);
        case TrackerFileKind::CommandFile: {
            FileCompatibilityReport report = beginReport(path, TrackerFileKind::CommandFile, 0);
            if (!report.readable) {
                return report;
            }
            try {
                (void)loadScriptCommandFile(path);
                report.compatible = true;
                report.loadable = true;
            } catch (const std::exception& error) {
                report.error = error.what();
            }
            return report;
        }
        case TrackerFileKind::Unknown:
            break;
    }

    FileCompatibilityReport report = beginReport(path, TrackerFileKind::Unknown, -1);
    if (report.exists && report.readable) {
        report.error = "unsupported file extension";
    }
    return report;
}

std::string formatFileCompatibilityReport(const FileCompatibilityReport& report) {
    std::ostringstream out;
    out << "File compatibility: " << trackerFileKindName(report.kind) << "\n";
    out << "path: " << report.path << "\n";
    out << "exists: " << (report.exists ? "yes" : "no") << "\n";
    out << "readable: " << (report.readable ? "yes" : "no") << "\n";
    if (report.version >= 0) {
        out << "version: " << report.version << " / supported " << report.supportedVersion << "\n";
    }
    out << "compatible: " << (report.compatible ? "yes" : "no") << "\n";
    out << "loadable: " << (report.loadable ? "yes" : "no") << "\n";
    for (const std::string& warning : report.warnings) {
        out << "warning: " << warning << "\n";
    }
    if (!report.error.empty()) {
        out << "error: " << report.error << "\n";
    }
    return out.str();
}

} // namespace arachno
