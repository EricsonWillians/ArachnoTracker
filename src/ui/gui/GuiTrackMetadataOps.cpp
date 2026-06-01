#include "ui/gui/GuiTrackMetadataOps.h"

#include <algorithm>

namespace arachno {

void runTrackMetadataAction(const GuiTrackMetadataActionContext& context, const std::string& role, int track) {
    const AppSessionSnapshot snap = context.activeSnapshot();
    if (snap.editor.tracks.empty()) {
        return;
    }

    const int trackIndex = std::clamp(track, 0, static_cast<int>(snap.editor.tracks.size()) - 1);
    const TrackStripSummary& summary = snap.editor.tracks[static_cast<std::size_t>(trackIndex)];
    if (role == "rename") {
        context.beginInlinePrompt(
            InlinePromptKind::RenameTrack,
            "Rename track",
            "Track name",
            summary.name,
            trackIndex,
            -1);
        return;
    }

    AppActionRequest request;
    if (role == "vol_down" || role == "vol_up") {
        request.actionId = "editor.track.volume";
        const double delta = role == "vol_down" ? -0.05 : 0.05;
        const double value = std::clamp(summary.volume + delta, 0.0, 2.0);
        request.parameters = {
            {"track", std::to_string(trackIndex)},
            {"value", std::to_string(value)}};
    } else if (role == "pan_left" || role == "pan_right") {
        request.actionId = "editor.track.pan";
        const double delta = role == "pan_left" ? -0.05 : 0.05;
        const double value = std::clamp(summary.pan + delta, -1.0, 1.0);
        request.parameters = {
            {"track", std::to_string(trackIndex)},
            {"value", std::to_string(value)}};
    } else if (role == "mute") {
        request.actionId = "editor.track.mute";
        request.parameters = {
            {"track", std::to_string(trackIndex)},
            {"value", summary.muted ? "false" : "true"}};
    } else if (role == "solo") {
        request.actionId = "editor.track.solo";
        request.parameters = {
            {"track", std::to_string(trackIndex)},
            {"value", summary.solo ? "false" : "true"}};
    } else {
        return;
    }

    (void)context.runAction(request);
}

} // namespace arachno
