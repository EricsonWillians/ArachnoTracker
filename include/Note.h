// Note.h
#ifndef NOTE_H
#define NOTE_H

#include "Effect.h"
#include <vector>
#include <cstdint>

struct Note {
    int16_t pitch;                // MIDI note number or custom pitch value (e.g., 60 for C4)
    uint8_t instrument;           // Instrument ID (0 if no instrument)
    uint8_t volume;               // Volume (0-64, or other range)
    std::vector<Effect> effects;  // List of effects applied to this note

    Note() : pitch(-1), instrument(0), volume(64) {}
};

#endif // NOTE_H
