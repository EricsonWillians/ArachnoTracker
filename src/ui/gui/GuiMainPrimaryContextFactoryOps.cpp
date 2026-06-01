#include "ui/gui/GuiMainPrimaryContextFactoryOps.h"

#include <algorithm>

namespace arachno {

GuiMainPrimaryClickContext makeMainPrimaryClickContextFromState(const GuiMainPrimaryContextFactoryInput& input) {
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
        [&input](const std::string& actionId) {
            AppActionRequest request;
            request.actionId = actionId;
            input.runActionRequest(request);
        },
        [&input](const std::string& actionId) { input.runFileButtonAction(actionId); },
        [&input](GuiThemeMode mode) { input.setThemeMode(mode); },
        [&input](AudioPerformanceMode mode, int sampleRate) { input.setAudioPerformanceMode(mode, sampleRate); },
        [&input](int direction) {
            const AppSessionSnapshot snap = input.activeSnapshot();
            const int totalTrackCols = std::max(1, snap.editor.activeGrid.trackCount);
            const int maxTrackStart = std::max(0, totalTrackCols - std::max(1, input.layout.trackCols));
            input.gridTrackStart = std::clamp(input.gridTrackStart + direction, 0, maxTrackStart);
        },
        [&input]() { input.beginPatternCreatePrompt(); },
        [&input]() { input.beginPatternClonePrompt(); },
        [&input]() { input.deleteActivePattern(); },
        [&input]() { input.insertOrderAtSelection(); },
        [&input]() { input.appendOrderFromActivePattern(); },
        [&input]() { input.removeSelectedOrder(); },
        [&input](int direction) {
            const AppSessionSnapshot snap = input.activeSnapshot();
            const int count = static_cast<int>(snap.editor.patterns.size());
            if (count <= 0) {
                return;
            }
            const int current = std::clamp(snap.editor.status.activePattern, 0, count - 1);
            const int next = direction < 0 ? (current + count - 1) % count : (current + 1) % count;
            (void)input.selectPatternIndex(next, true);
        },
        [&input](int direction) {
            const AppSessionSnapshot snap = input.activeSnapshot();
            const int count = static_cast<int>(snap.editor.order.size());
            if (count <= 0) {
                return;
            }
            const int current = std::clamp(input.selectedOrderIndex, 0, count - 1);
            const int next = direction < 0 ? (current + count - 1) % count : (current + 1) % count;
            (void)input.selectOrderIndex(next, true);
        },
        [&input](int index, bool valid) { (void)input.selectOrderIndex(index, valid); },
        [&input](const TrackHeaderHit& hit, int clickX, int clickY) {
            const AppSessionSnapshot snap = input.activeSnapshot();
            const TrackStripSummary* summary = hit.track < static_cast<int>(snap.editor.tracks.size())
                ? &snap.editor.tracks[static_cast<std::size_t>(hit.track)]
                : nullptr;
            if (hit.muteRect.contains(clickX, clickY) && summary != nullptr) {
                AppActionRequest mute;
                mute.actionId = "editor.track.mute";
                mute.parameters = {
                    {"track", std::to_string(hit.track)},
                    {"value", summary->muted ? "false" : "true"}};
                input.runActionRequest(mute);
                return true;
            }
            if (hit.soloRect.contains(clickX, clickY) && summary != nullptr) {
                AppActionRequest solo;
                solo.actionId = "editor.track.solo";
                solo.parameters = {
                    {"track", std::to_string(hit.track)},
                    {"value", summary->solo ? "false" : "true"}};
                input.runActionRequest(solo);
                return true;
            }
            const int cursorRow = snap.editor.status.cursorRow;
            input.moveCursor(cursorRow, hit.track);
            return true;
        }};
}

} // namespace arachno
