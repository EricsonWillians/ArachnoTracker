#include "ui/gui/GuiMainKeyModalOps.h"

#include <algorithm>
#include <array>

#include <X11/keysym.h>

#include "GuiInput.h"

namespace arachno {

GuiMainKeyModalResult handleMainKeyModal(const GuiMainKeyModalContext& context) {
    GuiMainKeyModalResult result;

    if (context.unsavedPromptActive) {
        const KeySym normalized = normalizeLetterKey(context.key);
        if (normalized == XK_s) {
            context.resolveUnsavedPrompt(UnsavedChangesChoice::Save);
        } else if (normalized == XK_d) {
            context.resolveUnsavedPrompt(UnsavedChangesChoice::Discard);
        } else {
            context.resolveUnsavedPrompt(UnsavedChangesChoice::Cancel);
        }
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }

    if (context.audioTuningDialogActive) {
        bool consumed = false;
        if (context.key == XK_Escape || context.keyMatches(XK_Return) || context.keyMatches(XK_KP_Enter)) {
            context.audioTuningDialogActive = false;
            consumed = true;
        } else if (context.keyMatches(XK_Left) || context.keyMatches(XK_KP_Left)
            || context.key == XK_minus || context.key == XK_KP_Subtract) {
            context.adjustAudioCustomLevel(context.shiftDown ? -10 : -1, context.playbackSampleRate);
            consumed = true;
        } else if (context.keyMatches(XK_Right) || context.keyMatches(XK_KP_Right)
            || context.key == XK_equal || context.key == XK_plus || context.key == XK_KP_Add) {
            context.adjustAudioCustomLevel(context.shiftDown ? 10 : 1, context.playbackSampleRate);
            consumed = true;
        } else if (context.keyMatches(XK_Page_Up)) {
            context.adjustAudioCustomLevel(-50, context.playbackSampleRate);
            consumed = true;
        } else if (context.keyMatches(XK_Page_Down)) {
            context.adjustAudioCustomLevel(50, context.playbackSampleRate);
            consumed = true;
        } else if (context.ctrlDown && context.key == XK_F7) {
            context.setAudioPerformanceMode(AudioPerformanceMode::Custom, context.playbackSampleRate);
            consumed = true;
        } else {
            const int digit = context.resolvedDigit();
            if (digit >= 1 && digit <= 5) {
                const std::array<AudioPerformanceMode, 5> modes {
                    AudioPerformanceMode::Auto,
                    AudioPerformanceMode::Live,
                    AudioPerformanceMode::Balanced,
                    AudioPerformanceMode::Heavy,
                    AudioPerformanceMode::Custom};
                context.setAudioPerformanceMode(modes[static_cast<std::size_t>(digit - 1)], context.playbackSampleRate);
                consumed = true;
            }
        }
        if (consumed) {
            result.consumed = true;
            result.needsRedraw = true;
            return result;
        }
    }

    if (context.inlinePrompt.active && !(context.synthWindowVisible && context.isSynthInlinePromptKind(context.inlinePrompt.kind))) {
        const bool browserMode = context.inlinePromptUsesFileBrowser(context.inlinePrompt.kind);
        bool consumed = false;
        if (context.key == XK_Escape) {
            context.cancelInlinePrompt();
            consumed = true;
        } else if (context.keyMatches(XK_Return) || context.keyMatches(XK_KP_Enter)) {
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
            consumed = true;
        } else if (browserMode && (context.keyMatches(XK_Up) || context.keyMatches(XK_KP_Up))) {
            if (context.fileBrowserEntries.empty()) {
                context.refreshFileBrowserEntries();
            }
            if (!context.fileBrowserEntries.empty()) {
                const int previous = context.fileBrowserSelected < 0 ? 0 : context.fileBrowserSelected;
                context.fileBrowserSelected = std::max(0, previous - 1);
                if (context.fileBrowserSelected < context.fileBrowserScroll) {
                    context.fileBrowserScroll = context.fileBrowserSelected;
                }
                const FileBrowserEntry& entry = context.fileBrowserEntries[static_cast<std::size_t>(context.fileBrowserSelected)];
                context.inlinePrompt.value = entry.path.string();
            }
            consumed = true;
        } else if (browserMode && (context.keyMatches(XK_Down) || context.keyMatches(XK_KP_Down))) {
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
                const FileBrowserEntry& entry = context.fileBrowserEntries[static_cast<std::size_t>(context.fileBrowserSelected)];
                context.inlinePrompt.value = entry.path.string();
            }
            consumed = true;
        } else if (context.keyMatches(XK_BackSpace)) {
            if (!context.inlinePrompt.value.empty()) {
                context.inlinePrompt.value.pop_back();
            }
            context.fileBrowserSelected = -1;
            consumed = true;
        } else if (context.key == XK_Delete) {
            context.inlinePrompt.value.clear();
            context.fileBrowserSelected = -1;
            consumed = true;
        } else if (!context.ctrlDown && !context.altDown && context.lookupCount > 0 && context.lookupBuffer != nullptr) {
            for (int index = 0; index < context.lookupCount; ++index) {
                const unsigned char ch = static_cast<unsigned char>(context.lookupBuffer[index]);
                if (ch >= 32 && ch <= 126) {
                    context.inlinePrompt.value.push_back(static_cast<char>(ch));
                }
            }
            context.fileBrowserSelected = -1;
            consumed = true;
        }
        if (consumed) {
            result.consumed = true;
            result.needsRedraw = true;
            return result;
        }
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }

