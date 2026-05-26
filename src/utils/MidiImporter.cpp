#include "MidiImporter.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace arachno {

namespace {
struct MidiMessage {
    enum class Kind {
        NoteOn,
        NoteOff,
        ProgramChange,
        ControlChange,
        PitchBend,
        Tempo,
        TrackName
    };

    Kind kind = Kind::NoteOn;
    int tick = 0;
    int track = 0;
    int channel = 0;
    int data1 = 0;
    int data2 = 0;
    std::string text;
};

struct ChannelPerformanceState {
    int program = 0;
    int ccVolume = 127;
    int ccModWheel = 0;
    int ccPan = 64;
    int ccExpression = 127;
    int ccResonance = 64;
    int ccCutoff = 64;
    int pitchBend = 8192;
};

struct NoteStartState {
    int tick = 0;
    int track = 0;
    int channel = 0;
    int midi = 60;
    int velocity = 100;
    ChannelPerformanceState performance;
};

struct ImportedNote {
    int startTick = 0;
    int endTick = 1;
    int sourceTrack = 0;
    int channel = 0;
    int midi = 60;
    int velocity = 100;
    int program = 0;
    ChannelPerformanceState performance;
};

struct LaneKey {
    int sourceTrack = 0;
    int channel = 0;
    int program = 0;

