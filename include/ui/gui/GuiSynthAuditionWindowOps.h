#pragma once

#include <functional>
#include <string>

#include "AppActions.h"
#include "ApplicationSession.h"
#include "Synthesizer.h"

namespace arachno {

struct GuiSynthAuditionWindowContext {
    ApplicationSession& session;
    int& armedInstrument;
    int& synthPreviewMidi;
    int& paintNoteMidi;
    float defaultVelocity = 0.8f;
    bool& synthWindowNeedsRedraw;
    AppActionResult& lastAction;

    std::function<void(int)> ensureSynthKeyboardShowsMidi;
    std::function<void(const AppActionResult&)> applyActionResultStatus;
};

void auditionArmedInstrumentNoteFromWindowState(
    const GuiSynthAuditionWindowContext& context,
    int midiNote,
    float velocity,
    double gateSeconds,
    const std::string& actionId);

void auditionArmedInstrumentFromWindowState(const GuiSynthAuditionWindowContext& context);

void auditionSynthPreviewMidiFromWindowState(const GuiSynthAuditionWindowContext& context, int midiNote);

void auditionSynthPreviewMidiVelocityFromWindowState(
    const GuiSynthAuditionWindowContext& context,
    int midiNote,
    float velocity);

void auditionSynthOscillatorPreviewFromWindowState(
    const GuiSynthAuditionWindowContext& context,
    const SynthPatch& sourcePatch,
    int oscillatorIndex,
    int midiNote);

} // namespace arachno
