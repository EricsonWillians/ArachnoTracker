#pragma once

#include <string>
#include <vector>
#include "Effect.h"

class Instrument {
public:
    Instrument(const std::string& name) : name(name) {}
    
    void addEffect(Effect* effect) { effects.push_back(effect); }
    const std::vector<Effect*>& getEffects() const { return effects; }

    std::string getName() const { return name; }

private:
    std::string name;
    std::vector<Effect*> effects;
};
