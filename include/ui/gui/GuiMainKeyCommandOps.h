#pragma once

#include <functional>
#include <string>

#include <X11/Xlib.h>

#include "ui/gui/GuiAudioRuntime.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiMainKeyCommandResult {
    bool consumed = false;
    bool needsRedraw = false;
};

struct GuiMainKeyCommandContext {
    KeySym key = NoSymbol;
    bool ctrlDown = false;
    bool shiftDown = false;
    bool altDown = false;

    bool& audioTuningDialogActive;
    bool& midiImportSplitByTrack;
    bool synthWindowVisible = false;
    double targetSongLengthMinutes = 0.0;
    GuiThemeMode& themeMode;
    int playbackSampleRate = 48000;
    bool transportPlaying = false;

    std::function<bool(KeySym)> keyMatches;

    std::function<void(AudioPerformanceMode, int)> setAudioPerformanceMode;
    std::function<AudioPerformanceMode()> currentPerformanceMode;
    std::function<void(const std::string&)> runActionById;
    std::function<void(const std::string&)> runSyncById;
    std::function<void()> runEvents;
    std::function<void()> openInstrumentBrowser;
    std::function<void(int)> cycleInstrumentBy;
    std::function<void(const std::string&)> runFileButtonAction;
    std::function<void()> beginSaveAsPrompt;
    std::function<void()> beginTemporalPastePrompt;
    std::function<void()> beginPatternCreatePrompt;
    std::function<void()> beginPatternClonePrompt;
    std::function<void()> deleteActivePattern;
    std::function<void()> beginSongLengthPrompt;
    std::function<void(bool)> setSynthWindowVisible;
    std::function<void(int)> cyclePatternBy;
    std::function<void(int)> cycleOrderBy;
    std::function<void()> insertOrderAtSelection;
    std::function<void()> appendOrderFromActivePattern;
    std::function<void()> removeSelectedOrder;
    std::function<void(double)> buildSongToTargetSeconds;
    std::function<void(double)> trimSongToTargetSeconds;
    std::function<void(double)> velocityNudge;
    std::function<void(int)> transposeSelection;
    std::function<void()> repeatSelectionOnce;
};

GuiMainKeyCommandResult handleMainKeyCommands(const GuiMainKeyCommandContext& context);

} // namespace arachno
