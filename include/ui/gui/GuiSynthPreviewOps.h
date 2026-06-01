#pragma once

#include <array>
#include <string>
#include <vector>

#include "AppActions.h"

namespace arachno {

int clampSynthKeyboardBaseOctave(int baseOctave, int visibleOctaves);
int synthKeyboardBaseForMidi(int currentBaseOctave, int visibleOctaves, int midiNote);

std::vector<int> collectSynthPreviewNotes(
    const std::array<bool, 256>& previewKeyHeld,
    const std::array<int, 256>& previewKeyMidi,
    bool pointerDown,
    int pointerMidi,
    const std::array<bool, 128>& midiPreviewHeld);

AppActionResult auditionArmedInstrumentNote(
    ApplicationSession& session,
    int& armedInstrument,
    int midiNote,
    float velocity,
    double gateSeconds,
    const std::string& actionId);

AppActionResult auditionCurrentSynthPatchNote(
    ApplicationSession& session,
    int armedInstrument,
    int midiNote,
    float velocity,
    double gateSeconds,
    const std::string& actionId);

double defaultSynthPreviewGateSeconds(
    const ApplicationSession& session,
    int armedInstrument);

AppActionResult auditionSynthOscillatorPreview(
    ApplicationSession& session,
    const SynthPatch& sourcePatch,
    int oscillatorIndex,
    int midiNote,
    float velocity);

} // namespace arachno
