#include "ui/gui/GuiPathDefaults.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

#include "ui/gui/GuiInstrumentBrowserOps.h"

namespace arachno {

std::string defaultProjectPath(bool hasProjectPath, const std::string& projectPath) {
    if (hasProjectPath && !projectPath.empty()) {
        return projectPath;
    }
    return "gui_project.arachno";
}

std::string defaultMixdownPath(bool hasProjectPath, const std::string& projectPath) {
    if (hasProjectPath && !projectPath.empty()) {
        const std::filesystem::path path(projectPath);
        const std::string stem = path.stem().string().empty() ? "mixdown" : path.stem().string();
        return (path.parent_path() / (stem + ".wav")).string();
    }
    return "gui_mixdown.wav";
}

std::string defaultMidiImportPath(bool hasProjectPath, const std::string& projectPath) {
    if (hasProjectPath && !projectPath.empty()) {
        const std::filesystem::path path(projectPath);
        return (path.parent_path() / "import.mid").string();
    }
    return "import.mid";
}

std::string patchStemFromName(const std::string& name) {
    std::string stem;
    stem.reserve(name.size());
    for (char ch : name) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isalnum(uch)) {
            stem.push_back(static_cast<char>(std::tolower(uch)));
        } else if (ch == '_' || ch == '-' || ch == '.') {
            stem.push_back(ch);
        } else if (!stem.empty() && stem.back() != '_') {
            stem.push_back('_');
        }
    }
    if (stem.empty()) {
        stem = "patch";
    }
    return stem;
}

std::string defaultPatchPath(
    bool hasProjectPath,
    const std::string& projectPath,
    const std::string& patchName,
    const std::filesystem::path& workingDir) {
    const std::string stem = patchStemFromName(patchName);
    const std::filesystem::path factoryDir = workingDir / "patches" / "factory";
    std::error_code ec;
    if (std::filesystem::exists(factoryDir, ec) && std::filesystem::is_directory(factoryDir, ec)) {
        // Route saves into the matching category subfolder when one exists.
        const std::string category = instrumentCategoryFromName(patchName);
        const char* subfolder = nullptr;
        if (category == "Percussion") {
            subfolder = "drums";
        } else if (category == "Bass") {
            subfolder = "bass";
        } else if (category == "Lead") {
            subfolder = "lead";
        } else if (category == "Pads") {
            subfolder = "pads";
        } else if (category == "Strings") {
            subfolder = "strings";
        } else if (category == "FX") {
            subfolder = "fx";
        }
        if (subfolder != nullptr) {
            const std::filesystem::path categoryDir = factoryDir / subfolder;
            if (std::filesystem::exists(categoryDir, ec) && std::filesystem::is_directory(categoryDir, ec)) {
                return (categoryDir / (stem + ".arachnopatch")).string();
            }
        }
        return (factoryDir / (stem + ".arachnopatch")).string();
    }
    if (hasProjectPath && !projectPath.empty()) {
        const std::filesystem::path projectPathFs(projectPath);
        return (projectPathFs.parent_path() / (stem + ".arachnopatch")).string();
    }
    return stem + ".arachnopatch";
}

std::string defaultPatchBulkPath(
    bool hasProjectPath,
    const std::string& projectPath,
    const std::filesystem::path& workingDir) {
    const std::filesystem::path factoryDir = workingDir / "patches" / "factory";
    std::error_code ec;
    if (std::filesystem::exists(factoryDir, ec) && std::filesystem::is_directory(factoryDir, ec)) {
        return factoryDir.string();
    }
    if (hasProjectPath && !projectPath.empty()) {
        const std::filesystem::path projectPathFs(projectPath);
        return projectPathFs.parent_path().string();
    }
    return workingDir.string();
}

std::filesystem::path defaultSettingsPath() {
    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
    if (xdgConfig != nullptr && *xdgConfig != '\0') {
        return std::filesystem::path(xdgConfig) / "arachnotracker" / "settings.txt";
    }
    const char* home = std::getenv("HOME");
    if (home != nullptr && *home != '\0') {
        return std::filesystem::path(home) / ".config" / "arachnotracker" / "settings.txt";
    }
    return std::filesystem::path("arachno_settings.txt");
}

} // namespace arachno

