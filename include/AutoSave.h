#pragma once

#include <string>

#include "Tracker.h"

namespace arachno {

struct RecoveryInfo {
    std::string path;
    bool exists = false;
    bool loadable = false;
    std::string error;
};

struct RecoveryResult {
    bool ok = false;
    std::string path;
    std::string message;
    std::string error;
};

std::string defaultRecoveryFileName(const std::string& projectPath);
std::string recoveryPathForProject(const std::string& projectPath, const std::string& recoveryDirectory);
RecoveryInfo inspectRecoveryFile(const std::string& path);
RecoveryResult saveRecoveryFile(const Song& song, const std::string& path);
RecoveryResult clearRecoveryFile(const std::string& path);
Song loadRecoveryFile(const std::string& path);

} // namespace arachno
