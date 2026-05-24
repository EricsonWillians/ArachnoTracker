#include "MidiExporter.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace arachno {

namespace {
struct MidiEvent {
    int tick = 0;
    std::vector<std::uint8_t> bytes;
};

void writeU16BE(std::ostream& out, std::uint16_t value) {
    out.put(static_cast<char>((value >> 8) & 0xff));
    out.put(static_cast<char>(value & 0xff));
}

void writeU32BE(std::ostream& out, std::uint32_t value) {
    out.put(static_cast<char>((value >> 24) & 0xff));
    out.put(static_cast<char>((value >> 16) & 0xff));
    out.put(static_cast<char>((value >> 8) & 0xff));
    out.put(static_cast<char>(value & 0xff));
}

void appendVarLen(std::vector<std::uint8_t>& data, int value) {
    std::uint32_t buffer = static_cast<std::uint32_t>(std::max(0, value)) & 0x7f;
    while ((value >>= 7) > 0) {
        buffer <<= 8;
        buffer |= static_cast<std::uint32_t>((value & 0x7f) | 0x80);
    }

    while (true) {
        data.push_back(static_cast<std::uint8_t>(buffer & 0xff));
        if (buffer & 0x80) {
            buffer >>= 8;
        } else {
            break;
        }
    }
}

void appendMetaText(std::vector<std::uint8_t>& data, std::uint8_t type, const std::string& text) {
    data.push_back(0xff);
    data.push_back(type);
    appendVarLen(data, static_cast<int>(text.size()));
    data.insert(data.end(), text.begin(), text.end());
}

void writeTrack(std::ostream& out, const std::vector<MidiEvent>& events) {
    std::vector<MidiEvent> sorted = events;
    std::stable_sort(sorted.begin(), sorted.end(), [](const MidiEvent& lhs, const MidiEvent& rhs) {
        return lhs.tick < rhs.tick;
    });

    std::vector<std::uint8_t> trackData;
    int previousTick = 0;
    for (const MidiEvent& event : sorted) {
        appendVarLen(trackData, event.tick - previousTick);
        trackData.insert(trackData.end(), event.bytes.begin(), event.bytes.end());
        previousTick = event.tick;
    }

    appendVarLen(trackData, 0);
    trackData.push_back(0xff);
    trackData.push_back(0x2f);
    trackData.push_back(0x00);

    out.write("MTrk", 4);
    writeU32BE(out, static_cast<std::uint32_t>(trackData.size()));
    out.write(reinterpret_cast<const char*>(trackData.data()), static_cast<std::streamsize>(trackData.size()));
}

int midiChannelForTrack(int track) {
    const int channel = track % 15;
    return channel >= 9 ? channel + 1 : channel;
}

std::vector<MidiEvent> buildTempoTrack(const Song& song) {
    const int microsecondsPerQuarter = static_cast<int>(std::llround(60000000.0 / song.bpm));
    std::vector<std::uint8_t> tempo = {
        0xff,
        0x51,
        0x03,
        static_cast<std::uint8_t>((microsecondsPerQuarter >> 16) & 0xff),
        static_cast<std::uint8_t>((microsecondsPerQuarter >> 8) & 0xff),
        static_cast<std::uint8_t>(microsecondsPerQuarter & 0xff),
    };

    std::vector<MidiEvent> events;
    events.push_back({0, tempo});

    std::vector<std::uint8_t> name;
    appendMetaText(name, 0x03, song.title.empty() ? "ArachnoTracker" : song.title);
    events.push_back({0, name});
    return events;
}
} // namespace

void exportMidiFile(const Song& song, const std::string& path, int ticksPerQuarterNote) {
    if (ticksPerQuarterNote <= 0) {
        throw std::invalid_argument("ticks per quarter note must be positive");
    }
    if (song.bpm <= 0.0 || song.rowsPerBeat <= 0) {
        throw std::invalid_argument("song timing must be positive");
    }

    const int ticksPerRow = std::max(1, ticksPerQuarterNote / song.rowsPerBeat);
    std::vector<std::vector<MidiEvent>> tracks(song.tracks.size() + 1);
    tracks[0] = buildTempoTrack(song);

    for (std::size_t trackIndex = 0; trackIndex < song.tracks.size(); ++trackIndex) {
        std::vector<std::uint8_t> name;
        appendMetaText(name, 0x03, song.tracks[trackIndex].name);
        tracks[trackIndex + 1].push_back({0, name});
    }

    int globalRow = 0;
    for (int patternIndex : song.order) {
        if (patternIndex < 0 || patternIndex >= static_cast<int>(song.patterns.size())) {
            continue;
        }

        const Pattern& pattern = song.patterns[static_cast<std::size_t>(patternIndex)];
        for (int row = 0; row < pattern.rowCount(); ++row) {
            for (int track = 0; track < pattern.trackCount(); ++track) {
                if (track < 0 || track >= static_cast<int>(song.tracks.size())) {
                    continue;
                }

                const PatternStep& step = pattern.step(row, track);
                if (!step.note.has_value()) {
                    continue;
                }

                const int channel = midiChannelForTrack(track);
                const int note = std::clamp(step.note->midi, 0, 127);
                const int velocity = std::clamp(static_cast<int>(std::llround(step.note->velocity * 127.0f)), 1, 127);
                const int startTick = static_cast<int>(std::llround(
                    (static_cast<double>(globalRow + row) + step.microOffsetRows) * ticksPerRow));
                const int durationTicks = std::max(1, static_cast<int>(std::llround(step.gate * ticksPerRow)));
                const int endTick = std::max(startTick + 1, startTick + durationTicks);

                tracks[static_cast<std::size_t>(track + 1)].push_back({
                    std::max(0, startTick),
                    {
                        static_cast<std::uint8_t>(0x90 | channel),
                        static_cast<std::uint8_t>(note),
                        static_cast<std::uint8_t>(velocity),
                    }
                });
                tracks[static_cast<std::size_t>(track + 1)].push_back({
                    std::max(0, endTick),
                    {
                        static_cast<std::uint8_t>(0x80 | channel),
                        static_cast<std::uint8_t>(note),
                        0x00,
                    }
                });
            }
        }

        globalRow += pattern.rowCount();
    }

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("failed to open MIDI output: " + path);
    }

    out.write("MThd", 4);
    writeU32BE(out, 6);
    writeU16BE(out, 1);
    writeU16BE(out, static_cast<std::uint16_t>(tracks.size()));
    writeU16BE(out, static_cast<std::uint16_t>(ticksPerQuarterNote));

    for (const std::vector<MidiEvent>& track : tracks) {
        writeTrack(out, track);
    }
}

} // namespace arachno
