#pragma once

#include <string>
#include <utility>
#include <vector>

namespace arachno {

enum class ScriptArtifactType {
    Unknown,
    Project,
    Patch,
    CommandFile
};

struct ScriptCommand {
    std::string executable = "python3";
    std::vector<std::string> arguments;
    std::string workingDirectory;
    std::vector<std::pair<std::string, std::string>> environment;
};

struct ScriptArtifactInfo {
    ScriptArtifactType type = ScriptArtifactType::Unknown;
    std::string path;
    bool exists = false;
    bool loadable = false;
    std::string error;
};

struct ScriptImportResult {
    bool ok = false;
    ScriptArtifactType type = ScriptArtifactType::Unknown;
    std::string path;
    std::string message;
    std::string error;
    int importedInstrument = -1;
    int appliedCommandCount = 0;
};

ScriptCommand buildPythonNewProjectCommand(
    const std::string& outputPath,
    const std::string& title,
    double bpm,
    const std::string& pythonExecutable = "python3",
    const std::string& sdkPath = "python");

ScriptCommand buildPythonPatchPluginCommand(
    const std::string& pluginPath,
    const std::string& outputPath,
    const std::string& patchName = "",
    const std::string& factory = "create_patch",
    const std::string& pythonExecutable = "python3",
    const std::string& sdkPath = "python");

std::string formatScriptCommand(const ScriptCommand& command);
ScriptArtifactType scriptArtifactTypeFromPath(const std::string& path);
const char* scriptArtifactTypeName(ScriptArtifactType type);
ScriptArtifactInfo inspectScriptArtifact(const std::string& path);
std::vector<std::string> loadScriptCommandFile(const std::string& path);

} // namespace arachno
