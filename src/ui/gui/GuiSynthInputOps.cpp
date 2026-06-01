#include "ui/gui/GuiSynthInputOps.h"

#include <algorithm>

namespace arachno {

bool isX11AutoRepeatRelease(Display* display, const XEvent& event) {
    if (event.type != KeyRelease) {
        return false;
    }
    if (XEventsQueued(display, QueuedAfterReading) <= 0) {
        return false;
    }
    XEvent nextEvent;
    XPeekEvent(display, &nextEvent);
    return nextEvent.type == KeyPress
        && nextEvent.xkey.window == event.xkey.window
        && nextEvent.xkey.keycode == event.xkey.keycode
        && nextEvent.xkey.time == event.xkey.time;
}

bool pollSynthMidiPreviewInput(
    GuiMidiInput& midiInput,
    std::array<bool, 128>& synthMidiPreviewHeld,
    bool synthWindowVisible,
    const std::function<void(int, float)>& auditionSynthPreviewMidiVelocity,
    bool& synthWindowNeedsRedraw) {
    std::vector<GuiMidiEvent> midiEvents;
    const bool changed = midiInput.poll(midiEvents);
    for (const GuiMidiEvent& midiEvent : midiEvents) {
        const int midiNote = std::clamp(midiEvent.midiNote, 0, 127);
        if (midiEvent.type == GuiMidiEvent::Type::NoteOn) {
            synthMidiPreviewHeld[static_cast<std::size_t>(midiNote)] = true;
            if (synthWindowVisible) {
                auditionSynthPreviewMidiVelocity(midiNote, midiEvent.velocity);
            }
        } else {
            synthMidiPreviewHeld[static_cast<std::size_t>(midiNote)] = false;
        }
    }
    if (changed && synthWindowVisible) {
        synthWindowNeedsRedraw = true;
    }
    return changed;
}

} // namespace arachno
