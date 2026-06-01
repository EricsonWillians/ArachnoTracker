#pragma once

#include <filesystem>
#include <string>

namespace arachno {

std::string defaultProjectPath(bool hasProjectPath, const std::string& projectPath);
std::string defaultMixdownPath(bool hasProjectPath, const std::string& projectPath);
std::string defaultMidiImportPath(bool hasProjectPath, const std::string& projectPath);
std::string patchStemFromName(const std::string& name);
std::string defaultPatchPath(
    bool hasProjectPath,
    const std::string& projectPath,
    const std::string& patchName,
    const std::filesystem::path& workingDir);
std::string defaultPatchBulkPath(
    bool hasProjectPath,
    const std::string& projectPath,
    const std::filesystem::path& workingDir);

} // namespace arachno

