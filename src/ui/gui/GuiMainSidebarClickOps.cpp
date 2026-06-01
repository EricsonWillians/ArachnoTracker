#include "ui/gui/GuiMainSidebarClickOps.h"

namespace arachno {

GuiMainSidebarClickResult handleMainSidebarLeftClick(const GuiMainSidebarClickContext& context) {
    GuiMainSidebarClickResult result;
    if (!context.pointerInSidebar) {
        return result;
    }

    for (const auto& octaveHit : context.octaveHitTargets) {
        if (!octaveHit.first.contains(context.mx, context.my)) {
            continue;
        }
        context.applyOctaveHit(octaveHit.second);
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }

    for (const PianoKeyHit& hit : context.pianoKeyHits) {
        if (!hit.black || !hit.rect.contains(context.mx, context.my)) {
            continue;
        }
        context.paintNoteMidi = hit.midiNote;
        context.paintCursorMidiWithStepAdvance(context.paintNoteMidi);
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }

    for (const PianoKeyHit& hit : context.pianoKeyHits) {
        if (hit.black || !hit.rect.contains(context.mx, context.my)) {
            continue;
        }
        context.paintNoteMidi = hit.midiNote;
        context.paintCursorMidiWithStepAdvance(context.paintNoteMidi);
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }

    for (const TrackMetadataHit& hit : context.trackMetadataHits) {
        if (!hit.rect.contains(context.mx, context.my)) {
            continue;
        }
        context.runTrackMetadataAction(hit.role, hit.track);
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }

    if (context.patternRowsMinus.contains(context.mx, context.my)) {
        context.resizePatternRows(context.activePatternRows - 8);
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.patternRowsPlus.contains(context.mx, context.my)) {
        context.resizePatternRows(context.activePatternRows + 8);
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.patternRowsValue.contains(context.mx, context.my)) {
        context.draggingPatternRows = true;
        context.patternResizeAnchorY = context.my;
        context.patternResizeStartRows = context.activePatternRows;
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.stepAdvanceButton.contains(context.mx, context.my)) {
        context.stepAdvance = !context.stepAdvance;
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (context.followPlaybackButton.contains(context.mx, context.my)) {
        context.followPlayback = !context.followPlayback;
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }

    for (const SongLengthHit& hit : context.songLengthHits) {
        if (!hit.rect.contains(context.mx, context.my)) {
            continue;
        }
        context.handleSongLengthRole(hit.role);
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }

    for (const MidiImportSettingHit& hit : context.midiImportSettingHits) {
        if (!hit.rect.contains(context.mx, context.my)) {
            continue;
        }
        context.handleMidiImportRole(hit.role);
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }

    for (const auto& target : context.instrumentControlHits) {
        if (!target.first.contains(context.mx, context.my)) {
            continue;
        }
        context.handleInstrumentControlRole(target.second);
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }

    for (const auto& target : context.instrumentHitTargets) {
        if (!target.first.contains(context.mx, context.my)) {
            continue;
        }
        context.selectInstrument(target.second);
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }

    return result;
}

} // namespace arachno
