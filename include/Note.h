#pragma once

#include <string>

namespace arachno {

struct Note {
    int midi = 60;
    float velocity = 1.0f;

    Note() = default;
    Note(int midiNote, float noteVelocity) : midi(midiNote), velocity(noteVelocity) {}

    double frequency() const;
    std::string name() const;
};

double midiNoteToFrequency(int midiNote);
std::string midiNoteName(int midiNote);
int noteNameToMidi(const std::string& noteName);

} // namespace arachno
