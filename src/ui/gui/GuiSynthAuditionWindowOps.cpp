#include "ui/gui/GuiSynthAuditionWindowOps.h"

#include <algorithm>

#include "ui/gui/GuiSynthPreviewOps.h"

namespace arachno {

void auditionArmedInstrumentNoteFromWindowState(
    const GuiSynthAuditionWindowContext& context,
    int midiNote,
    float velocity,
    double gateSeconds,
    const std::string& actionId) {
    const AppActionResult result = arachno::auditionArmedInstrumentNote(
        context.session,
        context.armedInstrument,
        midiNote,
        velocity,
        gateSeconds,
        actionId);
    context.applyActionResultStatus(result);
}

void auditionArmedInstrumentFromWindowState(const GuiSynthAuditionWindowContext& context) {
    auditionArmedInstrumentNoteFromWindowState(
        context,
        std::clamp(context.paintNoteMidi, 0, 127),
        context.defaultVelocity,
        0.20,
        "instrument.audition");
}

void auditionSynthPreviewMidiFromWindowState(const GuiSynthAuditionWindowContext& context, int midiNote) {
    context.synthPreviewMidi = std::clamp(midiNote, 0, 127);
    context.paintNoteMidi = context.synthPreviewMidi;
    context.ensureSynthKeyboardShowsMidi(context.synthPreviewMidi);
    const AppActionResult result = arachno::auditionCurrentSynthPatchNote(
        context.session,
        context.armedInstrument,
        context.synthPreviewMidi,
        context.defaultVelocity,
        0.20,
        "instrument.audition.patch_preview",
        true);
    context.applyActionResultStatus(result);
    context.synthWindowNeedsRedraw = true;
}

void auditionSynthPreviewMidiVelocityFromWindowState(
    const GuiSynthAuditionWindowContext& context,
    int midiNote,
    float velocity) {
    context.synthPreviewMidi = std::clamp(midiNote, 0, 127);
    context.paintNoteMidi = context.synthPreviewMidi;
    context.ensureSynthKeyboardShowsMidi(context.synthPreviewMidi);
    const double gateSeconds = defaultSynthPreviewGateSeconds(context.session, context.armedInstrument);
    const AppActionResult result = arachno::auditionCurrentSynthPatchNote(
        context.session,
        context.armedInstrument,
        context.synthPreviewMidi,
        velocity,
        gateSeconds,
        "instrument.audition.midi",
        true);
    context.applyActionResultStatus(result);
    context.synthWindowNeedsRedraw = true;
}

void auditionSynthOscillatorPreviewFromWindowState(
    const GuiSynthAuditionWindowContext& context,
    const SynthPatch& sourcePatch,
    int oscillatorIndex,
    int midiNote) {
    const AppActionResult result = arachno::auditionSynthOscillatorPreview(
        context.session,
        sourcePatch,
        oscillatorIndex,
        midiNote,
        context.defaultVelocity);
    context.applyActionResultStatus(result);
}

} // namespace arachno
