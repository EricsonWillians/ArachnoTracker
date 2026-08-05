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
// True for all patch import/export prompt kinds.
bool inlinePromptKindIsPatch(InlinePromptKind kind);
// True for single-file patch import kinds, where selecting a file previews it.
bool inlinePromptKindPreviewsPatchFile(InlinePromptKind kind);
std::vector<std::string> fileBrowserExtensionFilter(InlinePromptKind kind);
void refreshFileBrowserEntries(const GuiFileBrowserState& state);
void initFileBrowserFromPrompt(const GuiFileBrowserState& state);

// Click-streak helper for double-click-to-confirm in the file browser. Returns true
// when the same entry index is clicked twice within `windowMs`. State is function-local
// (the GUI runs a single X11 event thread).
bool fileBrowserRegisterClickForDoubleClick(int entryIndex, int windowMs = 500);

} // namespace arachno
