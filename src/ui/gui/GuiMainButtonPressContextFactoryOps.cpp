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
    return makeMainSidebarClickContextFromState(
        GuiMainSidebarContextFactoryInput {
            pointerInSidebar,
            mx,
            my,
            input.octaveHitTargets,
            input.pianoKeyHits,
            input.trackMetadataHits,
            input.songLengthHits,
            input.midiImportSettingHits,
            input.instrumentControlHits,
            input.instrumentHitTargets,
            input.patternRowsMinus,
            input.patternRowsPlus,
            input.patternRowsValue,
            input.stepAdvanceButton,
            input.followPlaybackButton,
            input.draggingPatternRows,
            input.patternResizeAnchorY,
            input.patternResizeStartRows,
            input.activePatternRows,
            input.stepAdvance,
            input.followPlayback,
            input.paintNoteMidi,
            input.armedOctave,
            input.armedInstrument,
            input.targetSongLengthMinutes,
            input.midiImportRowsPerBeat,
            input.midiImportPatternRows,
            input.midiImportSplitByTrack,
            input.setArmedOctave,
            input.applyArmedOctaveToSelection,
            input.activeSnapshot,
            input.paintNoteAt,
            input.ensurePatternRowsForRow,
            [&](const std::string& actionId) {
                AppActionRequest request;
                request.actionId = actionId;
                input.runActionRequest(request);
            },
            input.refreshSnapshot,
            input.runTrackMetadataAction,
            input.resizePatternRows,
            input.beginInlinePrompt,
            input.buildSongToTargetSeconds,
            input.trimSongToTargetSeconds,
            input.runFileButtonAction,
            input.selectInstrument,
            input.auditionArmedInstrument,
            input.openInstrumentBrowser});
}

} // namespace arachno
