#pragma once

#include <array>
#include <functional>
#include <vector>

#include <X11/Xlib.h>

#include "ui/gui/GuiMidiInput.h"

namespace arachno {

bool isX11AutoRepeatRelease(Display* display, const XEvent& event);

bool pollSynthMidiPreviewInput(
    GuiMidiInput& midiInput,
    std::array<bool, 128>& synthMidiPreviewHeld,
    bool synthWindowVisible,
    const std::function<void(int, float)>& auditionSynthPreviewMidiVelocity,
    const std::function<void(int)>& noteOffSynthPreviewMidi,
    bool& synthWindowNeedsRedraw);

} // namespace arachno
