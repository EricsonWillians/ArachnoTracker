#include "ui/gui/GuiMainButtonPressContextFactoryOps.h"

#include "ui/gui/GuiMainPrimaryContextFactoryOps.h"
#include "ui/gui/GuiMainSidebarContextFactoryOps.h"

namespace arachno {

GuiMainPrimaryClickContext makeMainPrimaryClickContextForButtonPress(
    const GuiMainButtonPressContextFactoryInput& input,
    int mx,
    int my,
    int playbackSampleRate) {
    return makeMainPrimaryClickContextFromState(
        GuiMainPrimaryContextFactoryInput {
            mx,
            my,
            playbackSampleRate,
            input.transportButtons,
            input.fileButtons,
            input.themeButtons,
            input.audioPerformanceButtons,
            input.orderSlotHits,
            input.trackHeaderHits,
            input.gridTrackPrevButton,
            input.gridTrackNextButton,
            input.patternPrevButton,
            input.patternNextButton,
            input.patternNewButton,
            input.patternCloneButton,
            input.patternDeleteButton,
            input.orderPrevButton,
            input.orderNextButton,
            input.orderInsertButton,
            input.orderAppendButton,
            input.orderDeleteButton,
            input.layout,
            input.gridTrackStart,
            input.selectedOrderIndex,
            input.activeSnapshot,
            input.runActionRequest,
            input.runFileButtonAction,
            input.setThemeMode,
            input.setAudioPerformanceMode,
            input.beginPatternCreatePrompt,
            input.beginPatternClonePrompt,
            input.deleteActivePattern,
            input.insertOrderAtSelection,
            input.appendOrderFromActivePattern,
            input.removeSelectedOrder,
            input.selectPatternIndex,
            input.selectOrderIndex,
            input.moveCursor});
}

GuiMainSidebarClickContext makeMainSidebarClickContextForButtonPress(
    const GuiMainButtonPressContextFactoryInput& input,
    bool pointerInSidebar,
    int mx,
    int my) {
    const auto state = input;
    return makeMainSidebarClickContextFromState(
        GuiMainSidebarContextFactoryInput {
            pointerInSidebar,
            mx,
            my,
            state.octaveHitTargets,
            state.pianoKeyHits,
            state.trackMetadataHits,
            state.songLengthHits,
            state.midiImportSettingHits,
            state.instrumentControlHits,
            state.instrumentHitTargets,
            state.patternRowsMinus,
            state.patternRowsPlus,
            state.patternRowsValue,
            state.stepAdvanceButton,
            state.legatoButton,
            state.followPlaybackButton,
            state.draggingPatternRows,
            state.patternResizeAnchorY,
            state.patternResizeStartRows,
            state.activePatternRows,
            state.stepAdvance,
            state.followPlayback,
            state.paintNoteMidi,
            state.armedOctave,
            state.armedInstrument,
            state.targetSongLengthMinutes,
            state.midiImportRowsPerBeat,
            state.midiImportPatternRows,
            state.midiImportSplitByTrack,
            state.setArmedOctave,
            state.applyArmedOctaveToSelection,
            state.activeSnapshot,
            state.paintNoteAt,
            state.ensurePatternRowsForRow,
            [state](const std::string& actionId) {
                AppActionRequest request;
                request.actionId = actionId;
                state.runActionRequest(request);
            },
            state.refreshSnapshot,
            state.runTrackMetadataAction,
            state.resizePatternRows,
            state.beginInlinePrompt,
            state.buildSongToTargetSeconds,
            state.trimSongToTargetSeconds,
            state.runFileButtonAction,
            state.selectInstrument,
            state.auditionArmedInstrument,
            state.openInstrumentBrowser});
}

} // namespace arachno