    bool operator==(const LaneKey& other) const {
        return sourceTrack == other.sourceTrack
            && channel == other.channel
            && program == other.program;
    }
};

struct LaneKeyHash {
    std::size_t operator()(const LaneKey& key) const {
        return (static_cast<std::size_t>(key.sourceTrack) << 16)
            ^ (static_cast<std::size_t>(key.channel) << 8)
            ^ static_cast<std::size_t>(std::clamp(key.program, 0, 127));
    }
};

std::uint16_t readU16BE(std::istream& in) {
    const int b0 = in.get();
    const int b1 = in.get();
    if (b0 < 0 || b1 < 0) {
        throw std::runtime_error("unexpected end of MIDI file");
    }
    return static_cast<std::uint16_t>((b0 << 8) | b1);
}

std::uint32_t readU32BE(std::istream& in) {
    const int b0 = in.get();
    const int b1 = in.get();
    const int b2 = in.get();
    const int b3 = in.get();
    if (b0 < 0 || b1 < 0 || b2 < 0 || b3 < 0) {
        throw std::runtime_error("unexpected end of MIDI file");
    }
    return (static_cast<std::uint32_t>(b0) << 24)
        | (static_cast<std::uint32_t>(b1) << 16)
        | (static_cast<std::uint32_t>(b2) << 8)
        | static_cast<std::uint32_t>(b3);
}

std::uint32_t readVarLen(const std::vector<std::uint8_t>& data, std::size_t& cursor) {
    std::uint32_t value = 0;
    int count = 0;
    while (cursor < data.size()) {
        const std::uint8_t byte = data[cursor++];
        value = (value << 7) | static_cast<std::uint32_t>(byte & 0x7f);
        ++count;
        if ((byte & 0x80u) == 0) {
            return value;
        }
        if (count >= 4) {
            throw std::runtime_error("invalid MIDI variable-length integer");
        }
    }
    throw std::runtime_error("unexpected end of MIDI track");
}

std::string fallbackTrackName(int track, int channel) {
    return "MIDI T" + std::to_string(track + 1) + " Ch " + std::to_string(channel + 1);
}

std::string gmProgramName(int program) {
    static const std::array<const char*, 128> names = {
        "Acoustic Grand Piano", "Bright Acoustic Piano", "Electric Grand Piano", "Honky-tonk Piano",
        "Electric Piano 1", "Electric Piano 2", "Harpsichord", "Clavinet",
        "Celesta", "Glockenspiel", "Music Box", "Vibraphone",
        "Marimba", "Xylophone", "Tubular Bells", "Dulcimer",
        "Drawbar Organ", "Percussive Organ", "Rock Organ", "Church Organ",
        "Reed Organ", "Accordion", "Harmonica", "Tango Accordion",
        "Acoustic Guitar (nylon)", "Acoustic Guitar (steel)", "Electric Guitar (jazz)", "Electric Guitar (clean)",
        "Electric Guitar (muted)", "Overdriven Guitar", "Distortion Guitar", "Guitar Harmonics",
        "Acoustic Bass", "Electric Bass (finger)", "Electric Bass (pick)", "Fretless Bass",
        "Slap Bass 1", "Slap Bass 2", "Synth Bass 1", "Synth Bass 2",
        "Violin", "Viola", "Cello", "Contrabass",
        "Tremolo Strings", "Pizzicato Strings", "Orchestral Harp", "Timpani",
        "String Ensemble 1", "String Ensemble 2", "SynthStrings 1", "SynthStrings 2",
        "Choir Aahs", "Voice Oohs", "Synth Voice", "Orchestra Hit",
        "Trumpet", "Trombone", "Tuba", "Muted Trumpet",
        "French Horn", "Brass Section", "Synth Brass 1", "Synth Brass 2",
        "Soprano Sax", "Alto Sax", "Tenor Sax", "Baritone Sax",
        "Oboe", "English Horn", "Bassoon", "Clarinet",
        "Piccolo", "Flute", "Recorder", "Pan Flute",
        "Blown Bottle", "Shakuhachi", "Whistle", "Ocarina",
        "Lead 1 (square)", "Lead 2 (sawtooth)", "Lead 3 (calliope)", "Lead 4 (chiff)",
        "Lead 5 (charang)", "Lead 6 (voice)", "Lead 7 (fifths)", "Lead 8 (bass + lead)",
        "Pad 1 (new age)", "Pad 2 (warm)", "Pad 3 (polysynth)", "Pad 4 (choir)",
        "Pad 5 (bowed)", "Pad 6 (metallic)", "Pad 7 (halo)", "Pad 8 (sweep)",
        "FX 1 (rain)", "FX 2 (soundtrack)", "FX 3 (crystal)", "FX 4 (atmosphere)",
        "FX 5 (brightness)", "FX 6 (goblins)", "FX 7 (echoes)", "FX 8 (sci-fi)",
        "Sitar", "Banjo", "Shamisen", "Koto",
        "Kalimba", "Bagpipe", "Fiddle", "Shanai",
        "Tinkle Bell", "Agogo", "Steel Drums", "Woodblock",
        "Taiko Drum", "Melodic Tom", "Synth Drum", "Reverse Cymbal",
        "Guitar Fret Noise", "Breath Noise", "Seashore", "Bird Tweet",
        "Telephone Ring", "Helicopter", "Applause", "Gunshot"};
    return names[static_cast<std::size_t>(std::clamp(program, 0, 127))];
}

struct TempoPoint {
    int tick = 0;
    int microsecondsPerQuarter = 500000;
};

struct TempoMap {
    std::vector<TempoPoint> points;
    std::vector<double> prefixSeconds;
};

TempoMap buildTempoMap(std::vector<TempoPoint> points, int division) {
    if (points.empty()) {
        points.push_back({0, 500000});
    }
    std::stable_sort(points.begin(), points.end(), [](const TempoPoint& left, const TempoPoint& right) {
        return left.tick < right.tick;
    });
    std::vector<TempoPoint> merged;
    merged.reserve(points.size());
    for (const TempoPoint& point : points) {
        if (!merged.empty() && merged.back().tick == point.tick) {
            merged.back().microsecondsPerQuarter = point.microsecondsPerQuarter;
        } else {
            merged.push_back(point);
        }
    }
    if (merged.front().tick != 0) {
        merged.insert(merged.begin(), TempoPoint {0, merged.front().microsecondsPerQuarter});
    }

    TempoMap map;
    map.points = std::move(merged);
    map.prefixSeconds.assign(map.points.size(), 0.0);
    for (std::size_t index = 1; index < map.points.size(); ++index) {
        const TempoPoint& previous = map.points[index - 1];
        const TempoPoint& current = map.points[index];
        const int deltaTicks = std::max(0, current.tick - previous.tick);
        const double deltaSeconds = static_cast<double>(deltaTicks)
            * (static_cast<double>(previous.microsecondsPerQuarter) / 1000000.0)
            / static_cast<double>(division);
        map.prefixSeconds[index] = map.prefixSeconds[index - 1] + deltaSeconds;
    }
    return map;
}

double tickToSeconds(const TempoMap& map, int division, int tick) {
    if (map.points.empty()) {
        return 0.0;
    }
    const int clampedTick = std::max(0, tick);
    auto it = std::upper_bound(map.points.begin(), map.points.end(), clampedTick, [](int value, const TempoPoint& point) {
        return value < point.tick;
    });
    std::size_t index = 0;
    if (it == map.points.begin()) {
        index = 0;
    } else {
        index = static_cast<std::size_t>(std::distance(map.points.begin(), it) - 1);
    }
    const TempoPoint& point = map.points[index];
    const int deltaTicks = std::max(0, clampedTick - point.tick);
    const double deltaSeconds = static_cast<double>(deltaTicks)
        * (static_cast<double>(point.microsecondsPerQuarter) / 1000000.0)
        / static_cast<double>(division);
    return map.prefixSeconds[index] + deltaSeconds;
}

void parseTrackMessages(
    const std::vector<std::uint8_t>& trackData,
    int trackIndex,
    std::vector<MidiMessage>& messages,
    int& maxTick) {
    std::size_t cursor = 0;
    int tick = 0;
    std::uint8_t runningStatus = 0;

    while (cursor < trackData.size()) {
        tick += static_cast<int>(readVarLen(trackData, cursor));
        maxTick = std::max(maxTick, tick);

        if (cursor >= trackData.size()) {
            break;
        }
        std::uint8_t status = trackData[cursor++];
        std::uint8_t firstData = 0;
        bool reusedRunningStatus = false;
        if (status < 0x80u) {
            if (runningStatus == 0) {
                throw std::runtime_error("MIDI running status used before status byte");
            }
            firstData = status;
            status = runningStatus;
            reusedRunningStatus = true;
        } else if (status < 0xf0u) {
            runningStatus = status;
        }

        const std::uint8_t kind = static_cast<std::uint8_t>(status & 0xf0u);
        const int channel = static_cast<int>(status & 0x0fu);

        auto readDataByte = [&](std::uint8_t fallback, bool hasFallback) {
            if (hasFallback) {
                return fallback;
            }
            if (cursor >= trackData.size()) {
                throw std::runtime_error("unexpected end of MIDI channel event");
            }
            return trackData[cursor++];
        };

        if (kind == 0x80u || kind == 0x90u) {
            const std::uint8_t note = readDataByte(firstData, reusedRunningStatus);
            const std::uint8_t velocity = readDataByte(0, false);
            MidiMessage message;
            message.kind = (kind == 0x90u && velocity > 0) ? MidiMessage::Kind::NoteOn : MidiMessage::Kind::NoteOff;
            message.tick = tick;
            message.track = trackIndex;
            message.channel = channel;
            message.data1 = static_cast<int>(note);
            message.data2 = static_cast<int>(velocity);
            messages.push_back(message);
            continue;
        }
        if (kind == 0xa0u) {
            (void)readDataByte(firstData, reusedRunningStatus);
            (void)readDataByte(0, false);
            continue;
        }
        if (kind == 0xb0u) {
            const std::uint8_t controller = readDataByte(firstData, reusedRunningStatus);
            const std::uint8_t value = readDataByte(0, false);
            MidiMessage message;
            message.kind = MidiMessage::Kind::ControlChange;
            message.tick = tick;
            message.track = trackIndex;
            message.channel = channel;
            message.data1 = static_cast<int>(controller);
            message.data2 = static_cast<int>(value);
            messages.push_back(message);
            continue;
        }
        if (kind == 0xc0u) {
            const std::uint8_t program = readDataByte(firstData, reusedRunningStatus);
            MidiMessage message;
            message.kind = MidiMessage::Kind::ProgramChange;
            message.tick = tick;
            message.track = trackIndex;
            message.channel = channel;
            message.data1 = static_cast<int>(program);
            messages.push_back(message);
            continue;
        }
        if (kind == 0xd0u) {
            (void)readDataByte(firstData, reusedRunningStatus);
            continue;
        }
        if (kind == 0xe0u) {
            const std::uint8_t lsb = readDataByte(firstData, reusedRunningStatus);
            const std::uint8_t msb = readDataByte(0, false);
            MidiMessage message;
            message.kind = MidiMessage::Kind::PitchBend;
            message.tick = tick;
            message.track = trackIndex;
            message.channel = channel;
            message.data1 = static_cast<int>((static_cast<int>(msb) << 7) | static_cast<int>(lsb));
            messages.push_back(message);
            continue;
        }

        if (status == 0xffu) {
            if (cursor >= trackData.size()) {
                throw std::runtime_error("unexpected end of MIDI meta event");
            }
            const std::uint8_t metaType = trackData[cursor++];
            const std::uint32_t length = readVarLen(trackData, cursor);
            if (cursor + length > trackData.size()) {
                throw std::runtime_error("invalid MIDI meta event length");
            }

            if (metaType == 0x2fu) {
                break;
            }
            if (metaType == 0x03u) {
                MidiMessage message;
                message.kind = MidiMessage::Kind::TrackName;
                message.tick = tick;
                message.track = trackIndex;
                message.text.assign(
                    reinterpret_cast<const char*>(trackData.data() + cursor),
                    reinterpret_cast<const char*>(trackData.data() + cursor + length));
                messages.push_back(message);
            } else if (metaType == 0x51u && length == 3) {
                const int microsecondsPerQuarter = (static_cast<int>(trackData[cursor]) << 16)
                    | (static_cast<int>(trackData[cursor + 1]) << 8)
                    | static_cast<int>(trackData[cursor + 2]);
                MidiMessage message;
                message.kind = MidiMessage::Kind::Tempo;
                message.tick = tick;
                message.track = trackIndex;
                message.data1 = microsecondsPerQuarter;
                messages.push_back(message);
            }
            cursor += length;
            continue;
        }

        if (status == 0xf0u || status == 0xf7u) {
            const std::uint32_t length = readVarLen(trackData, cursor);
            if (cursor + length > trackData.size()) {
                throw std::runtime_error("invalid MIDI SysEx event length");
            }
            cursor += length;
            continue;
        }

        throw std::runtime_error("unsupported MIDI event status byte");
    }
}

double ccToUnit(int value) {
    return std::clamp(static_cast<double>(value) / 127.0, 0.0, 1.0);
}

double ccPanToSigned(int value) {
    return std::clamp((static_cast<double>(value) - 64.0) / 63.0, -1.0, 1.0);
}

int dominantProgramForLane(const std::vector<ImportedNote>& notes) {
    std::array<int, 128> counts {};
    for (const ImportedNote& note : notes) {
        const int clamped = std::clamp(note.program, 0, 127);
        ++counts[static_cast<std::size_t>(clamped)];
    }
    int bestProgram = 0;
    int bestCount = -1;
    for (int program = 0; program < 128; ++program) {
        if (counts[static_cast<std::size_t>(program)] > bestCount) {
            bestProgram = program;
            bestCount = counts[static_cast<std::size_t>(program)];
        }
    }
    return bestProgram;
}

double averageMidiForLane(const std::vector<ImportedNote>& notes) {
    if (notes.empty()) {
        return 60.0;
    }
    double sum = 0.0;
    for (const ImportedNote& note : notes) {
        sum += static_cast<double>(note.midi);
    }
    return sum / static_cast<double>(notes.size());
}

int distinctProgramCountForLane(const std::vector<ImportedNote>& notes) {
    std::array<bool, 128> seen {};
    int count = 0;
    for (const ImportedNote& note : notes) {
        const int clamped = std::clamp(note.program, 0, 127);
        if (!seen[static_cast<std::size_t>(clamped)]) {
            seen[static_cast<std::size_t>(clamped)] = true;
            ++count;
        }
    }
    return count;
}

double averagePerformanceVolumeScale(const std::vector<ImportedNote>& notes) {
    if (notes.empty()) {
        return 1.0;
    }
    double sum = 0.0;
    int count = 0;
    for (const ImportedNote& note : notes) {
        const double scale = ccToUnit(note.performance.ccVolume) * ccToUnit(note.performance.ccExpression);
        sum += std::clamp(scale, 0.0, 1.0);
        ++count;
    }
    if (count <= 0) {
        return 1.0;
    }
    return std::clamp(sum / static_cast<double>(count), 0.05, 1.0);
}

double averageVelocityForLane(const std::vector<ImportedNote>& notes) {
    if (notes.empty()) {
        return 0.8;
    }
    double sum = 0.0;
    int count = 0;
    for (const ImportedNote& note : notes) {
        sum += std::clamp(static_cast<double>(note.velocity) / 127.0, 0.0, 1.0);
        ++count;
    }
    if (count <= 0) {
        return 0.8;
    }
    return std::clamp(sum / static_cast<double>(count), 0.05, 1.0);
}

SynthPatch buildPatchForLane(
    int channel,
    int dominantProgram,
    double averageMidi,
    const std::string& laneName) {
    SynthPatch patch;
    const int program = std::clamp(dominantProgram, 0, 127);
    const int family = program / 8;
    const int variant = program % 8;
    const double t = static_cast<double>(variant) / 7.0;
    std::string laneLower = laneName;
    std::transform(laneLower.begin(), laneLower.end(), laneLower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    auto laneHas = [&](const std::string& token) {
        return laneLower.find(token) != std::string::npos;
    };
    patch.name = gmProgramName(program);

    if (channel == 9
        || family == 14) {
        patch.oscillatorA = Waveform::Noise;
        patch.oscillatorB = Waveform::Triangle;
        patch.oscillatorC = Waveform::Saw;
        patch.oscillatorBEnabled = true;
        patch.oscillatorCEnabled = true;
        patch.noiseEnabled = true;
        patch.noise = std::clamp(0.35 + t * 0.35, 0.0, 1.0);
        patch.subEnabled = false;
        patch.click = std::clamp(0.2 + t * 0.35, 0.0, 1.0);
        patch.transientNoise = std::clamp(0.45 + t * 0.4, 0.0, 1.0);
        patch.transientShape = std::clamp(0.2 + t * 0.6, 0.0, 1.0);
        patch.transientDecay = std::clamp(0.012 + t * 0.05, 0.001, 2.0);
        patch.transientPitchSemitones = -12.0 + (t * 24.0);
        patch.transientPitchDecay = 0.008 + (t * 0.02);
        patch.drive = std::clamp(0.2 + t * 0.3, 0.0, 2.0);
        patch.cutoff = std::clamp(0.58 + t * 0.25, 0.0, 1.0);
        patch.resonance = std::clamp(0.18 + t * 0.38, 0.0, 1.0);
        patch.filterEnvelopeAmount = std::clamp(0.18 + t * 0.28, 0.0, 1.0);
        patch.bitCrushEnabled = variant >= 4;
        patch.bitCrush = std::clamp((t - 0.45) * 0.6, 0.0, 1.0);
        patch.gain = std::clamp(0.7 + t * 0.2, 0.0, 1.2);
        if (averageMidi < 45.0) {
            patch.subEnabled = true;
            patch.subOscillator = std::max(patch.subOscillator, 0.36);
            patch.highPass = std::min(patch.highPass, 0.08);
            patch.transientPitchSemitones = std::min(patch.transientPitchSemitones, -9.0);
            patch.cutoff = std::min(patch.cutoff, 0.58);
        } else if (averageMidi > 74.0) {
            patch.noise = std::max(patch.noise, 0.58);
            patch.noiseTone = std::max(patch.noiseTone, 0.86);
            patch.highPass = std::max(patch.highPass, 0.56);
            patch.cutoff = std::max(patch.cutoff, 0.78);
            patch.transientShape = std::max(patch.transientShape, 0.72);
        } else {
            patch.noise = std::max(patch.noise, 0.42);
            patch.highPass = std::max(patch.highPass, 0.24);
            patch.transientBurstCount = std::max(patch.transientBurstCount, 2);
            patch.transientBurstSpacing = std::min(patch.transientBurstSpacing, 0.0032);
        }
        return patch;
    }

    patch.oscillatorA = Waveform::Saw;
    patch.oscillatorB = Waveform::Square;
    patch.oscillatorC = Waveform::Triangle;
    patch.oscillatorCEnabled = true;
    patch.oscillatorCMix = std::clamp(0.12 + t * 0.22, 0.0, 1.0);
    patch.oscillatorMix = std::clamp(0.28 + t * 0.14, 0.0, 1.0);
    patch.detuneCents = 2.0 + (t * 12.0);
    patch.detuneCCents = -patch.detuneCents;
    patch.drive = std::clamp(0.06 + t * 0.18, 0.0, 1.0);
    patch.cutoff = std::clamp(0.45 + t * 0.35, 0.0, 1.0);
    patch.resonance = std::clamp(0.08 + t * 0.22, 0.0, 1.0);
    patch.filterEnvelopeAmount = std::clamp(0.14 + t * 0.26, 0.0, 1.0);
    patch.ampEnvelope.attack = 0.004 + t * 0.02;
    patch.ampEnvelope.decay = 0.08 + t * 0.2;
    patch.ampEnvelope.sustain = std::clamp(0.35 + t * 0.55, 0.0, 1.0);
    patch.ampEnvelope.release = 0.08 + t * 0.45;
    patch.gain = std::clamp(0.48 + t * 0.16, 0.0, 1.2);

    switch (family) {
        case 0: // Pianos
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Triangle;
            patch.pulseWidth = 0.46 - t * 0.16;
            patch.transientShape = 0.32 + t * 0.2;
            patch.transientNoise = 0.08 + t * 0.16;
            patch.click = 0.16 + t * 0.22;
            patch.subEnabled = false;
            patch.chorusEnabled = variant >= 4;
            patch.chorusMix = 0.12 + t * 0.12;
            break;
        case 1: // Chromatic percussion
            patch.oscillatorA = Waveform::Triangle;
            patch.oscillatorB = Waveform::Sine;
            patch.oscillatorC = Waveform::Square;
            patch.oscillatorCEnabled = variant >= 3;
            patch.fmEnabled = true;
            patch.fmAmount = 0.12 + t * 0.38;
            patch.fmRatio = 1.5 + t * 3.0;
            patch.fmFeedback = t * 0.24;
            patch.ampEnvelope.attack = 0.001;
            patch.ampEnvelope.decay = 0.05 + t * 0.22;
            patch.ampEnvelope.sustain = 0.0 + t * 0.16;
            patch.ampEnvelope.release = 0.03 + t * 0.3;
            break;
        case 2: // Organs
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Square;
            patch.oscillatorC = Waveform::Saw;
            patch.oscillatorCEnabled = true;
            patch.subEnabled = true;
            patch.subOscillator = 0.12 + t * 0.12;
            patch.cutoff = 0.88;
            patch.resonance = 0.04 + t * 0.08;
            patch.ampEnvelope.attack = 0.001;
            patch.ampEnvelope.decay = 0.03 + t * 0.06;
            patch.ampEnvelope.sustain = 0.9;
            patch.ampEnvelope.release = 0.06 + t * 0.1;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.15 + t * 0.15;
            break;
        case 3: // Guitars
            patch.oscillatorA = variant >= 5 ? Waveform::Saw : Waveform::Square;
            patch.oscillatorB = Waveform::Saw;
            patch.pulseWidth = 0.38 + t * 0.2;
            patch.filterEnvelopeAmount = 0.22 + t * 0.3;
            patch.ampEnvelope.attack = 0.001;
            patch.ampEnvelope.decay = 0.06 + t * 0.22;
            patch.ampEnvelope.sustain = 0.18 + t * 0.52;
            patch.ampEnvelope.release = 0.05 + t * 0.18;
            patch.drive = 0.14 + t * 0.36;
            patch.wavefold = std::clamp((t - 0.5) * 0.45, 0.0, 1.0);
            break;
        case 4: // Basses
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.subEnabled = true;
            patch.subOscillator = 0.25 + t * 0.2;
            patch.cutoff = 0.24 + t * 0.26;
            patch.resonance = 0.12 + t * 0.22;
            patch.filterEnvelopeAmount = 0.16 + t * 0.24;
            patch.ampEnvelope.attack = 0.001;
            patch.ampEnvelope.decay = 0.06 + t * 0.18;
            patch.ampEnvelope.sustain = 0.52 + t * 0.34;
            patch.ampEnvelope.release = 0.06 + t * 0.16;
            patch.drive = 0.18 + t * 0.22;
            break;
        case 5: // Solo strings
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Triangle;
            patch.unisonVoices = 2 + (variant >= 4 ? 1 : 0);
            patch.unisonDetuneCents = 3.5 + t * 5.0;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.18 + t * 0.2;
            patch.chorusDepth = 0.32 + t * 0.3;
            patch.ampEnvelope.attack = 0.02 + t * 0.12;
            patch.ampEnvelope.decay = 0.18 + t * 0.25;
            patch.ampEnvelope.sustain = 0.72 + t * 0.22;
            patch.ampEnvelope.release = 0.25 + t * 0.55;
            break;
        case 6: // Ensemble/choir
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.oscillatorD = Waveform::Sine;
            patch.oscillatorDEnabled = true;
            patch.oscillatorDMix = 0.12 + t * 0.22;
            patch.unisonVoices = 3 + (variant >= 4 ? 1 : 0);
            patch.unisonDetuneCents = 4.0 + t * 8.0;
            patch.stereoSpread = 0.32 + t * 0.48;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.25 + t * 0.3;
            patch.ampEnvelope.attack = 0.03 + t * 0.14;
            patch.ampEnvelope.decay = 0.24 + t * 0.36;
            patch.ampEnvelope.sustain = 0.68 + t * 0.3;
            patch.ampEnvelope.release = 0.35 + t * 0.65;
            break;
        case 7: // Brass
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.oscillatorC = Waveform::Saw;
            patch.oscillatorCEnabled = true;
            patch.oscillatorCMix = 0.14 + t * 0.24;
            patch.cutoff = 0.38 + t * 0.22;
            patch.resonance = 0.18 + t * 0.18;
            patch.filterEnvelopeAmount = 0.26 + t * 0.32;
            patch.ampEnvelope.attack = 0.008 + t * 0.04;
            patch.ampEnvelope.decay = 0.12 + t * 0.2;
            patch.ampEnvelope.sustain = 0.58 + t * 0.32;
            patch.ampEnvelope.release = 0.12 + t * 0.24;
            patch.drive = 0.16 + t * 0.22;
            break;
        case 8: // Reeds
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Triangle;
            patch.pulseWidth = 0.38 + t * 0.24;
            patch.vibratoCents = 2.0 + t * 7.0;
            patch.lfoRate = 4.5 + t * 3.5;
            patch.cutoff = 0.55 + t * 0.25;
            patch.resonance = 0.12 + t * 0.16;
            patch.ampEnvelope.attack = 0.004 + t * 0.02;
            patch.ampEnvelope.decay = 0.1 + t * 0.14;
            patch.ampEnvelope.sustain = 0.62 + t * 0.3;
            patch.ampEnvelope.release = 0.08 + t * 0.2;
            break;
        case 9: // Pipes/flutes
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Triangle;
            patch.oscillatorBEnabled = true;
            patch.oscillatorCEnabled = variant >= 5;
            patch.oscillatorC = Waveform::Square;
            patch.cutoff = 0.72 + t * 0.18;
            patch.resonance = 0.06 + t * 0.14;
            patch.vibratoCents = 1.5 + t * 5.0;
            patch.lfoRate = 4.8 + t * 2.8;
            patch.ampEnvelope.attack = 0.004 + t * 0.03;
            patch.ampEnvelope.decay = 0.08 + t * 0.18;
            patch.ampEnvelope.sustain = 0.66 + t * 0.28;
            patch.ampEnvelope.release = 0.12 + t * 0.24;
            break;
        case 10: // Synth leads
            patch.oscillatorA = variant == 0 ? Waveform::Square : Waveform::Saw;
            patch.oscillatorB = variant == 2 ? Waveform::Triangle : Waveform::Square;
            patch.oscillatorDEnabled = variant >= 5;
            patch.oscillatorD = Waveform::Sine;
            patch.oscillatorDMix = 0.08 + t * 0.24;
            patch.unisonVoices = 2 + (variant >= 4 ? 1 : 0);
            patch.unisonDetuneCents = 4.0 + t * 8.0;
            patch.cutoff = 0.58 + t * 0.22;
            patch.resonance = 0.14 + t * 0.2;
            patch.filterEnvelopeAmount = 0.28 + t * 0.26;
            patch.vibratoCents = 3.0 + t * 8.0;
            patch.lfoRate = 5.4 + t * 3.2;
            patch.ampEnvelope.attack = 0.003 + t * 0.015;
            patch.ampEnvelope.decay = 0.08 + t * 0.16;
            patch.ampEnvelope.sustain = 0.54 + t * 0.34;
            patch.ampEnvelope.release = 0.08 + t * 0.16;
            patch.drive = 0.14 + t * 0.2;
            break;
        case 11: // Pads
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Triangle;
            patch.oscillatorC = Waveform::Sine;
            patch.oscillatorCEnabled = true;
            patch.oscillatorCMix = 0.2 + t * 0.25;
            patch.unisonVoices = 4;
            patch.unisonDetuneCents = 6.0 + t * 6.0;
            patch.stereoSpread = 0.4 + t * 0.4;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.24 + t * 0.3;
            patch.chorusDepth = 0.35 + t * 0.35;
            patch.cutoff = 0.46 + t * 0.3;
            patch.resonance = 0.08 + t * 0.14;
            patch.ampEnvelope.attack = 0.04 + t * 0.2;
            patch.ampEnvelope.decay = 0.22 + t * 0.42;
            patch.ampEnvelope.sustain = 0.72 + t * 0.24;
            patch.ampEnvelope.release = 0.35 + t * 0.75;
            break;
        case 12: // Synth FX
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.oscillatorC = Waveform::Noise;
            patch.oscillatorCEnabled = true;
            patch.oscillatorCMix = 0.06 + t * 0.36;
            patch.fmEnabled = true;
            patch.fmAmount = 0.14 + t * 0.55;
            patch.fmRatio = 1.8 + t * 4.2;
            patch.ringEnabled = variant >= 3;
            patch.ringMod = std::clamp((t - 0.2) * 0.8, 0.0, 1.0);
            patch.bitCrushEnabled = variant >= 5;
            patch.bitCrush = std::clamp((t - 0.4) * 0.8, 0.0, 1.0);
            patch.sampleRateReduction = std::clamp((t - 0.45) * 0.9, 0.0, 1.0);
            patch.combMix = std::clamp((t - 0.2) * 0.5, 0.0, 1.0);
            patch.combFeedback = 0.12 + t * 0.58;
            patch.ampEnvelope.attack = 0.006 + t * 0.06;
            patch.ampEnvelope.decay = 0.12 + t * 0.3;
            patch.ampEnvelope.sustain = 0.35 + t * 0.45;
            patch.ampEnvelope.release = 0.12 + t * 0.45;
            break;
        case 13: // Ethnic
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Saw;
            patch.oscillatorC = Waveform::Triangle;
            patch.oscillatorCEnabled = variant >= 2;
            patch.pulseWidth = 0.34 + t * 0.3;
            patch.filterEnvelopeAmount = 0.22 + t * 0.32;
            patch.cutoff = 0.5 + t * 0.32;
            patch.resonance = 0.14 + t * 0.24;
            patch.ampEnvelope.attack = 0.002 + t * 0.03;
            patch.ampEnvelope.decay = 0.08 + t * 0.28;
            patch.ampEnvelope.sustain = 0.26 + t * 0.52;
            patch.ampEnvelope.release = 0.06 + t * 0.22;
            patch.transientShape = 0.15 + t * 0.35;
            break;
        case 15: // SFX
            patch.oscillatorA = variant < 4 ? Waveform::Noise : Waveform::Saw;
            patch.oscillatorB = Waveform::Sine;
            patch.oscillatorC = Waveform::Square;
            patch.oscillatorCEnabled = true;
            patch.oscillatorCMix = 0.15 + t * 0.45;
            patch.fmEnabled = true;
            patch.fmAmount = 0.24 + t * 0.6;
            patch.fmRatio = 1.2 + t * 5.2;
            patch.ringEnabled = true;
            patch.ringMod = 0.18 + t * 0.72;
            patch.hardSyncEnabled = variant >= 3;
            patch.hardSync = std::clamp((t - 0.2) * 0.95, 0.0, 1.0);
            patch.bitCrushEnabled = true;
            patch.bitCrush = std::clamp((t * 0.9), 0.0, 1.0);
            patch.sampleRateReduction = std::clamp((t * 0.85), 0.0, 1.0);
            patch.transientNoise = 0.22 + t * 0.6;
            patch.transientShape = 0.3 + t * 0.6;
            patch.transientPitchSemitones = -24.0 + t * 48.0;
            patch.transientDecay = 0.01 + t * 0.09;
            patch.ampEnvelope.attack = 0.001 + t * 0.06;
            patch.ampEnvelope.decay = 0.04 + t * 0.4;
            patch.ampEnvelope.sustain = 0.08 + t * 0.52;
            patch.ampEnvelope.release = 0.04 + t * 0.55;
            break;
        default:
            break;
    }

    // Program-aware refinements to increase GM patch identity.
    switch (program) {
        case 0: // Acoustic Grand Piano
        case 1: // Bright Acoustic Piano
        case 2: // Electric Grand Piano
        case 3: // Honky-tonk Piano
            patch.transientShape = std::max(patch.transientShape, 0.34);
            patch.click = std::max(patch.click, 0.15);
            patch.filterEnvelopeAmount = std::max(patch.filterEnvelopeAmount, 0.22);
            patch.ampEnvelope.attack = std::min(patch.ampEnvelope.attack, 0.004);
            patch.ampEnvelope.release = std::max(patch.ampEnvelope.release, 0.16);
            break;
        case 4: // Electric Piano 1
        case 5: // Electric Piano 2
            patch.fmEnabled = true;
            patch.fmAmount = std::max(patch.fmAmount, 0.18);
            patch.fmRatio = 2.0 + t * 1.8;
            patch.chorusEnabled = true;
            patch.chorusMix = std::max(patch.chorusMix, 0.22);
            patch.chorusDepth = std::max(patch.chorusDepth, 0.33);
            break;
        case 16: // Drawbar Organ
        case 17: // Percussive Organ
        case 18: // Rock Organ
            patch.subEnabled = true;
            patch.subOscillator = std::max(patch.subOscillator, 0.2);
            patch.drive = std::max(patch.drive, 0.16);
            patch.vibratoCents = std::max(patch.vibratoCents, 1.2);
            break;
        case 29: // Overdriven Guitar
        case 30: // Distortion Guitar
            patch.drive = std::max(patch.drive, 0.48);
            patch.wavefold = std::max(patch.wavefold, 0.16);
            patch.cutoff = std::min(patch.cutoff, 0.66);
            patch.highPass = std::max(patch.highPass, 0.12);
            break;
        case 56: // Trumpet
        case 57: // Trombone
        case 58: // Tuba
        case 59: // Muted Trumpet
        case 60: // French Horn
        case 61: // Brass Section
            patch.filterEnvelopeAmount = std::max(patch.filterEnvelopeAmount, 0.34);
            patch.resonance = std::max(patch.resonance, 0.2);
            patch.drive = std::max(patch.drive, 0.18);
            patch.ampEnvelope.attack = std::min(patch.ampEnvelope.attack, 0.012);
            break;
        case 64: // Soprano Sax
        case 65: // Alto Sax
        case 66: // Tenor Sax
        case 67: // Baritone Sax
        case 68: // Oboe
        case 69: // English Horn
        case 70: // Bassoon
        case 71: // Clarinet
            patch.oscillatorA = Waveform::Square;
            patch.pulseWidth = 0.4;
            patch.vibratoCents = std::max(patch.vibratoCents, 2.6);
            patch.lfoRate = std::clamp(patch.lfoRate, 4.2, 6.5);
            patch.cutoff = std::max(patch.cutoff, 0.58);
            break;
        case 72: // Piccolo
        case 73: // Flute
        case 74: // Recorder
        case 75: // Pan Flute
        case 76: // Blown Bottle
        case 77: // Shakuhachi
        case 78: // Whistle
        case 79: // Ocarina
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Triangle;
            patch.oscillatorBEnabled = true;
            patch.noiseEnabled = true;
            patch.noise = std::max(0.01, patch.noise * 0.6);
            patch.noiseTone = std::max(patch.noiseTone, 0.72);
            patch.ampEnvelope.attack = std::max(patch.ampEnvelope.attack, 0.006);
            patch.ampEnvelope.release = std::max(patch.ampEnvelope.release, 0.18);
            break;
        case 38: // Synth Bass 1
        case 39: // Synth Bass 2
            patch.subEnabled = true;
            patch.subOscillator = std::max(patch.subOscillator, 0.42);
            patch.unisonVoices = std::max(patch.unisonVoices, 2);
            patch.detuneCents = std::max(patch.detuneCents, 11.0);
            patch.detuneCCents = std::min(patch.detuneCCents, -13.0);
            patch.drive = std::max(patch.drive, 0.3);
            patch.filterEnvelopeAmount = std::max(patch.filterEnvelopeAmount, 0.32);
            break;
        case 48: // String Ensemble 1
        case 49: // String Ensemble 2
        case 50: // SynthStrings 1
        case 51: // SynthStrings 2
            patch.unisonVoices = std::max(patch.unisonVoices, 4);
            patch.unisonDetuneCents = std::max(patch.unisonDetuneCents, 9.0);
            patch.stereoSpread = std::max(patch.stereoSpread, 0.52);
            patch.chorusEnabled = true;
            patch.chorusMix = std::max(patch.chorusMix, 0.3);
            patch.ampEnvelope.attack = std::max(patch.ampEnvelope.attack, 0.035);
            patch.ampEnvelope.release = std::max(patch.ampEnvelope.release, 0.5);
            break;
        case 62: // Synth Brass 1
        case 63: // Synth Brass 2
            patch.unisonVoices = std::max(patch.unisonVoices, 3);
            patch.detuneCents = std::max(patch.detuneCents, 9.0);
            patch.cutoff = std::min(patch.cutoff, 0.62);
            patch.resonance = std::max(patch.resonance, 0.22);
            patch.filterEnvelopeAmount = std::max(patch.filterEnvelopeAmount, 0.36);
            patch.drive = std::max(patch.drive, 0.26);
            break;
        case 80: // Lead 1 (square)
            patch.oscillatorA = Waveform::Square;
            patch.pulseWidth = 0.43;
            patch.vibratoCents = std::max(patch.vibratoCents, 5.0);
            patch.unisonVoices = std::max(patch.unisonVoices, 2);
            break;
        case 81: // Lead 2 (sawtooth)
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Saw;
            patch.unisonVoices = std::max(patch.unisonVoices, 3);
            patch.unisonDetuneCents = std::max(patch.unisonDetuneCents, 8.0);
            patch.drive = std::max(patch.drive, 0.24);
            break;
        case 87: // Lead 8 (bass + lead)
            patch.subEnabled = true;
            patch.subOscillator = std::max(patch.subOscillator, 0.36);
            patch.cutoff = std::min(patch.cutoff, 0.56);
            patch.filterEnvelopeAmount = std::max(patch.filterEnvelopeAmount, 0.34);
            break;
        case 89: // Pad 2 (warm)
        case 90: // Pad 3 (polysynth)
        case 95: // Pad 8 (sweep)
            patch.chorusEnabled = true;
            patch.chorusMix = std::max(patch.chorusMix, 0.34);
            patch.chorusDepth = std::max(patch.chorusDepth, 0.5);
            patch.unisonVoices = std::max(patch.unisonVoices, 4);
            patch.stereoSpread = std::max(patch.stereoSpread, 0.6);
            patch.filterEnvelopeAmount = std::max(patch.filterEnvelopeAmount, 0.24);
            patch.lfoFilterDepth = std::max(patch.lfoFilterDepth, 0.18);
            break;
        case 96: // FX 1 (rain)
        case 97: // FX 2 (soundtrack)
        case 98: // FX 3 (crystal)
        case 99: // FX 4 (atmosphere)
        case 100: // FX 5 (brightness)
        case 101: // FX 6
        case 102: // FX 7 (echoes)
        case 103: // FX 8 (sci-fi)
            patch.oscillatorC = Waveform::Noise;
            patch.oscillatorCEnabled = true;
            patch.oscillatorCMix = std::max(patch.oscillatorCMix, 0.2);
            patch.combMix = std::max(patch.combMix, 0.24);
            patch.combFeedback = std::max(patch.combFeedback, 0.35);
            patch.chorusEnabled = true;
            patch.chorusMix = std::max(patch.chorusMix, 0.36);
            patch.ampEnvelope.attack = std::max(patch.ampEnvelope.attack, 0.02);
            patch.ampEnvelope.release = std::max(patch.ampEnvelope.release, 0.4);
            break;
        case 112: // Tinkle Bell
        case 113: // Agogo
        case 114: // Steel Drums
        case 115: // Woodblock
        case 116: // Taiko Drum
        case 117: // Melodic Tom
        case 118: // Synth Drum
        case 119: // Reverse Cymbal
            patch.fmEnabled = true;
            patch.fmAmount = std::max(patch.fmAmount, 0.24);
            patch.fmRatio = std::max(patch.fmRatio, 2.8);
            patch.transientShape = std::max(patch.transientShape, 0.5);
            patch.transientNoise = std::max(patch.transientNoise, 0.28);
            patch.ampEnvelope.decay = std::min(patch.ampEnvelope.decay, 0.24);
            patch.ampEnvelope.sustain = std::min(patch.ampEnvelope.sustain, 0.28);
            break;
        default:
            break;
    }

    if (program >= 88 && program <= 95) {
        patch.chorusEnabled = true;
        patch.chorusMix = std::max(patch.chorusMix, 0.28);
        patch.stereoSpread = std::max(patch.stereoSpread, 0.45);
    }
    if (program >= 80 && program <= 87) {
        patch.vibratoCents = std::max(patch.vibratoCents, 4.0);
        patch.drive = std::max(patch.drive, 0.16);
    }
    if (laneHas("bass")) {
        patch.subEnabled = true;
        patch.subOscillator = std::max(patch.subOscillator, 0.28);
        patch.cutoff = std::min(patch.cutoff, 0.62);
        patch.filterEnvelopeAmount = std::max(patch.filterEnvelopeAmount, 0.22);
    } else if (laneHas("lead")) {
        patch.vibratoCents = std::max(patch.vibratoCents, 3.5);
        patch.unisonVoices = std::max(patch.unisonVoices, 2);
        patch.stereoSpread = std::max(patch.stereoSpread, 0.25);
    } else if (laneHas("pad")) {
        patch.chorusEnabled = true;
        patch.chorusMix = std::max(patch.chorusMix, 0.28);
        patch.ampEnvelope.attack = std::max(patch.ampEnvelope.attack, 0.035);
        patch.ampEnvelope.release = std::max(patch.ampEnvelope.release, 0.45);
    } else if (laneHas("arp") || laneHas("pluck")) {
        patch.ampEnvelope.attack = std::min(patch.ampEnvelope.attack, 0.003);
        patch.ampEnvelope.decay = std::min(patch.ampEnvelope.decay, 0.16);
        patch.ampEnvelope.release = std::min(patch.ampEnvelope.release, 0.18);
        patch.filterEnvelopeAmount = std::max(patch.filterEnvelopeAmount, 0.3);
        patch.cutoff = std::max(patch.cutoff, 0.58);
    } else if (laneHas("fx")) {
        patch.chorusEnabled = true;
        patch.chorusMix = std::max(patch.chorusMix, 0.26);
        patch.combMix = std::max(patch.combMix, 0.18);
        patch.ampEnvelope.release = std::max(patch.ampEnvelope.release, 0.28);
    }

    return patch;
}

int trackPriority(const MidiMessage& message) {
    switch (message.kind) {
        case MidiMessage::Kind::Tempo:
        case MidiMessage::Kind::TrackName:
            return 0;
        case MidiMessage::Kind::ProgramChange:
        case MidiMessage::Kind::ControlChange:
        case MidiMessage::Kind::PitchBend:
            return 1;
        case MidiMessage::Kind::NoteOff:
            return 2;
        case MidiMessage::Kind::NoteOn:
            return 3;
    }
    return 4;
}

void applyPerformanceEffects(const ChannelPerformanceState& state, PatternStep& step) {
    auto addEffect = [&step](const std::string& name, double value) {
        EffectCommand effect;
        effect.name = name;
        effect.parameters["value"] = value;
        step.effects.push_back(effect);
    };

    if (state.ccPan != 64) {
        addEffect("pan", ccPanToSigned(state.ccPan));
    }
    const double volumeScale = ccToUnit(state.ccVolume) * ccToUnit(state.ccExpression);
    if (std::abs(volumeScale - 1.0) > 0.0001) {
        addEffect("velocity", volumeScale);
    }
    if (state.ccCutoff != 64) {
        addEffect("cutoff", ccToUnit(state.ccCutoff));
    }
    if (state.ccResonance != 64) {
        addEffect("resonance", ccToUnit(state.ccResonance));
    }
    if (state.ccModWheel > 0) {
        addEffect("vibrato", ccToUnit(state.ccModWheel) * 18.0);
    }
    if (state.pitchBend != 8192) {
        const double normalized = std::clamp(
            static_cast<double>(state.pitchBend - 8192) / 8192.0,
            -1.0,
            1.0);
        addEffect("transpose", normalized * 2.0);
    }
}
} // namespace

MidiImportReport importMidiFile(const std::string& path, const MidiImportOptions& options) {
    if (options.rowsPerBeat <= 0) {
        throw std::invalid_argument("MIDI import rows_per_beat must be positive");
    }
    if (options.patternRows <= 0) {
        throw std::invalid_argument("MIDI import pattern_rows must be positive");
    }
    if (options.sampleRate <= 0) {
        throw std::invalid_argument("MIDI import sample_rate must be positive");
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("failed to open MIDI file for reading: " + path);
    }

    char header[4] {};
    in.read(header, 4);
    if (!in || std::string(header, 4) != "MThd") {
        throw std::runtime_error("invalid MIDI header chunk");
    }
    const std::uint32_t headerLength = readU32BE(in);
    if (headerLength < 6) {
        throw std::runtime_error("invalid MIDI header length");
    }
    const int midiFormat = static_cast<int>(readU16BE(in));
    const int trackCount = static_cast<int>(readU16BE(in));
    const int division = static_cast<int>(readU16BE(in));
    if (headerLength > 6) {
        in.ignore(static_cast<std::streamsize>(headerLength - 6));
    }
    if (midiFormat != 0 && midiFormat != 1) {
        throw std::runtime_error("only MIDI format 0 and 1 are supported");
    }
    if (trackCount <= 0) {
        throw std::runtime_error("MIDI file has no tracks");
    }
    if (division <= 0) {
        throw std::runtime_error("SMPTE MIDI timing is not supported");
    }

    std::vector<MidiMessage> messages;
    messages.reserve(4096);
    int maxTick = 0;
    for (int track = 0; track < trackCount; ++track) {
        char trackHeader[4] {};
        in.read(trackHeader, 4);
        if (!in || std::string(trackHeader, 4) != "MTrk") {
            throw std::runtime_error("invalid MIDI track chunk");
        }
        const std::uint32_t length = readU32BE(in);
        std::vector<std::uint8_t> trackData(length);
        if (!trackData.empty()) {
            in.read(reinterpret_cast<char*>(trackData.data()), static_cast<std::streamsize>(trackData.size()));
            if (!in) {
                throw std::runtime_error("unexpected end of MIDI track data");
            }
        }
        parseTrackMessages(trackData, track, messages, maxTick);
    }

    std::stable_sort(messages.begin(), messages.end(), [](const MidiMessage& left, const MidiMessage& right) {
        if (left.tick != right.tick) {
            return left.tick < right.tick;
        }
        const int leftPriority = trackPriority(left);
        const int rightPriority = trackPriority(right);
        if (leftPriority != rightPriority) {
            return leftPriority < rightPriority;
        }
        return left.track < right.track;
    });

    std::vector<std::string> trackNames(static_cast<std::size_t>(trackCount));
    int initialTempoMicrosecondsPerQuarter = 500000;
    bool sawTempo = false;
    std::vector<TempoPoint> tempoPoints;
    std::unordered_map<std::uint16_t, std::vector<NoteStartState>> activeNotes;
    std::vector<ImportedNote> importedNotes;
    importedNotes.reserve(2048);
    std::array<ChannelPerformanceState, 16> channels {};

    for (const MidiMessage& message : messages) {
        if (message.kind == MidiMessage::Kind::TrackName) {
            if (message.track >= 0 && message.track < trackCount && !message.text.empty()) {
                trackNames[static_cast<std::size_t>(message.track)] = message.text;
            }
            continue;
        }
        if (message.kind == MidiMessage::Kind::Tempo) {
            if (message.data1 > 0) {
                if (!sawTempo) {
                    initialTempoMicrosecondsPerQuarter = message.data1;
                    sawTempo = true;
                }
                tempoPoints.push_back({message.tick, message.data1});
            }
            continue;
        }

        ChannelPerformanceState& channelState = channels[static_cast<std::size_t>(message.channel)];
        if (message.kind == MidiMessage::Kind::ProgramChange) {
            channelState.program = std::clamp(message.data1, 0, 127);
            continue;
        }
        if (message.kind == MidiMessage::Kind::ControlChange) {
            const int controller = message.data1;
            const int value = std::clamp(message.data2, 0, 127);
            if (controller == 1) {
                channelState.ccModWheel = value;
            } else if (controller == 7) {
                channelState.ccVolume = value;
            } else if (controller == 10) {
                channelState.ccPan = value;
            } else if (controller == 11) {
                channelState.ccExpression = value;
            } else if (controller == 71) {
                channelState.ccResonance = value;
            } else if (controller == 74) {
                channelState.ccCutoff = value;
            }
            continue;
        }
        if (message.kind == MidiMessage::Kind::PitchBend) {
            channelState.pitchBend = std::clamp(message.data1, 0, 16383);
            continue;
        }

        const int note = std::clamp(message.data1, 0, 127);
        const std::uint16_t activeKey = static_cast<std::uint16_t>((message.channel << 8) | note);
        if (message.kind == MidiMessage::Kind::NoteOn) {
            NoteStartState start;
            start.tick = message.tick;
            start.track = message.track;
            start.channel = message.channel;
            start.midi = note;
            start.velocity = std::clamp(message.data2, 1, 127);
            start.performance = channelState;
            activeNotes[activeKey].push_back(start);
            continue;
        }

        auto it = activeNotes.find(activeKey);
        if (it == activeNotes.end() || it->second.empty()) {
            continue;
        }
        NoteStartState start = it->second.back();
        it->second.pop_back();
        if (it->second.empty()) {
            activeNotes.erase(it);
        }
        if (message.tick <= start.tick) {
            continue;
        }

        ImportedNote imported;
        imported.startTick = start.tick;
        imported.endTick = message.tick;
        imported.sourceTrack = start.track;
        imported.channel = start.channel;
        imported.midi = start.midi;
        imported.velocity = start.velocity;
        imported.program = start.performance.program;
        imported.performance = start.performance;
        importedNotes.push_back(imported);
    }

    for (const auto& [key, starts] : activeNotes) {
        (void)key;
        for (const NoteStartState& start : starts) {
            ImportedNote imported;
            imported.startTick = start.tick;
            imported.endTick = std::max(start.tick + 1, maxTick + 1);
            imported.sourceTrack = start.track;
            imported.channel = start.channel;
            imported.midi = start.midi;
            imported.velocity = start.velocity;
            imported.program = start.performance.program;
            imported.performance = start.performance;
            importedNotes.push_back(imported);
        }
    }

    if (importedNotes.empty()) {
        throw std::runtime_error("MIDI file has no note events");
    }

    std::stable_sort(importedNotes.begin(), importedNotes.end(), [](const ImportedNote& left, const ImportedNote& right) {
        if (left.startTick != right.startTick) {
            return left.startTick < right.startTick;
        }
        if (left.sourceTrack != right.sourceTrack) {
            return left.sourceTrack < right.sourceTrack;
        }
        return left.channel < right.channel;
    });

    Song song;
    song.title = std::filesystem::path(path).stem().string();
    song.author = "Imported MIDI";
    song.description = "Imported from " + std::filesystem::path(path).filename().string();
    song.rowsPerBeat = options.rowsPerBeat;
    song.sampleRate = options.sampleRate;
    if (initialTempoMicrosecondsPerQuarter > 0) {
        song.bpm = 60000000.0 / static_cast<double>(initialTempoMicrosecondsPerQuarter);
    }
    std::vector<std::string> importWarnings;
    const TempoMap tempoMap = buildTempoMap(std::move(tempoPoints), division);
    const double baseSecondsPerRow = 60.0
        / std::max(1e-9, song.bpm)
        / static_cast<double>(song.rowsPerBeat);

    std::unordered_map<LaneKey, std::vector<ImportedNote>, LaneKeyHash> notesByLane;
    notesByLane.reserve(importedNotes.size());
    for (const ImportedNote& note : importedNotes) {
        const LaneKey lane {
            options.splitByTrack ? note.sourceTrack : 0,
            note.channel,
            options.splitByProgram ? note.program : 0
        };
        notesByLane[lane].push_back(note);
    }

    struct VoiceLane {
        LaneKey lane;
        std::string trackName;
        std::vector<ImportedNote> notes;
    };
    std::vector<VoiceLane> voiceLanes;

    for (auto& [lane, laneNotes] : notesByLane) {
        std::stable_sort(laneNotes.begin(), laneNotes.end(), [](const ImportedNote& left, const ImportedNote& right) {
            if (left.startTick != right.startTick) {
                return left.startTick < right.startTick;
            }
            return left.endTick < right.endTick;
        });

        std::vector<int> voiceEndTicks;
        std::vector<std::vector<ImportedNote>> voices;
        for (const ImportedNote& note : laneNotes) {
            int selectedVoice = -1;
            for (int voiceIndex = 0; voiceIndex < static_cast<int>(voiceEndTicks.size()); ++voiceIndex) {
                if (note.startTick >= voiceEndTicks[static_cast<std::size_t>(voiceIndex)]) {
                    selectedVoice = voiceIndex;
                    break;
                }
            }
            if (selectedVoice < 0) {
                selectedVoice = static_cast<int>(voiceEndTicks.size());
                voiceEndTicks.push_back(note.endTick);
                voices.emplace_back();
            } else {
                voiceEndTicks[static_cast<std::size_t>(selectedVoice)] = note.endTick;
            }
            voices[static_cast<std::size_t>(selectedVoice)].push_back(note);
        }

        const std::string baseName = !trackNames[static_cast<std::size_t>(lane.sourceTrack)].empty()
            ? trackNames[static_cast<std::size_t>(lane.sourceTrack)]
            : fallbackTrackName(lane.sourceTrack, lane.channel);
        for (int voice = 0; voice < static_cast<int>(voices.size()); ++voice) {
            VoiceLane laneVoice;
            laneVoice.lane = lane;
            laneVoice.trackName = baseName;
            if (options.splitByProgram) {
                laneVoice.trackName += " " + gmProgramName(lane.program);
            }
            if (voices.size() > 1) {
                laneVoice.trackName += " V" + std::to_string(voice + 1);
            }
            laneVoice.notes = std::move(voices[static_cast<std::size_t>(voice)]);
            voiceLanes.push_back(std::move(laneVoice));
        }
    }

    std::sort(voiceLanes.begin(), voiceLanes.end(), [](const VoiceLane& left, const VoiceLane& right) {
        if (left.lane.sourceTrack != right.lane.sourceTrack) {
            return left.lane.sourceTrack < right.lane.sourceTrack;
        }
        if (left.lane.channel != right.lane.channel) {
            return left.lane.channel < right.lane.channel;
        }
        if (left.lane.program != right.lane.program) {
            return left.lane.program < right.lane.program;
        }
        return left.trackName < right.trackName;
    });

    song.tracks.clear();
    song.instruments.clear();
    std::vector<int> laneInstrumentByTrack;
    laneInstrumentByTrack.reserve(voiceLanes.size());
    std::vector<int> dominantProgramByTrack;
    dominantProgramByTrack.reserve(voiceLanes.size());
    for (const VoiceLane& lane : voiceLanes) {
        Track track;
        track.name = lane.trackName;
        const double averageScale = averagePerformanceVolumeScale(lane.notes);
        const double compensation = std::clamp(1.08 / averageScale, 0.9, 2.8);
        track.volume = std::clamp(track.volume * compensation, 0.65, 2.0);
        song.tracks.push_back(track);

        const int dominantProgram = dominantProgramForLane(lane.notes);
        dominantProgramByTrack.push_back(dominantProgram);
        const double averageMidi = averageMidiForLane(lane.notes);
        Instrument instrument;
        instrument.id = static_cast<int>(song.instruments.size());
        instrument.patch = buildPatchForLane(lane.lane.channel, dominantProgram, averageMidi, lane.trackName);
        const double averageVelocity = averageVelocityForLane(lane.notes);
        const double velocityCompensation = std::clamp(0.9 / averageVelocity, 0.9, 1.8);
        instrument.patch.gain = std::clamp(instrument.patch.gain * velocityCompensation, 0.35, 1.8);
        const double projected = track.volume * instrument.patch.gain * averageVelocity * averageScale;
        if (projected < 0.42) {
            const double boost = std::clamp(0.56 / std::max(projected, 1e-6), 1.0, 2.2);
            track.volume = std::clamp(track.volume * std::sqrt(boost), 0.75, 2.0);
            instrument.patch.gain = std::clamp(instrument.patch.gain * std::sqrt(boost), 0.45, 1.95);
            song.tracks.back().volume = track.volume;
        }
        song.instruments.push_back(instrument);
        laneInstrumentByTrack.push_back(instrument.id);

        const int programCount = distinctProgramCountForLane(lane.notes);
        if (programCount > 1) {
            importWarnings.push_back(
                "track '" + lane.trackName + "' contains multiple MIDI programs; using one editable patch");
        }
    }

    int highestRow = 0;
    auto rowPositionForTick = [&](int tick) {
        if (!options.preserveTempoMap) {
            return static_cast<double>(tick) * static_cast<double>(song.rowsPerBeat) / static_cast<double>(division);
        }
        const double seconds = tickToSeconds(tempoMap, division, tick);
        return seconds / std::max(1e-9, baseSecondsPerRow);
    };
    for (const VoiceLane& lane : voiceLanes) {
        for (const ImportedNote& note : lane.notes) {
            const int row = std::max(0, static_cast<int>(std::llround(rowPositionForTick(note.startTick))));
            highestRow = std::max(highestRow, row + 1);
        }
    }
    const int totalRows = std::max(options.patternRows, highestRow + 1);
    const int patternCount = std::max(1, (totalRows + options.patternRows - 1) / options.patternRows);

    song.patterns.clear();
    song.order.clear();
    for (int patternIndex = 0; patternIndex < patternCount; ++patternIndex) {
        Pattern pattern(
            "Pattern " + std::to_string(patternIndex + 1),
            options.patternRows,
            static_cast<int>(song.tracks.size()));
        song.patterns.push_back(pattern);
        song.order.push_back(patternIndex);
    }

    for (int trackIndex = 0; trackIndex < static_cast<int>(voiceLanes.size()); ++trackIndex) {
        const VoiceLane& lane = voiceLanes[static_cast<std::size_t>(trackIndex)];
        for (const ImportedNote& note : lane.notes) {
            const double rowPosition = rowPositionForTick(note.startTick);
            const int absoluteRow = std::max(0, static_cast<int>(std::llround(rowPosition)));
            const double microOffsetRows = rowPosition - static_cast<double>(absoluteRow);
            const int patternIndex = std::clamp(absoluteRow / options.patternRows, 0, patternCount - 1);
            const int rowInPattern = std::clamp(absoluteRow % options.patternRows, 0, options.patternRows - 1);

            PatternStep& step = song.patterns[static_cast<std::size_t>(patternIndex)].step(rowInPattern, trackIndex);
            if (step.note.has_value()) {
                continue;
            }

            step.note = Note(note.midi, std::clamp(static_cast<float>(note.velocity / 127.0f), 0.0f, 1.0f));
            step.instrument = laneInstrumentByTrack[static_cast<std::size_t>(trackIndex)];
            const double durationRows = std::max(
                0.05,
                rowPositionForTick(note.endTick) - rowPositionForTick(note.startTick));
            step.gate = std::max(0.05, durationRows);
            step.microOffsetRows = microOffsetRows;
            applyPerformanceEffects(note.performance, step);
        }
    }

    if (song.instruments.empty()) {
        song.instruments.push_back(Instrument {});
        song.instruments.front().patch.name = "MIDI Import";
    }

    MidiImportReport report;
    report.song = std::move(song);
    report.midiFormat = midiFormat;
    report.ticksPerQuarterNote = division;
    report.importedTrackCount = static_cast<int>(voiceLanes.size());
    report.importedNoteCount = static_cast<int>(importedNotes.size());
    report.trackMappings.reserve(voiceLanes.size());
    for (int trackIndex = 0; trackIndex < static_cast<int>(voiceLanes.size()); ++trackIndex) {
        MidiImportTrackMapping mapping;
        mapping.trackIndex = trackIndex;
        mapping.trackName = report.song.tracks[static_cast<std::size_t>(trackIndex)].name;
        mapping.instrumentIndex = laneInstrumentByTrack[static_cast<std::size_t>(trackIndex)];
        mapping.instrumentName =
            report.song.instruments[static_cast<std::size_t>(mapping.instrumentIndex)].patch.name;
        mapping.midiSourceTrack = voiceLanes[static_cast<std::size_t>(trackIndex)].lane.sourceTrack;
        mapping.midiChannel = voiceLanes[static_cast<std::size_t>(trackIndex)].lane.channel;
        mapping.dominantProgram = dominantProgramByTrack[static_cast<std::size_t>(trackIndex)];
        report.trackMappings.push_back(mapping);
    }
    if (!sawTempo) {
        report.warnings.push_back({"MIDI file had no tempo meta event; using 120 BPM"});
    } else if (!options.preserveTempoMap && tempoMap.points.size() > 1) {
        report.warnings.push_back(
            {"MIDI file has tempo changes; preserve_tempo_map=false may alter timing"});
    }
    if (!activeNotes.empty()) {
        report.warnings.push_back({"some MIDI notes had no note-off event and were closed at file end"});
    }
    for (const std::string& warning : importWarnings) {
        report.warnings.push_back({warning});
    }
    return report;
}

Song loadMidiFileAsSong(const std::string& path, const MidiImportOptions& options) {
    return importMidiFile(path, options).song;
}

} // namespace arachno
