#include "ScriptIntegration.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "PatchIO.h"
#include "ProjectIO.h"

namespace arachno {

namespace {
std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string trim(const std::string& value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string shellQuote(const std::string& value) {
    if (value.empty()) {
        return "''";
    }
    const bool plain = std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return std::isalnum(ch) || ch == '_' || ch == '-' || ch == '.' || ch == '/' || ch == ':';
    });
    if (plain) {
        return value;
    }

    std::string quoted = "'";
    for (char ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted.push_back(ch);
        }
    }
    quoted += "'";
    return quoted;
}
} // namespace

ScriptCommand buildPythonNewProjectCommand(
    const std::string& outputPath,
    const std::string& title,
    double bpm,
    const std::string& pythonExecutable,
    const std::string& sdkPath) {
    ScriptCommand command;
    command.executable = pythonExecutable;
    command.arguments = {
        "-m",
        "arachnotracker",
        "new-ebm",
        outputPath,
        "--title",
        title,
        "--bpm",
        std::to_string(bpm),
    };
    command.environment.push_back({"PYTHONPATH", sdkPath});
    return command;
}

ScriptCommand buildPythonPatchPluginCommand(
    const std::string& pluginPath,
    const std::string& outputPath,
    const std::string& patchName,
    const std::string& factory,
    const std::string& pythonExecutable,
    const std::string& sdkPath) {
    ScriptCommand command;
    command.executable = pythonExecutable;
    command.arguments = {
        "-m",
        "arachnotracker",
        "patch-plugin",
        pluginPath,
        outputPath,
        "--factory",
        factory,
    };
    if (!patchName.empty()) {
        command.arguments.push_back("--name");
        command.arguments.push_back(patchName);
    }
    command.environment.push_back({"PYTHONPATH", sdkPath});
    return command;
}

std::string formatScriptCommand(const ScriptCommand& command) {
    std::ostringstream out;
    for (const auto& [name, value] : command.environment) {
        out << name << "=" << shellQuote(value) << " ";
    }
    out << shellQuote(command.executable);
    for (const std::string& argument : command.arguments) {
        out << " " << shellQuote(argument);
    }
    return out.str();
}

ScriptArtifactType scriptArtifactTypeFromPath(const std::string& path) {
    const std::string extension = lowerCopy(std::filesystem::path(path).extension().string());
    if (extension == ".arachno") {
        return ScriptArtifactType::Project;
    }
    if (extension == ".arachnopatch") {
        return ScriptArtifactType::Patch;
    }
    if (extension == ".arachno-edit") {
        return ScriptArtifactType::CommandFile;
    }
    return ScriptArtifactType::Unknown;
}

const char* scriptArtifactTypeName(ScriptArtifactType type) {
    switch (type) {
        case ScriptArtifactType::Unknown:
            return "unknown";
        case ScriptArtifactType::Project:
            return "project";
        case ScriptArtifactType::Patch:
            return "patch";
        case ScriptArtifactType::CommandFile:
            return "command-file";
    }
    return "unknown";
}

ScriptArtifactInfo inspectScriptArtifact(const std::string& path) {
    ScriptArtifactInfo info;
    info.path = path;
    info.type = scriptArtifactTypeFromPath(path);
    info.exists = std::filesystem::exists(path);
    if (!info.exists) {
        info.error = "script artifact does not exist";
        return info;
    }

    try {
        if (info.type == ScriptArtifactType::Project) {
            (void)loadProject(path);
            info.loadable = true;
        } else if (info.type == ScriptArtifactType::Patch) {
            (void)loadPatch(path);
            info.loadable = true;
        } else if (info.type == ScriptArtifactType::CommandFile) {
            (void)loadScriptCommandFile(path);
            info.loadable = true;
        } else {
            info.error = "unknown script artifact type";
        }
    } catch (const std::exception& error) {
        info.error = error.what();
    }
    return info;
}

std::vector<std::string> loadScriptCommandFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("failed to open script command file: " + path);
    }

    std::vector<std::string> commands;
    std::string line;
    while (std::getline(in, line)) {
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed.front() == '#') {
            continue;
        }
        commands.push_back(trimmed);
    }
    return commands;
}

} // namespace arachno
