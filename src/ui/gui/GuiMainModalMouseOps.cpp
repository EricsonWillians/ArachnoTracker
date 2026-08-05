#include "ui/gui/GuiMainModalMouseOps.h"

#include <algorithm>
#include <cstdlib>

#include "ui/gui/GuiFileBrowserOps.h"

namespace arachno {

GuiMainModalMouseResult handleMainModalButtonPress(
    const GuiMainModalMouseContext& context,
    int button,
    int mx,
    int my,
    unsigned int stateMask) {
    GuiMainModalMouseResult result;

    if (button == Button1 && context.unsavedPrompt.active) {
        for (const auto& choice : context.unsavedPromptChoices) {
            if (!choice.first.contains(mx, my)) {
                continue;
            }
            context.resolveUnsavedPrompt(choice.second);
            break;
        }
        // Clicks outside the buttons keep the dialog open instead of silently
        // cancelling the pending action (use CANCEL or Escape to dismiss).
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }

    if (context.instrumentBrowserActive) {
        if (button == Button1) {
            if (context.instrumentBrowserAcceptButton.contains(mx, my)) {
                context.closeInstrumentBrowser(true);
            } else if (context.instrumentBrowserCancelButton.contains(mx, my)) {
                context.closeInstrumentBrowser(false);
            } else {
                for (const auto& hit : context.instrumentBrowserHitTargets) {
                    if (!hit.first.contains(mx, my)) {
                        continue;
                    }
                    context.instrumentBrowserSelected = hit.second;
                    context.closeInstrumentBrowser(true);
                    break;
                }
            }
        } else if (button == Button4 || button == Button5) {
            if (context.instrumentBrowserListRect.contains(mx, my)) {
                const int delta = button == Button4 ? -1 : 1;
                context.instrumentBrowserScroll = std::max(0, context.instrumentBrowserScroll + delta);
            }
        }
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }

    if (context.audioTuningDialogActive) {
        if (button == Button1) {
            bool handled = false;
            for (const AudioTuningDialogHit& hit : context.audioTuningDialogHits) {
                if (!hit.rect.contains(mx, my)) {
                    continue;
                }
                if (hit.role == "close") {
                    context.audioTuningDialogActive = false;
                } else if (hit.role == "mode") {
                    context.setAudioPerformanceMode(hit.mode, context.playbackSampleRate);
                } else if (hit.role == "delta") {
                    context.adjustAudioCustomLevel(hit.delta, context.playbackSampleRate);
                } else if (hit.role == "reset") {
                    context.resetAudioCustomLevel(context.playbackSampleRate);
                }
                handled = true;
                break;
            }
            if (!handled && !context.audioTuningDialogRect.contains(mx, my)) {
                context.audioTuningDialogActive = false;
            }
        } else if (button == Button4 || button == Button5) {
            if (context.audioTuningDialogRect.contains(mx, my)) {
                int delta = button == Button4 ? -1 : 1;
                if ((stateMask & ShiftMask) != 0) {
                    delta *= 10;
                } else if ((stateMask & ControlMask) != 0) {
                    delta *= 50;
                }
                context.adjustAudioCustomLevel(delta, context.playbackSampleRate);
            }
        }
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }

    if (context.inlinePrompt.active && !(context.synthWindowVisible && context.isSynthInlinePromptKind(context.inlinePrompt.kind))) {
        const bool browserMode = context.inlinePromptUsesFileBrowser(context.inlinePrompt.kind);
        bool handled = false;
        if (button == Button1) {
            if (context.inlinePromptButtonsVisible && context.inlinePromptAcceptButton.contains(mx, my)) {
                context.executeInlinePrompt();
                handled = true;
            } else if (context.inlinePromptButtonsVisible && context.inlinePromptCancelButton.contains(mx, my)) {
                context.cancelInlinePrompt();
                handled = true;
            } else if (browserMode) {
                for (const FileBrowserHit& hit : context.fileBrowserHits) {
                    if (!hit.rect.contains(mx, my)) {
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
            if (context.fileBrowserListRect.contains(mx, my)) {
                const int delta = button == Button4 ? -1 : 1;
                context.fileBrowserScroll = std::max(0, context.fileBrowserScroll + delta);
                handled = true;
            }
        }
        if (handled || browserMode || button != Button1) {
            result.consumed = true;
            result.needsRedraw = true;
            return result;
        }
    } else if (context.unsavedPrompt.active) {
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }

    return result;
}

} // namespace arachno
