#pragma once

#include <functional>
#include <string>

#include <X11/Xlib.h>

#include "AppActions.h"
#include "ui/gui/GuiMainKeyCommandOps.h"

namespace arachno {

struct GuiMainKeyCommandContextFactoryInput {
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
    int selectedOrderIndex = 0;

    std::function<bool(KeySym)> keyMatches;
    std::function<void(AudioPerformanceMode, int)> setAudioPerformanceMode;
    std::function<AudioPerformanceMode()> currentPerformanceMode;
    std::function<void(const std::string&)> runActionById;
    std::function<void(const std::string&)> runSyncById;
    std::function<void()> runEvents;
    std::function<void()> openInstrumentBrowser;
    std::function<void(int)> cycleInstrumentBy;
    std::function<void(const std::string&)> runFileButtonAction;
    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<void(
        InlinePromptKind,
        const std::string&,
        const std::string&,
        const std::string&,
        int,
        int)> beginInlinePrompt;
    std::function<void()> beginTemporalPastePrompt;
    std::function<void()> beginPatternCreatePrompt;
    std::function<void()> beginPatternClonePrompt;
    std::function<void()> deleteActivePattern;
    std::function<void(bool)> setSynthWindowVisible;
    std::function<bool(int, bool)> selectPatternIndex;
    std::function<bool(int, bool)> selectOrderIndex;
    std::function<void()> insertOrderAtSelection;
    std::function<void()> appendOrderFromActivePattern;
    std::function<void()> removeSelectedOrder;
    std::function<void(double)> buildSongToTargetSeconds;
    std::function<void(double)> trimSongToTargetSeconds;
    std::function<AppActionResult(const AppActionRequest&)> runAction;
};

GuiMainKeyCommandContext makeMainKeyCommandContextFromState(const GuiMainKeyCommandContextFactoryInput& input);

} // namespace arachno