    if (context.instrumentBrowserActive) {
        bool consumed = false;
        if (context.key == XK_Escape) {
            context.closeInstrumentBrowser(false);
            consumed = true;
        } else if (context.keyMatches(XK_Return) || context.keyMatches(XK_KP_Enter)) {
            context.closeInstrumentBrowser(true);
            consumed = true;
        } else if (context.ctrlDown && (context.keyMatches(XK_Up) || context.keyMatches(XK_KP_Up))) {
            context.cycleInstrumentBy(-1);
            context.syncInstrumentBrowserSelectionFromArmed();
            consumed = true;
        } else if (context.ctrlDown && (context.keyMatches(XK_Down) || context.keyMatches(XK_KP_Down))) {
            context.cycleInstrumentBy(1);
            context.syncInstrumentBrowserSelectionFromArmed();
            consumed = true;
        } else if (context.keyMatches(XK_Up) || context.keyMatches(XK_KP_Up)) {
            context.instrumentBrowserSelected = std::max(0, context.instrumentBrowserSelected - 1);
            consumed = true;
        } else if (context.keyMatches(XK_Down) || context.keyMatches(XK_KP_Down)) {
            const int count = context.filteredInstrumentCount();
            context.instrumentBrowserSelected = std::min(std::max(0, count - 1), context.instrumentBrowserSelected + 1);
            consumed = true;
        } else if (context.keyMatches(XK_Page_Up) || context.keyMatches(XK_KP_Page_Up)) {
            context.instrumentBrowserSelected = std::max(0, context.instrumentBrowserSelected - 8);
            consumed = true;
        } else if (context.keyMatches(XK_Page_Down) || context.keyMatches(XK_KP_Page_Down)) {
            const int count = context.filteredInstrumentCount();
            context.instrumentBrowserSelected = std::min(std::max(0, count - 1), context.instrumentBrowserSelected + 8);
            consumed = true;
        } else if (context.keyMatches(XK_BackSpace)) {
            if (!context.instrumentBrowserQuery.empty()) {
                context.instrumentBrowserQuery.pop_back();
            }
            context.instrumentBrowserSelected = 0;
            context.instrumentBrowserScroll = 0;
            consumed = true;
        } else if (context.key == XK_Delete) {
            context.instrumentBrowserQuery.clear();
            context.instrumentBrowserSelected = 0;
            context.instrumentBrowserScroll = 0;
            consumed = true;
        } else if (!context.ctrlDown && !context.altDown && context.lookupCount > 0 && context.lookupBuffer != nullptr) {
            for (int index = 0; index < context.lookupCount; ++index) {
                const unsigned char ch = static_cast<unsigned char>(context.lookupBuffer[index]);
                if (ch >= 32 && ch <= 126) {
                    context.instrumentBrowserQuery.push_back(static_cast<char>(ch));
                }
            }
            context.instrumentBrowserSelected = 0;
            context.instrumentBrowserScroll = 0;
            consumed = true;
        }
        if (consumed) {
            result.consumed = true;
            result.needsRedraw = true;
            return result;
        }
    }

    return result;
}

} // namespace arachno
