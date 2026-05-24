#include "Note.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <stdexcept>

namespace arachno {

double midiNoteToFrequency(int midiNote) {
    return 440.0 * std::pow(2.0, (static_cast<double>(midiNote) - 69.0) / 12.0);
}

std::string midiNoteName(int midiNote) {
    static constexpr std::array<const char*, 12> names = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    const int octave = (midiNote / 12) - 1;
    const int note = ((midiNote % 12) + 12) % 12;
    return std::string(names[static_cast<std::size_t>(note)]) + std::to_string(octave);
}

int noteNameToMidi(const std::string& noteName) {
    if (noteName.size() < 2) {
        throw std::invalid_argument("note name is too short");
    }

    std::string normalized;
    normalized.reserve(noteName.size());
    for (char ch : noteName) {
        normalized.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
    }

    const char root = normalized[0];
    int semitone = 0;
    switch (root) {
        case 'C': semitone = 0; break;
        case 'D': semitone = 2; break;
        case 'E': semitone = 4; break;
        case 'F': semitone = 5; break;
        case 'G': semitone = 7; break;
        case 'A': semitone = 9; break;
        case 'B': semitone = 11; break;
        default: throw std::invalid_argument("unknown note root: " + noteName);
    }

    std::size_t octaveOffset = 1;
    if (normalized.size() > 1 && (normalized[1] == '#' || normalized[1] == 'B')) {
        semitone += normalized[1] == '#' ? 1 : -1;
        octaveOffset = 2;
    }

    const int octave = std::stoi(normalized.substr(octaveOffset));
    return (octave + 1) * 12 + semitone;
}

double Note::frequency() const {
    return midiNoteToFrequency(midi);
}

std::string Note::name() const {
    return midiNoteName(midi);
}

} // namespace arachno
