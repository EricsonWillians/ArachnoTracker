#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiFileBrowserState {
    InlinePromptState& inlinePrompt;
    std::filesystem::path& fileBrowserDirectory;
    std::vector<FileBrowserEntry>& fileBrowserEntries;
    std::vector<FileBrowserHit>& fileBrowserHits;
    int& fileBrowserScroll;
    int& fileBrowserSelected;
};

bool inlinePromptKindUsesFileBrowser(InlinePromptKind kind);
std::vector<std::string> fileBrowserExtensionFilter(InlinePromptKind kind);
void refreshFileBrowserEntries(const GuiFileBrowserState& state);
void initFileBrowserFromPrompt(const GuiFileBrowserState& state);

} // namespace arachno
