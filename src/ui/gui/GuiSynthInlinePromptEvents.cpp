#include "ui/gui/GuiSynthInlinePromptEvents.h"

#include <algorithm>
#include <cstdlib>

#include <X11/X.h>

#include "ui/gui/GuiFileBrowserOps.h"

namespace arachno {

bool handleSynthInlinePromptButtonPress(
    const GuiSynthInlinePromptEventContext& context,
    int button,
    int x,
    int y) {
    const bool browserMode = context.inlinePromptUsesFileBrowser(context.inlinePrompt.kind);
    bool handled = false;
    if (button == Button1) {
        if (context.synthInlinePromptButtonsVisible
            && context.synthInlinePromptAcceptButton.contains(x, y)) {
            context.executeInlinePrompt();
            handled = true;
        } else if (context.synthInlinePromptButtonsVisible
            && context.synthInlinePromptCancelButton.contains(x, y)) {
            context.cancelInlinePrompt();
            handled = true;
        } else if (browserMode) {
            for (const FileBrowserHit& hit : context.fileBrowserHits) {
                if (!hit.rect.contains(x, y)) {
                    continue;
                }
                if (hit.role == "nav_home") {
                    const char* home = std::getenv("HOME");
                    if (home != nullptr && *home != '\0') {
                        context.fileBrowserDirectory = std::filesystem::path(home);
                        context.refreshFileBrowserEntries();
                    }
                } else if (hit.role == "nav_up") {
                    const std::filesystem::path parent = context.fileBrowserDirectory.parent_path();
                    if (!parent.empty()) {
                        context.fileBrowserDirectory = parent;
                        context.refreshFileBrowserEntries();
                    }
                } else if (hit.role == "nav_refresh") {
                    context.refreshFileBrowserEntries();
                } else if (hit.role == "entry"
                    && hit.index >= 0
                    && hit.index < static_cast<int>(context.fileBrowserEntries.size())) {
                    context.fileBrowserSelected = hit.index;
                    const FileBrowserEntry& entry = context.fileBrowserEntries[static_cast<std::size_t>(hit.index)];
                    if (entry.directory) {
                        context.fileBrowserDirectory = entry.path;
                        context.refreshFileBrowserEntries();
                        context.inlinePrompt.value = context.fileBrowserDirectory.string();
                    } else {
                        context.inlinePrompt.value = entry.path.string();
                        // Double-click a file to confirm it immediately.
                        if (fileBrowserRegisterClickForDoubleClick(hit.index)) {
                            context.executeInlinePrompt();
                        }
                    }
                }
                handled = true;
                break;
            }
        }
    } else if ((button == Button4 || button == Button5) && browserMode) {
        if (context.fileBrowserListRect.contains(x, y)) {
            const int delta = button == Button4 ? -1 : 1;
            context.fileBrowserScroll = std::max(0, context.fileBrowserScroll + delta);
            handled = true;
        }
    }
    return handled || browserMode;
}

bool handleSynthInlinePromptKeyPress(
    const GuiSynthInlinePromptEventContext& context,
    KeySym key,
    bool ctrlDown,
    bool altDown,
    const char* lookupBuffer,
    int lookupCount) {
    const bool browserMode = context.inlinePromptUsesFileBrowser(context.inlinePrompt.kind);
    if (key == XK_Escape) {
        context.cancelInlinePrompt();
        return true;
    }
    if (key == XK_Return || key == XK_KP_Enter) {
        if (browserMode
            && context.fileBrowserSelected >= 0
            && context.fileBrowserSelected < static_cast<int>(context.fileBrowserEntries.size())
            && context.fileBrowserEntries[static_cast<std::size_t>(context.fileBrowserSelected)].directory) {
            context.fileBrowserDirectory = context.fileBrowserEntries[static_cast<std::size_t>(context.fileBrowserSelected)].path;
            context.refreshFileBrowserEntries();
            context.inlinePrompt.value = context.fileBrowserDirectory.string();
        } else {
            context.executeInlinePrompt();
        }
        return true;
    }
    if (browserMode && (key == XK_Up || key == XK_KP_Up)) {
        if (context.fileBrowserEntries.empty()) {
            context.refreshFileBrowserEntries();
        }
        if (!context.fileBrowserEntries.empty()) {
            const int previous = context.fileBrowserSelected < 0 ? 0 : context.fileBrowserSelected;
            context.fileBrowserSelected = std::max(0, previous - 1);
            if (context.fileBrowserSelected < context.fileBrowserScroll) {
                context.fileBrowserScroll = context.fileBrowserSelected;
            }
            context.inlinePrompt.value = context.fileBrowserEntries[static_cast<std::size_t>(context.fileBrowserSelected)].path.string();
        }
        return true;
    }
    if (browserMode && (key == XK_Down || key == XK_KP_Down)) {
        if (context.fileBrowserEntries.empty()) {
            context.refreshFileBrowserEntries();
        }
        if (!context.fileBrowserEntries.empty()) {
            const int previous = context.fileBrowserSelected < 0 ? -1 : context.fileBrowserSelected;
            context.fileBrowserSelected = std::min(static_cast<int>(context.fileBrowserEntries.size()) - 1, previous + 1);
            const int visibleRows = std::max(1, (context.fileBrowserListRect.height - 4) / 18);
            if (context.fileBrowserSelected >= context.fileBrowserScroll + visibleRows) {
                context.fileBrowserScroll = std::max(0, context.fileBrowserSelected - visibleRows + 1);
            }
            context.inlinePrompt.value = context.fileBrowserEntries[static_cast<std::size_t>(context.fileBrowserSelected)].path.string();
        }
        return true;
    }
    if (browserMode && (key == XK_Page_Up || key == XK_KP_Page_Up || key == XK_Page_Down || key == XK_KP_Page_Down
        || key == XK_Home || key == XK_End)) {
        if (context.fileBrowserEntries.empty()) {
            context.refreshFileBrowserEntries();
        }
        if (!context.fileBrowserEntries.empty()) {
            const int last = static_cast<int>(context.fileBrowserEntries.size()) - 1;
            const int visibleRows = std::max(1, (context.fileBrowserListRect.height - 4) / 18);
            const int current = std::clamp(context.fileBrowserSelected, 0, last);
            if (key == XK_Home) {
                context.fileBrowserSelected = 0;
            } else if (key == XK_End) {
                context.fileBrowserSelected = last;
            } else if (key == XK_Page_Up || key == XK_KP_Page_Up) {
                context.fileBrowserSelected = std::max(0, current - visibleRows);
            } else {
                context.fileBrowserSelected = std::min(last, current + visibleRows);
            }
            if (context.fileBrowserSelected < context.fileBrowserScroll) {
                context.fileBrowserScroll = context.fileBrowserSelected;
            }
            if (context.fileBrowserSelected >= context.fileBrowserScroll + visibleRows) {
                context.fileBrowserScroll = std::max(0, context.fileBrowserSelected - visibleRows + 1);
            }
            context.inlinePrompt.value = context.fileBrowserEntries[static_cast<std::size_t>(context.fileBrowserSelected)].path.string();
        }
        return true;
    }
    if (key == XK_BackSpace) {
        if (!context.inlinePrompt.value.empty()) {
            context.inlinePrompt.value.pop_back();
        }
        context.fileBrowserSelected = -1;
        return true;
    }
    if (key == XK_Delete) {
        context.inlinePrompt.value.clear();
        context.fileBrowserSelected = -1;
        return true;
    }
    if (!ctrlDown && !altDown && lookupCount > 0) {
        for (int index = 0; index < lookupCount; ++index) {
            const unsigned char ch = static_cast<unsigned char>(lookupBuffer[index]);
            if (ch >= 32 && ch <= 126) {
                context.inlinePrompt.value.push_back(static_cast<char>(ch));
            }
        }
        context.fileBrowserSelected = -1;
        return true;
    }
    return false;
}

} // namespace arachno
