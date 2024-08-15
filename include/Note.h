#pragma once

#include <string>

class Note {
public:
    Note(int pitch, float velocity) : pitch(pitch), velocity(velocity) {}

    int getPitch() const { return pitch; }
    float getVelocity() const { return velocity; }

private:
    int pitch;
    float velocity;
};
