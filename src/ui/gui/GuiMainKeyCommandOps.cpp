#include "ui/gui/GuiMainKeyCommandOps.h"

#include <array>

#include <X11/keysym.h>

#include "GuiInput.h"

namespace arachno {

GuiMainKeyCommandResult handleMainKeyCommands(const GuiMainKeyCommandContext& context) {
    GuiMainKeyCommandResult result;

    if (context.key == XK_F5) {
        context.runActionById("playback.play_song");
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.key == XK_F6) {
        context.runActionById("playback.play_pattern");
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.key == XK_F7) {
        if (context.ctrlDown) {
            context.audioTuningDialogActive = !context.audioTuningDialogActive;
            if (context.audioTuningDialogActive) {
                context.setAudioPerformanceMode(AudioPerformanceMode::Custom, context.playbackSampleRate);
            }
            result.consumed = true;
            result.needsRedraw = true;
            return result;
        }
        const std::array<AudioPerformanceMode, 4> modes {
            AudioPerformanceMode::Auto,
            AudioPerformanceMode::Live,
            AudioPerformanceMode::Balanced,
            AudioPerformanceMode::Heavy};
        std::size_t modeIndex = 0;
        bool foundMode = false;
        const AudioPerformanceMode currentMode = context.currentPerformanceMode();
        for (std::size_t index = 0; index < modes.size(); ++index) {
            if (modes[index] == currentMode) {
                modeIndex = index;
                foundMode = true;
                break;
            }
        }
        if (!foundMode) {
            modeIndex = context.shiftDown ? modes.size() - 1 : 0;
        } else if (context.shiftDown) {
            modeIndex = (modeIndex + modes.size() - 1) % modes.size();
        } else {
            modeIndex = (modeIndex + 1) % modes.size();
        }
        context.setAudioPerformanceMode(modes[modeIndex], context.playbackSampleRate);
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.key == XK_space) {
        if (context.ctrlDown) {
            context.runActionById("preview.cursor");
        } else if (context.shiftDown) {
            context.runActionById("playback.pause");
        } else {
            context.runActionById(context.transportPlaying ? "playback.stop" : "playback.play_pattern");
        }
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && normalizeLetterKey(context.key) == XK_z) {
        context.runActionById(context.shiftDown ? "editor.history.redo" : "editor.history.undo");
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && normalizeLetterKey(context.key) == XK_y) {
        context.runActionById("editor.history.redo");
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && normalizeLetterKey(context.key) == XK_r) {
        context.runSyncById("delta_with_fallback");
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && normalizeLetterKey(context.key) == XK_f) {
        context.runSyncById("force_snapshot");
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && !context.shiftDown && normalizeLetterKey(context.key) == XK_e) {
        context.runEvents();
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && normalizeLetterKey(context.key) == XK_h) {
        context.themeMode = context.themeMode == GuiThemeMode::Dos
            ? GuiThemeMode::HighContrast
            : GuiThemeMode::Dos;
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && context.shiftDown && normalizeLetterKey(context.key) == XK_i) {
        context.openInstrumentBrowser();
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && (context.keyMatches(XK_Up) || context.keyMatches(XK_KP_Up))) {
        context.cycleInstrumentBy(-1);
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && (context.keyMatches(XK_Down) || context.keyMatches(XK_KP_Down))) {
        context.cycleInstrumentBy(1);
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && normalizeLetterKey(context.key) == XK_n && !context.shiftDown) {
        context.runFileButtonAction("project.new");
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && normalizeLetterKey(context.key) == XK_o) {
        context.runFileButtonAction("project.open");
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && normalizeLetterKey(context.key) == XK_s && !context.shiftDown) {
        context.runFileButtonAction("project.save");
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && context.shiftDown && normalizeLetterKey(context.key) == XK_s) {
        context.beginSaveAsPrompt();
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && context.shiftDown && normalizeLetterKey(context.key) == XK_e) {
        context.runFileButtonAction("export.mixdown");
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && context.shiftDown && normalizeLetterKey(context.key) == XK_m) {
        context.midiImportSplitByTrack = !context.midiImportSplitByTrack;
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && !context.shiftDown && normalizeLetterKey(context.key) == XK_m) {
        context.runFileButtonAction("import.midi");
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && context.shiftDown && normalizeLetterKey(context.key) == XK_v) {
        context.beginTemporalPastePrompt();
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && context.shiftDown && normalizeLetterKey(context.key) == XK_p) {
        context.beginPatternCreatePrompt();
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && context.altDown && normalizeLetterKey(context.key) == XK_p) {
        context.beginPatternClonePrompt();
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && !context.shiftDown && !context.altDown
        && (context.keyMatches(XK_Delete) || context.keyMatches(XK_BackSpace))) {
        context.deleteActivePattern();
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && normalizeLetterKey(context.key) == XK_l) {
        context.beginSongLengthPrompt();
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && normalizeLetterKey(context.key) == XK_i) {
        context.setSynthWindowVisible(!context.synthWindowVisible);
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && !context.altDown && (context.keyMatches(XK_Page_Up) || context.keyMatches(XK_KP_Page_Up))) {
        context.cyclePatternBy(-1);
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && !context.altDown && (context.keyMatches(XK_Page_Down) || context.keyMatches(XK_KP_Page_Down))) {
        context.cyclePatternBy(1);
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && context.altDown && (context.keyMatches(XK_Page_Up) || context.keyMatches(XK_KP_Page_Up))) {
        context.cycleOrderBy(-1);
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && context.altDown && (context.keyMatches(XK_Page_Down) || context.keyMatches(XK_KP_Page_Down))) {
        context.cycleOrderBy(1);
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && context.altDown && (context.keyMatches(XK_Insert) || context.keyMatches(XK_KP_Insert))) {
        context.insertOrderAtSelection();
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && context.altDown && normalizeLetterKey(context.key) == XK_a) {
        context.appendOrderFromActivePattern();
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && context.altDown
        && (context.keyMatches(XK_Delete) || context.keyMatches(XK_BackSpace) || context.keyMatches(XK_KP_Delete))) {
        context.removeSelectedOrder();
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.shiftDown && !context.ctrlDown && !context.altDown && normalizeLetterKey(context.key) == XK_b) {
        context.buildSongToTargetSeconds(context.targetSongLengthMinutes * 60.0);
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.shiftDown && !context.ctrlDown && !context.altDown && normalizeLetterKey(context.key) == XK_t) {
        context.trimSongToTargetSeconds(context.targetSongLengthMinutes * 60.0);
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && context.shiftDown && !context.altDown
        && (context.keyMatches(XK_Up) || context.keyMatches(XK_KP_Up) || context.keyMatches(XK_Down) || context.keyMatches(XK_KP_Down))) {
        context.velocityNudge((context.keyMatches(XK_Up) || context.keyMatches(XK_KP_Up)) ? 0.05 : -0.05);
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && context.altDown && !context.shiftDown
        && (context.keyMatches(XK_Up) || context.keyMatches(XK_KP_Up) || context.keyMatches(XK_Down) || context.keyMatches(XK_KP_Down))) {
        context.transposeSelection((context.keyMatches(XK_Up) || context.keyMatches(XK_KP_Up)) ? 1 : -1);
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.ctrlDown && context.shiftDown && !context.altDown && normalizeLetterKey(context.key) == XK_r) {
        context.repeatSelectionOnce();
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }

    return result;
}

} // namespace arachno
