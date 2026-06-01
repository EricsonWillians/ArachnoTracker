#include "ui/gui/GuiSynthPreviewOps.h"

#include <algorithm>

namespace arachno {

int clampSynthKeyboardBaseOctave(int baseOctave, int visibleOctaves) {
    const int maxStart = std::max(0, 10 - std::max(1, visibleOctaves));
    return std::clamp(baseOctave, 0, maxStart);
}

int synthKeyboardBaseForMidi(int currentBaseOctave, int visibleOctaves, int midiNote) {
    const int clampedNote = std::clamp(midiNote, 0, 127);
    const int noteOctave = clampedNote / 12;
    int base = clampSynthKeyboardBaseOctave(currentBaseOctave, visibleOctaves);
    if (noteOctave < base || noteOctave >= base + std::max(1, visibleOctaves)) {
        base = noteOctave - (std::max(1, visibleOctaves) / 2);
        base = clampSynthKeyboardBaseOctave(base, visibleOctaves);
    }
    return base;
}

std::vector<int> collectSynthPreviewNotes(
    const std::array<bool, 256>& previewKeyHeld,
    const std::array<int, 256>& previewKeyMidi,
    bool pointerDown,
    int pointerMidi,
    const std::array<bool, 128>& midiPreviewHeld) {
    std::vector<int> notes;
    notes.reserve(16);
    for (std::size_t keycode = 0; keycode < previewKeyHeld.size(); ++keycode) {
        if (!previewKeyHeld[keycode]) {
            continue;
        }
        const int midi = previewKeyMidi[keycode];
        if (midi >= 0 && midi <= 127) {
            notes.push_back(midi);
        }
    }
    if (pointerDown && pointerMidi >= 0 && pointerMidi <= 127) {
        notes.push_back(pointerMidi);
    }
    for (int midi = 0; midi < static_cast<int>(midiPreviewHeld.size()); ++midi) {
        if (midiPreviewHeld[static_cast<std::size_t>(midi)]) {
            notes.push_back(midi);
        }
    }
    std::sort(notes.begin(), notes.end());
    notes.erase(std::unique(notes.begin(), notes.end()), notes.end());
    return notes;
}

AppActionResult auditionArmedInstrumentNote(
    ApplicationSession& session,
    int& armedInstrument,
    int midiNote,
    float velocity,
    double gateSeconds,
    const std::string& actionId) {
    AppActionResult out;
    const AppSessionSnapshot snap = session.snapshot(0, 1);
    const int count = static_cast<int>(snap.editor.instruments.size());
    if (count <= 0) {
        out.ok = false;
        out.actionId = actionId;
        out.error = "no instruments available";
        return out;
    }
    armedInstrument = std::clamp(armedInstrument, 0, count - 1);
    const AuditionResult audition = session.playback().auditionInstrument(
        armedInstrument,
        std::clamp(midiNote, 0, 127),
        std::clamp(velocity, 0.02f, 1.0f),
        std::max(0.03, gateSeconds));
    out.ok = audition.ok;
    out.actionId = actionId;
    out.message = audition.message;
    out.error = audition.error;
    return out;
}

AppActionResult auditionCurrentSynthPatchNote(
    ApplicationSession& session,
    int armedInstrument,
    int midiNote,
    float velocity,
    double gateSeconds,
    const std::string& actionId) {
    AppActionResult out;
    const int instrumentCount = static_cast<int>(session.song().instruments.size());
    const int instrument = instrumentCount <= 0 ? -1 : std::clamp(armedInstrument, 0, instrumentCount - 1);
    if (instrument < 0 || instrument >= instrumentCount) {
        out.ok = false;
        out.actionId = actionId;
        out.error = "no instruments available";
        return out;
    }
    const SynthPatch& patch = session.song().instruments[static_cast<std::size_t>(instrument)].patch;
    const AuditionResult audition = session.playback().auditionPatch(
        patch,
        std::clamp(midiNote, 0, 127),
        std::clamp(velocity, 0.02f, 1.0f),
        std::max(0.03, gateSeconds),
        0.0);
    out.ok = audition.ok;
    out.actionId = actionId;
    out.message = audition.message;
    out.error = audition.error;
    return out;
}

double defaultSynthPreviewGateSeconds(
    const ApplicationSession& session,
    int armedInstrument) {
    double gateSeconds = 0.24;
    const int instrumentCount = static_cast<int>(session.song().instruments.size());
    const int instrument = instrumentCount <= 0 ? -1 : std::clamp(armedInstrument, 0, instrumentCount - 1);
    if (instrument >= 0) {
        const SynthPatch& patch = session.song().instruments[static_cast<std::size_t>(instrument)].patch;
        gateSeconds = std::clamp(
            0.08 + patch.ampEnvelope.release * 0.75 + patch.ampEnvelope.decay * 0.20,
            0.08,
            0.55);
    }
    return gateSeconds;
}

AppActionResult auditionSynthOscillatorPreview(
    ApplicationSession& session,
    const SynthPatch& sourcePatch,
    int oscillatorIndex,
    int midiNote,
    float velocity) {
    SynthPatch previewPatch = sourcePatch;
    previewPatch.name = sourcePatch.name + " OSC " + std::string(1, static_cast<char>('A' + std::clamp(oscillatorIndex, 0, 3)));
    previewPatch.oscillatorAEnabled = oscillatorIndex == 0;
    previewPatch.oscillatorBEnabled = oscillatorIndex == 1;
    previewPatch.oscillatorCEnabled = oscillatorIndex == 2;
    previewPatch.oscillatorDEnabled = oscillatorIndex == 3;
    previewPatch.subEnabled = false;
    previewPatch.noiseEnabled = false;
    previewPatch.fmEnabled = false;
    previewPatch.ringEnabled = false;
    previewPatch.hardSyncEnabled = false;
    previewPatch.chorusEnabled = false;
    previewPatch.bitCrushEnabled = false;
    previewPatch.filterEnvelopeAmount = 0.0;
    previewPatch.filterDrive = 0.0;
    previewPatch.lfoFilterDepth = 0.0;
    previewPatch.lfoPanDepth = 0.0;
    previewPatch.vibratoCents = 0.0;
    previewPatch.tremoloDepth = 0.0;
    previewPatch.pitchEnvelopeSemitones = 0.0;
    previewPatch.wavefold = 0.0;
    previewPatch.drive = 0.0;
    previewPatch.highPass = 0.0;
    previewPatch.cutoff = 1.0;
    previewPatch.resonance = 0.0;
    previewPatch.unisonVoices = 1;
    previewPatch.unisonDetuneCents = 0.0;
    previewPatch.gain = 0.72;
    previewPatch.ampEnvelope.attack = 0.001;
    previewPatch.ampEnvelope.decay = 0.05;
    previewPatch.ampEnvelope.sustain = 0.92;
    previewPatch.ampEnvelope.release = 0.08;
    const AuditionResult audition = session.playback().auditionPatch(
        previewPatch,
        std::clamp(midiNote, 0, 127),
        std::clamp(velocity, 0.05f, 1.0f),
        0.22,
        0.0);
    AppActionResult out;
    out.ok = audition.ok;
    out.actionId = "instrument.wave.preview";
    out.message = audition.message;
    out.error = audition.error;
    return out;
}

} // namespace arachno
