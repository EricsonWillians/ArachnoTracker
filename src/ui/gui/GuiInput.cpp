#include "GuiInput.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#include <X11/keysym.h>

namespace arachno {

std::string trimCopy(const std::string& value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }
    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(start, end - start);
}

std::string lowerCopy(const std::string& value) {
    std::string out = value;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return out;
}

KeySym normalizeLetterKey(KeySym key) {
    if (key >= XK_A && key <= XK_Z) {
        return key + (XK_a - XK_A);
    }
    return key;
}

bool trackerKeyToMidi(KeySym key, int octave, int& midiNote) {
    const KeySym normalized = normalizeLetterKey(key);
    int semitone = -1;
    switch (normalized) {
        case XK_z: semitone = 0; break;
        case XK_s: semitone = 1; break;
        case XK_x: semitone = 2; break;
        case XK_d: semitone = 3; break;
        case XK_c: semitone = 4; break;
        case XK_v: semitone = 5; break;
        case XK_g: semitone = 6; break;
        case XK_b: semitone = 7; break;
        case XK_h: semitone = 8; break;
        case XK_n: semitone = 9; break;
        case XK_j: semitone = 10; break;
        case XK_m: semitone = 11; break;

        case XK_q: semitone = 12; break;
        case XK_2: semitone = 13; break;
        case XK_w: semitone = 14; break;
        case XK_3: semitone = 15; break;
        case XK_e: semitone = 16; break;
        case XK_r: semitone = 17; break;
        case XK_5: semitone = 18; break;
        case XK_t: semitone = 19; break;
        case XK_6: semitone = 20; break;
        case XK_y: semitone = 21; break;
        case XK_7: semitone = 22; break;
        case XK_u: semitone = 23; break;
        default:
            return false;
    }
    // Octave numbers follow the app-wide display convention (midiNoteName: 60 = C4),
    // so armed octave N maps its C to MIDI (N + 1) * 12.
    midiNote = ((octave + 1) * 12) + semitone;
    midiNote = std::clamp(midiNote, 0, 127);
    return true;
}

std::string velocityText(float velocity) {
    std::ostringstream out;
    out.setf(std::ios::fixed);
    out.precision(2);
    out << velocity;
    return out.str();
}

int digitKeyToInt(KeySym key) {
    if (key >= XK_0 && key <= XK_9) {
        return static_cast<int>(key - XK_0);
    }
    if (key >= XK_KP_0 && key <= XK_KP_9) {
        return static_cast<int>(key - XK_KP_0);
    }
    return -1;
}

} // namespace arachno
