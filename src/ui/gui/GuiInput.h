#pragma once

#include <string>

#include <X11/Xlib.h>

namespace arachno {

std::string trimCopy(const std::string& value);
std::string lowerCopy(const std::string& value);
KeySym normalizeLetterKey(KeySym key);
bool trackerKeyToMidi(KeySym key, int octave, int& midiNote);
std::string velocityText(float velocity);
int digitKeyToInt(KeySym key);

} // namespace arachno
