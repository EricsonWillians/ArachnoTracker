#include "ui/gui/GuiMainPrimaryContextFactoryOps.h"

#include <algorithm>

namespace arachno {

GuiMainPrimaryClickContext makeMainPrimaryClickContextFromState(const GuiMainPrimaryContextFactoryInput& input) {
    const auto state = input;
    return GuiMainPrimaryClickContext {
        input.mx,
        input.my,
        input.playbackSampleRate,
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
        [state](const std::string& actionId) {
            AppActionRequest request;
            request.actionId = actionId;
            state.runActionRequest(request);
        },
        [state](const std::string& actionId) { state.runFileButtonAction(actionId); },
        [state](GuiThemeMode mode) { state.setThemeMode(mode); },
        [state](AudioPerformanceMode mode, int sampleRate) { state.setAudioPerformanceMode(mode, sampleRate); },
        [state](int direction) {
            const AppSessionSnapshot snap = state.activeSnapshot();
            const int totalTrackCols = std::max(1, snap.editor.activeGrid.trackCount);
            const int maxTrackStart = std::max(0, totalTrackCols - std::max(1, state.layout.trackCols));
            state.gridTrackStart = std::clamp(state.gridTrackStart + direction, 0, maxTrackStart);
        },
        [state]() { state.beginPatternCreatePrompt(); },
        [state]() { state.beginPatternClonePrompt(); },
        [state]() { state.deleteActivePattern(); },
        [state]() { state.insertOrderAtSelection(); },
        [state]() { state.appendOrderFromActivePattern(); },
        [state]() { state.removeSelectedOrder(); },
        [state](int direction) {
            const AppSessionSnapshot snap = state.activeSnapshot();
            const int count = static_cast<int>(snap.editor.patterns.size());
            if (count <= 0) {
                return;
            }
            const int current = std::clamp(snap.editor.status.activePattern, 0, count - 1);
            const int next = direction < 0 ? (current + count - 1) % count : (current + 1) % count;
            (void)state.selectPatternIndex(next, true);
        },
        [state](int direction) {
            const AppSessionSnapshot snap = state.activeSnapshot();
            const int count = static_cast<int>(snap.editor.order.size());
            if (count <= 0) {
                return;
            }
            const int current = std::clamp(state.selectedOrderIndex, 0, count - 1);
            const int next = direction < 0 ? (current + count - 1) % count : (current + 1) % count;
            (void)state.selectOrderIndex(next, true);
        },
        [state](int index, bool valid) { (void)state.selectOrderIndex(index, valid); },
        [state](const TrackHeaderHit& hit, int clickX, int clickY) {
            const AppSessionSnapshot snap = state.activeSnapshot();
            const TrackStripSummary* summary = hit.track < static_cast<int>(snap.editor.tracks.size())
                ? &snap.editor.tracks[static_cast<std::size_t>(hit.track)]
                : nullptr;
            if (hit.muteRect.contains(clickX, clickY) && summary != nullptr) {
                AppActionRequest mute;
                mute.actionId = "editor.track.mute";
                mute.parameters = {
                    {"track", std::to_string(hit.track)},
                    {"value", summary->muted ? "false" : "true"}};
                state.runActionRequest(mute);
                return true;
            }
            if (hit.soloRect.contains(clickX, clickY) && summary != nullptr) {
                AppActionRequest solo;
                solo.actionId = "editor.track.solo";
                solo.parameters = {
                    {"track", std::to_string(hit.track)},
                    {"value", summary->solo ? "false" : "true"}};
                state.runActionRequest(solo);
                return true;
            }
            const int cursorRow = snap.editor.status.cursorRow;
            state.moveCursor(cursorRow, hit.track);
            return true;
        }};
}

} // namespace arachno
