#pragma once

#include <vector>
#include "Pattern.h"

class Tracker {
public:
    void addPattern(Pattern* pattern) { patterns.push_back(pattern); }
    const std::vector<Pattern*>& getPatterns() const { return patterns; }

private:
    std::vector<Pattern*> patterns;
};
