#include "ui/gui/GuiFileBrowserOps.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace arachno {

namespace {

std::string pathLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

} // namespace

bool inlinePromptKindUsesFileBrowser(InlinePromptKind kind) {
    return kind == InlinePromptKind::OpenProjectPath
        || kind == InlinePromptKind::SaveProjectPath
        || kind == InlinePromptKind::ExportMixdownPath
        || kind == InlinePromptKind::ImportMidiPath
        || kind == InlinePromptKind::ImportPatchAsNewPath
        || kind == InlinePromptKind::ImportPatchReplacePath
        || kind == InlinePromptKind::ImportPatchReplaceAllPath
        || kind == InlinePromptKind::ExportPatchPath;
}

std::vector<std::string> fileBrowserExtensionFilter(InlinePromptKind kind) {
    if (kind == InlinePromptKind::OpenProjectPath || kind == InlinePromptKind::SaveProjectPath) {
        return std::vector<std::string> {".arachno", ".mid", ".midi"};
    }
    if (kind == InlinePromptKind::ExportMixdownPath) {
        return std::vector<std::string> {".wav", ".mp3", ".ogg", ".mid", ".midi"};
    }
    if (kind == InlinePromptKind::ImportMidiPath) {
        return std::vector<std::string> {".mid", ".midi"};
    }
    if (kind == InlinePromptKind::ImportPatchAsNewPath
        || kind == InlinePromptKind::ImportPatchReplacePath
        || kind == InlinePromptKind::ImportPatchReplaceAllPath
        || kind == InlinePromptKind::ExportPatchPath) {
        return std::vector<std::string> {".arachnopatch"};
    }
    return std::vector<std::string> {};
}

void refreshFileBrowserEntries(const GuiFileBrowserState& state) {
    state.fileBrowserEntries.clear();
    state.fileBrowserHits.clear();
    state.fileBrowserSelected = -1;
    if (state.fileBrowserDirectory.empty()) {
        state.fileBrowserDirectory = std::filesystem::current_path();
    }

    std::error_code ec;
    const std::filesystem::path canonicalDir = std::filesystem::weakly_canonical(state.fileBrowserDirectory, ec);
    if (!ec && !canonicalDir.empty()) {
        state.fileBrowserDirectory = canonicalDir;
    }

    const std::vector<std::string> extensions = fileBrowserExtensionFilter(state.inlinePrompt.kind);
    std::vector<FileBrowserEntry> directories;
    std::vector<FileBrowserEntry> files;
    for (std::filesystem::directory_iterator it(state.fileBrowserDirectory, ec);
         !ec && it != std::filesystem::directory_iterator();
         it.increment(ec)) {
        const std::filesystem::directory_entry& entry = *it;
        FileBrowserEntry item;
        item.path = entry.path();
        item.name = item.path.filename().string();
        item.directory = entry.is_directory(ec);
        if (ec || item.name.empty()) {
            ec.clear();
            continue;
        }
        if (item.directory) {
            directories.push_back(item);
            continue;
        }
        if (!extensions.empty()) {
            const std::string ext = pathLower(item.path.extension().string());
            bool accepted = false;
            for (const std::string& candidate : extensions) {
                if (ext == candidate) {
                    accepted = true;
                    break;
                }
            }
            if (!accepted) {
                continue;
            }
        }
        files.push_back(item);
    }

    auto byName = [](const FileBrowserEntry& a, const FileBrowserEntry& b) {
        return pathLower(a.name) < pathLower(b.name);
    };
    std::sort(directories.begin(), directories.end(), byName);
    std::sort(files.begin(), files.end(), byName);

    state.fileBrowserEntries.reserve(directories.size() + files.size());
    state.fileBrowserEntries.insert(state.fileBrowserEntries.end(), directories.begin(), directories.end());
    state.fileBrowserEntries.insert(state.fileBrowserEntries.end(), files.begin(), files.end());
    state.fileBrowserScroll = std::clamp(
        state.fileBrowserScroll,
        0,
        std::max(0, static_cast<int>(state.fileBrowserEntries.size()) - 1));
}

void initFileBrowserFromPrompt(const GuiFileBrowserState& state) {
    state.fileBrowserScroll = 0;
    state.fileBrowserSelected = -1;

    std::filesystem::path seed;
    if (!state.inlinePrompt.value.empty()) {
        seed = std::filesystem::path(state.inlinePrompt.value);
    }

    std::error_code ec;
    if (seed.empty()) {
        state.fileBrowserDirectory = std::filesystem::current_path();
    } else if (std::filesystem::is_directory(seed, ec)) {
        state.fileBrowserDirectory = seed;
    } else {
        state.fileBrowserDirectory = seed.parent_path().empty() ? std::filesystem::current_path() : seed.parent_path();
    }

    if (!std::filesystem::exists(state.fileBrowserDirectory, ec)
        || !std::filesystem::is_directory(state.fileBrowserDirectory, ec)) {
        state.fileBrowserDirectory = std::filesystem::current_path();
    }

    refreshFileBrowserEntries(state);
    const std::string targetLeaf = seed.filename().string();
    if (targetLeaf.empty()) {
        return;
    }
    for (int index = 0; index < static_cast<int>(state.fileBrowserEntries.size()); ++index) {
        if (!state.fileBrowserEntries[static_cast<std::size_t>(index)].directory
            && state.fileBrowserEntries[static_cast<std::size_t>(index)].name == targetLeaf) {
            state.fileBrowserSelected = index;
            break;
        }
    }
}

} // namespace arachno
