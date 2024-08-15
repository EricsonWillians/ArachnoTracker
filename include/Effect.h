// Effect.h
#ifndef EFFECT_H
#define EFFECT_H

#include <cstdint>

struct Effect {
    uint8_t effectType;   // Effect ID (e.g., vibrato, pitch slide)
    uint8_t effectParam;  // Parameter for the effect

    Effect(uint8_t type, uint8_t param) : effectType(type), effectParam(param) {}
};

#endif // EFFECT_H
