#include "AutoSave.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>

#include "ProjectIO.h"

namespace arachno {

namespace {
std::string sanitizeBaseName(std::string value) {
    if (value.empty()) {
        value = "untitled";
    }

    for (char& ch : value) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (!std::isalnum(uch) && ch != '-' && ch != '_') {
            ch = '_';
        }
    }
    return value;
}
} // namespace

std::string defaultRecoveryFileName(const std::string& projectPath) {
    std::filesystem::path path(projectPath);
    std::string base = path.empty() ? "untitled" : path.stem().string();
    return sanitizeBaseName(base) + ".autosave.arachno";
}

std::string recoveryPathForProject(const std::string& projectPath, const std::string& recoveryDirectory) {
    const std::filesystem::path directory = recoveryDirectory.empty()
        ? std::filesystem::temp_directory_path()
        : std::filesystem::path(recoveryDirectory);
    return (directory / defaultRecoveryFileName(projectPath)).string();
}

RecoveryInfo inspectRecoveryFile(const std::string& path) {
    RecoveryInfo info;
    info.path = path;
    info.exists = std::filesystem::exists(path);
    if (!info.exists) {
        info.error = "recovery file does not exist";
        return info;
    }

    try {
        (void)loadProject(path);
        info.loadable = true;
        info.error.clear();
    } catch (const std::exception& error) {
        info.error = error.what();
    }
    return info;
}

RecoveryResult saveRecoveryFile(const Song& song, const std::string& path) {
    RecoveryResult result;
    result.path = path;
    try {
        const std::filesystem::path output(path);
        if (!output.parent_path().empty()) {
            std::filesystem::create_directories(output.parent_path());
        }
        saveProject(song, path);
        result.ok = true;
        result.message = "Saved recovery file";
    } catch (const std::exception& error) {
        result.error = error.what();
    }
    return result;
}

RecoveryResult clearRecoveryFile(const std::string& path) {
    RecoveryResult result;
    result.path = path;
    try {
        std::error_code error;
        const bool removed = std::filesystem::remove(path, error);
        if (error) {
            result.error = error.message();
            return result;
        }
        result.ok = true;
        result.message = removed ? "Cleared recovery file" : "No recovery file to clear";
    } catch (const std::exception& error) {
        result.error = error.what();
    }
    return result;
}

Song loadRecoveryFile(const std::string& path) {
    return loadProject(path);
}

} // namespace arachno
