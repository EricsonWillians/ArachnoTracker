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
constexpr double kCompetitionMidiQuality = 0.82;
constexpr double kCompetitionMidiQualityPercussive = 0.76;

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
        patch.click = std::clamp(0.12 + t * 0.2, 0.0, 1.0);
        patch.transientNoise = std::clamp(0.2 + t * 0.2, 0.0, 1.0);
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
    patch.drive = std::clamp(0.02 + t * 0.06, 0.0, 1.0);
    patch.cutoff = std::clamp(0.45 + t * 0.35, 0.0, 1.0);
    patch.resonance = std::clamp(0.08 + t * 0.22, 0.0, 1.0);
    patch.filterEnvelopeAmount = std::clamp(0.14 + t * 0.26, 0.0, 1.0);
    patch.ampEnvelope.attack = 0.004 + t * 0.02;
    patch.ampEnvelope.decay = 0.08 + t * 0.2;
    patch.ampEnvelope.sustain = std::clamp(0.35 + t * 0.55, 0.0, 1.0);
    patch.ampEnvelope.release = 0.08 + t * 0.45;
    patch.gain = std::clamp(0.48 + t * 0.16, 0.0, 1.2);

    // Key-scaled envelope time compensation (shorter for high notes)
    const double keyScaleFactor = std::clamp(1.0 - (averageMidi - 60.0) / 72.0, 0.35, 1.6);

    switch (family) {
        case 0: // Pianos
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Triangle;
            patch.oscillatorC = Waveform::Sine;
            patch.oscillatorCEnabled = true;
            patch.oscillatorCMix = 0.08 + t * 0.12;
            patch.pulseWidth = 0.46 - t * 0.16;
            patch.pwmDepth = 0.04 + t * 0.08;
            patch.transientShape = 0.38 + t * 0.22;
            patch.transientNoise = 0.04 + t * 0.06;
            patch.transientTone = 0.55 + t * 0.25;
            patch.click = 0.08 + t * 0.1;
            patch.subEnabled = false;
            patch.chorusEnabled = variant >= 4;
            patch.chorusMix = 0.08 + t * 0.14;
            patch.chorusRate = 0.28 + t * 0.22;
            patch.chorusDepth = 0.25 + t * 0.25;
            patch.cutoff = 0.62 + t * 0.22;
            patch.resonance = 0.1 + t * 0.14;
            patch.filterEnvelopeAmount = 0.18 + t * 0.22;
            patch.filterKeytrack = 0.45 + t * 0.25;
            patch.ampEnvelope.attack = (0.001 + t * 0.003) * keyScaleFactor;
            patch.ampEnvelope.decay = (0.12 + t * 0.28) * keyScaleFactor;
            patch.ampEnvelope.sustain = std::clamp(0.22 + t * 0.38, 0.0, 1.0);
            patch.ampEnvelope.release = (0.28 + t * 0.45) * keyScaleFactor;
            patch.ampEnvelopeCurve = 2; // logarithmic decay for piano-like character
            patch.velocityToAmp = 0.85;
            patch.velocityToFilter = 0.45;
            patch.velocityToAttack = 0.35;
            patch.combMix = 0.06 + t * 0.1;
            patch.combTime = 0.035 + t * 0.025;
            patch.combFeedback = 0.18 + t * 0.22;
            patch.reverbMix = 0.08 + t * 0.12;
            patch.reverbSize = 0.55 + t * 0.25;
            patch.reverbDamping = 0.42 + t * 0.18;
            patch.analogColor = 0.38 + t * 0.22;
            patch.toneTilt = -0.18 - t * 0.12;
            patch.gain = std::clamp(0.52 + t * 0.14, 0.0, 1.2);
            break;
        case 1: // Chromatic percussion
            patch.oscillatorA = Waveform::Triangle;
            patch.oscillatorB = Waveform::Sine;
            patch.oscillatorC = Waveform::Square;
            patch.oscillatorCEnabled = variant >= 3;
            patch.oscillatorCMix = 0.1 + t * 0.15;
            patch.fmEnabled = true;
            patch.fmAmount = 0.12 + t * 0.22;
            patch.fmRatio = 2.0 + t * 2.0;
            patch.fmFeedback = 0.08 + t * 0.28;
            patch.fmAlgorithm = variant >= 4 ? 3 : 1;
            patch.cutoff = 0.55 + t * 0.25;
            patch.resonance = 0.18 + t * 0.22;
            patch.filterEnvelopeAmount = 0.22 + t * 0.28;
            patch.filterKeytrack = 0.35 + t * 0.25;
            patch.ampEnvelope.attack = 0.0005;
            patch.ampEnvelope.decay = (0.08 + t * 0.35) * keyScaleFactor;
            patch.ampEnvelope.sustain = std::clamp(0.0 + t * 0.08, 0.0, 1.0);
            patch.ampEnvelope.release = (0.04 + t * 0.35) * keyScaleFactor;
            patch.ampEnvelopeCurve = 3; // analog RC for natural mallet decay
            patch.velocityToAmp = 0.92;
            patch.velocityToFilter = 0.55;
            patch.velocityToAttack = 0.55;
            patch.transientShape = 0.28 + t * 0.32;
            patch.transientNoise = 0.04 + t * 0.06;
            patch.click = 0.06 + t * 0.08;
            patch.combMix = 0.08 + t * 0.14;
            patch.combTime = 0.02 + t * 0.03;
            patch.combFeedback = 0.25 + t * 0.35;
            patch.reverbMix = 0.06 + t * 0.1;
            patch.reverbSize = 0.48 + t * 0.22;
            patch.gain = std::clamp(0.48 + t * 0.18, 0.0, 1.2);
            break;
        case 2: // Organs
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Square;
            patch.oscillatorC = Waveform::Saw;
            patch.oscillatorCEnabled = true;
            patch.oscillatorCMix = 0.15 + t * 0.2;
            patch.oscillatorD = Waveform::Sine;
            patch.oscillatorDEnabled = variant >= 3;
            patch.oscillatorDMix = 0.08 + t * 0.18;
            patch.subEnabled = true;
            patch.subOscillator = 0.15 + t * 0.18;
            patch.cutoff = 0.82 + t * 0.12;
            patch.resonance = 0.06 + t * 0.1;
            patch.filterEnvelopeAmount = 0.08 + t * 0.12;
            patch.ampEnvelope.attack = 0.008 + t * 0.025;
            patch.ampEnvelope.decay = 0.04 + t * 0.08;
            patch.ampEnvelope.sustain = 0.88 + t * 0.1;
            patch.ampEnvelope.release = 0.04 + t * 0.12;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.18 + t * 0.22;
            patch.chorusRate = 0.35 + t * 0.35;
            patch.chorusDepth = 0.28 + t * 0.32;
            patch.chorusEnsemble = 0.25 + t * 0.35;
            patch.tremoloDepth = 0.04 + t * 0.12;
            patch.lfoRate = 4.2 + t * 3.8;
            patch.vibratoCents = 1.5 + t * 3.5;
            patch.velocityToAmp = 0.65;
            patch.velocityToFilter = 0.15;
            patch.reverbMix = 0.12 + t * 0.18;
            patch.reverbSize = 0.62 + t * 0.22;
            patch.reverbDamping = 0.38 + t * 0.18;
            patch.analogColor = 0.55 + t * 0.25;
            patch.gain = std::clamp(0.42 + t * 0.16, 0.0, 1.2);
            break;
        case 3: // Guitars
            patch.oscillatorA = variant >= 5 ? Waveform::Saw : Waveform::Square;
            patch.oscillatorB = Waveform::Saw;
            patch.oscillatorC = Waveform::Triangle;
            patch.oscillatorCEnabled = variant >= 2;
            patch.oscillatorCMix = 0.08 + t * 0.18;
            patch.pulseWidth = 0.34 + t * 0.18;
            patch.pwmDepth = 0.02 + t * 0.06;
            patch.filterEnvelopeAmount = 0.28 + t * 0.32;
            patch.filterKeytrack = 0.38 + t * 0.22;
            patch.ampEnvelope.attack = (0.001 + t * 0.004) * keyScaleFactor;
            patch.ampEnvelope.decay = (0.08 + t * 0.28) * keyScaleFactor;
            patch.ampEnvelope.sustain = std::clamp(0.12 + t * 0.48, 0.0, 1.0);
            patch.ampEnvelope.release = (0.1 + t * 0.28) * keyScaleFactor;
            patch.ampEnvelopeCurve = variant >= 4 ? 0 : 2;
            patch.drive = 0.05 + t * 0.1;
            patch.velocityToAmp = 0.88;
            patch.velocityToFilter = 0.42;
            patch.velocityToAttack = 0.45;
            patch.transientShape = 0.22 + t * 0.28;
            patch.transientNoise = 0.02 + t * 0.04;
            patch.click = 0.05 + t * 0.06;
            patch.combMix = 0.04 + t * 0.12;
            patch.combTime = 0.025 + t * 0.02;
            patch.combFeedback = 0.22 + t * 0.28;
            patch.reverbMix = 0.06 + t * 0.12;
            patch.reverbSize = 0.45 + t * 0.2;
            patch.analogColor = 0.48 + t * 0.22;
            patch.gain = std::clamp(0.5 + t * 0.14, 0.0, 1.2);
            break;
        case 4: // Basses
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.oscillatorC = Waveform::Sine;
            patch.oscillatorCEnabled = variant >= 2;
            patch.oscillatorCMix = 0.06 + t * 0.14;
            patch.subEnabled = true;
            patch.subOscillator = 0.32 + t * 0.22;
            patch.cutoff = 0.2 + t * 0.22;
            patch.resonance = 0.18 + t * 0.28;
            patch.filterEnvelopeAmount = 0.22 + t * 0.32;
            patch.filterKeytrack = 0.55 + t * 0.25;
            patch.ampEnvelope.attack = 0.001;
            patch.ampEnvelope.decay = (0.08 + t * 0.22) * keyScaleFactor;
            patch.ampEnvelope.sustain = std::clamp(0.45 + t * 0.38, 0.0, 1.0);
            patch.ampEnvelope.release = (0.06 + t * 0.18) * keyScaleFactor;
            patch.drive = 0.08 + t * 0.12;
            patch.velocityToAmp = 0.82;
            patch.velocityToFilter = 0.55;
            patch.velocityToAttack = 0.38;
            patch.filterNonlinearity = 0.35 + t * 0.35;
            patch.combMix = 0.02 + t * 0.08;
            patch.combTime = 0.03 + t * 0.025;
            patch.combFeedback = 0.15 + t * 0.25;
            patch.reverbMix = 0.04 + t * 0.08;
            patch.analogColor = 0.62 + t * 0.22;
            patch.toneTilt = -0.28 - t * 0.18;
            patch.lowPunch = 0.55 + t * 0.25;
            patch.gain = std::clamp(0.55 + t * 0.16, 0.0, 1.2);
            break;
        case 5: // Solo strings
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Triangle;
            patch.oscillatorC = Waveform::Sine;
            patch.oscillatorCEnabled = variant >= 3;
            patch.oscillatorCMix = 0.06 + t * 0.14;
            patch.unisonVoices = 2 + (variant >= 4 ? 2 : 0);
            patch.unisonDetuneCents = 3.5 + t * 6.0;
            patch.stereoSpread = 0.18 + t * 0.32;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.12 + t * 0.18;
            patch.chorusRate = 0.22 + t * 0.28;
            patch.chorusDepth = 0.28 + t * 0.32;
            patch.cutoff = 0.52 + t * 0.22;
            patch.resonance = 0.1 + t * 0.16;
            patch.filterEnvelopeAmount = 0.12 + t * 0.18;
            patch.filterKeytrack = 0.28 + t * 0.22;
            patch.ampEnvelope.attack = (0.025 + t * 0.14) * keyScaleFactor;
            patch.ampEnvelope.decay = (0.18 + t * 0.28) * keyScaleFactor;
            patch.ampEnvelope.sustain = std::clamp(0.68 + t * 0.24, 0.0, 1.0);
            patch.ampEnvelope.release = (0.28 + t * 0.55) * keyScaleFactor;
            patch.ampEnvelopeCurve = 3;
            patch.vibratoCents = 2.5 + t * 5.5;
            patch.lfoRate = 3.8 + t * 3.2;
            patch.lfoPanDepth = 0.04 + t * 0.1;
            patch.velocityToAmp = 0.78;
            patch.velocityToFilter = 0.38;
            patch.velocityToAttack = 0.28;
            patch.reverbMix = 0.1 + t * 0.16;
            patch.reverbSize = 0.58 + t * 0.22;
            patch.reverbDamping = 0.45 + t * 0.2;
            patch.analogColor = 0.52 + t * 0.22;
            patch.gain = std::clamp(0.48 + t * 0.14, 0.0, 1.2);
            break;
        case 6: // Ensemble/choir
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.oscillatorC = Waveform::Triangle;
            patch.oscillatorCEnabled = true;
            patch.oscillatorCMix = 0.1 + t * 0.18;
            patch.oscillatorD = Waveform::Sine;
            patch.oscillatorDEnabled = true;
            patch.oscillatorDMix = 0.08 + t * 0.18;
            patch.unisonVoices = 4 + (variant >= 4 ? 2 : 0);
            patch.unisonDetuneCents = 5.0 + t * 9.0;
            patch.stereoSpread = 0.42 + t * 0.48;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.28 + t * 0.32;
            patch.chorusRate = 0.18 + t * 0.22;
            patch.chorusDepth = 0.42 + t * 0.38;
            patch.chorusEnsemble = 0.35 + t * 0.35;
            patch.cutoff = 0.42 + t * 0.28;
            patch.resonance = 0.08 + t * 0.12;
            patch.filterEnvelopeAmount = 0.12 + t * 0.18;
            patch.ampEnvelope.attack = (0.04 + t * 0.18) * keyScaleFactor;
            patch.ampEnvelope.decay = (0.22 + t * 0.38) * keyScaleFactor;
            patch.ampEnvelope.sustain = std::clamp(0.72 + t * 0.22, 0.0, 1.0);
            patch.ampEnvelope.release = (0.38 + t * 0.72) * keyScaleFactor;
            patch.ampEnvelopeCurve = 3;
            patch.vibratoCents = 1.8 + t * 4.2;
            patch.lfoRate = 2.8 + t * 2.8;
            patch.lfoPanDepth = 0.06 + t * 0.12;
            patch.velocityToAmp = 0.55;
            patch.velocityToFilter = 0.22;
            patch.reverbMix = 0.18 + t * 0.22;
            patch.reverbSize = 0.68 + t * 0.22;
            patch.reverbDamping = 0.35 + t * 0.18;
            patch.reverbDiffusion = 0.55 + t * 0.25;
            patch.analogColor = 0.48 + t * 0.22;
            patch.gain = std::clamp(0.38 + t * 0.14, 0.0, 1.2);
            break;
        case 7: // Brass
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.oscillatorC = Waveform::Saw;
            patch.oscillatorCEnabled = true;
            patch.oscillatorCMix = 0.12 + t * 0.22;
            patch.oscillatorD = Waveform::Sine;
            patch.oscillatorDEnabled = variant >= 3;
            patch.oscillatorDMix = 0.04 + t * 0.12;
            patch.cutoff = 0.32 + t * 0.18;
            patch.resonance = 0.22 + t * 0.22;
            patch.filterEnvelopeAmount = 0.32 + t * 0.38;
            patch.filterKeytrack = 0.42 + t * 0.28;
            patch.ampEnvelope.attack = (0.012 + t * 0.055) * keyScaleFactor;
            patch.ampEnvelope.decay = (0.14 + t * 0.22) * keyScaleFactor;
            patch.ampEnvelope.sustain = std::clamp(0.55 + t * 0.32, 0.0, 1.0);
            patch.ampEnvelope.release = (0.14 + t * 0.28) * keyScaleFactor;
            patch.ampEnvelopeCurve = 3;
            patch.drive = 0.06 + t * 0.12;
            patch.filterNonlinearity = 0.25 + t * 0.35;
            patch.velocityToAmp = 0.82;
            patch.velocityToFilter = 0.62;
            patch.velocityToAttack = 0.42;
            patch.vibratoCents = 2.0 + t * 5.5;
            patch.lfoRate = 4.2 + t * 3.8;
            patch.noiseEnabled = true;
            patch.noise = 0.004 + t * 0.008;
            patch.noiseTone = 0.62 + t * 0.22;
            patch.reverbMix = 0.08 + t * 0.14;
            patch.reverbSize = 0.52 + t * 0.22;
            patch.analogColor = 0.58 + t * 0.22;
            patch.gain = std::clamp(0.52 + t * 0.16, 0.0, 1.2);
            break;
        case 8: // Reeds
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Triangle;
            patch.oscillatorC = Waveform::Sine;
            patch.oscillatorCEnabled = variant >= 2;
            patch.oscillatorCMix = 0.06 + t * 0.14;
            patch.pulseWidth = 0.36 + t * 0.22;
            patch.pwmDepth = 0.03 + t * 0.08;
            patch.vibratoCents = 2.5 + t * 7.5;
            patch.lfoRate = 4.2 + t * 3.8;
            patch.cutoff = 0.52 + t * 0.22;
            patch.resonance = 0.16 + t * 0.2;
            patch.filterEnvelopeAmount = 0.18 + t * 0.28;
            patch.filterKeytrack = 0.32 + t * 0.22;
            patch.ampEnvelope.attack = (0.006 + t * 0.035) * keyScaleFactor;
            patch.ampEnvelope.decay = (0.12 + t * 0.18) * keyScaleFactor;
            patch.ampEnvelope.sustain = std::clamp(0.62 + t * 0.28, 0.0, 1.0);
            patch.ampEnvelope.release = (0.1 + t * 0.24) * keyScaleFactor;
            patch.ampEnvelopeCurve = 3;
            patch.velocityToAmp = 0.75;
            patch.velocityToFilter = 0.48;
            patch.velocityToAttack = 0.32;
            patch.noiseEnabled = true;
            patch.noise = 0.005 + t * 0.01;
            patch.noiseTone = 0.58 + t * 0.22;
            patch.reverbMix = 0.06 + t * 0.12;
            patch.reverbSize = 0.48 + t * 0.2;
            patch.analogColor = 0.45 + t * 0.2;
            patch.gain = std::clamp(0.5 + t * 0.14, 0.0, 1.2);
            break;
        case 9: // Pipes/flutes
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Triangle;
            patch.oscillatorBEnabled = true;
            patch.oscillatorCEnabled = variant >= 5;
            patch.oscillatorC = Waveform::Square;
            patch.oscillatorCMix = 0.04 + t * 0.12;
            patch.cutoff = 0.68 + t * 0.16;
            patch.resonance = 0.08 + t * 0.14;
            patch.filterEnvelopeAmount = 0.06 + t * 0.12;
            patch.filterKeytrack = 0.25 + t * 0.2;
            patch.vibratoCents = 2.0 + t * 6.0;
            patch.lfoRate = 4.2 + t * 3.2;
            patch.ampEnvelope.attack = (0.008 + t * 0.045) * keyScaleFactor;
            patch.ampEnvelope.decay = (0.1 + t * 0.18) * keyScaleFactor;
            patch.ampEnvelope.sustain = std::clamp(0.68 + t * 0.22, 0.0, 1.0);
            patch.ampEnvelope.release = (0.14 + t * 0.32) * keyScaleFactor;
            patch.ampEnvelopeCurve = 3;
            patch.velocityToAmp = 0.68;
            patch.velocityToFilter = 0.28;
            patch.velocityToAttack = 0.22;
            patch.noiseEnabled = true;
            patch.noise = 0.005 + t * 0.012;
            patch.noiseTone = 0.68 + t * 0.22;
            patch.reverbMix = 0.1 + t * 0.16;
            patch.reverbSize = 0.58 + t * 0.22;
            patch.reverbDamping = 0.42 + t * 0.18;
            patch.analogColor = 0.42 + t * 0.18;
            patch.gain = std::clamp(0.48 + t * 0.14, 0.0, 1.2);
            break;
        case 10: // Synth leads
            patch.oscillatorA = variant == 0 ? Waveform::Square : Waveform::Saw;
            patch.oscillatorB = variant == 2 ? Waveform::Triangle : Waveform::Square;
            patch.oscillatorC = Waveform::Sine;
            patch.oscillatorCEnabled = variant >= 3;
            patch.oscillatorCMix = 0.06 + t * 0.14;
            patch.oscillatorDEnabled = variant >= 5;
            patch.oscillatorD = Waveform::Sine;
            patch.oscillatorDMix = 0.06 + t * 0.18;
            patch.unisonVoices = 2 + (variant >= 4 ? 2 : 0);
            patch.unisonDetuneCents = 4.5 + t * 9.0;
            patch.stereoSpread = 0.22 + t * 0.38;
            patch.cutoff = 0.55 + t * 0.2;
            patch.resonance = 0.16 + t * 0.22;
            patch.filterEnvelopeAmount = 0.32 + t * 0.28;
            patch.filterKeytrack = 0.35 + t * 0.2;
            patch.vibratoCents = 3.5 + t * 9.0;
            patch.lfoRate = 5.2 + t * 3.8;
            patch.lfoFilterDepth = 0.04 + t * 0.12;
            patch.ampEnvelope.attack = 0.002 + t * 0.012;
            patch.ampEnvelope.decay = 0.08 + t * 0.18;
            patch.ampEnvelope.sustain = std::clamp(0.52 + t * 0.32, 0.0, 1.0);
            patch.ampEnvelope.release = 0.08 + t * 0.18;
            patch.drive = 0.06 + t * 0.1;
            patch.filterNonlinearity = 0.2 + t * 0.3;
            patch.velocityToAmp = 0.72;
            patch.velocityToFilter = 0.45;
            patch.velocityToAttack = 0.35;
            patch.hardSyncEnabled = variant >= 6;
            patch.hardSync = std::clamp((t - 0.5) * 0.4, 0.0, 1.0);
            patch.reverbMix = 0.06 + t * 0.12;
            patch.reverbSize = 0.48 + t * 0.2;
            patch.analogColor = 0.55 + t * 0.25;
            patch.gain = std::clamp(0.45 + t * 0.14, 0.0, 1.2);
            break;
        case 11: // Pads
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Triangle;
            patch.oscillatorC = Waveform::Sine;
            patch.oscillatorCEnabled = true;
            patch.oscillatorCMix = 0.15 + t * 0.22;
            patch.oscillatorD = Waveform::Square;
            patch.oscillatorDEnabled = variant >= 3;
            patch.oscillatorDMix = 0.04 + t * 0.12;
            patch.unisonVoices = 5;
            patch.unisonDetuneCents = 7.0 + t * 8.0;
            patch.stereoSpread = 0.48 + t * 0.42;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.28 + t * 0.32;
            patch.chorusRate = 0.15 + t * 0.2;
            patch.chorusDepth = 0.42 + t * 0.38;
            patch.chorusEnsemble = 0.3 + t * 0.35;
            patch.cutoff = 0.42 + t * 0.28;
            patch.resonance = 0.1 + t * 0.14;
            patch.filterEnvelopeAmount = 0.1 + t * 0.16;
            patch.lfoFilterDepth = 0.06 + t * 0.14;
            patch.lfoRate = 2.2 + t * 2.8;
            patch.ampEnvelope.attack = (0.06 + t * 0.28) * keyScaleFactor;
            patch.ampEnvelope.decay = (0.28 + t * 0.48) * keyScaleFactor;
            patch.ampEnvelope.sustain = std::clamp(0.75 + t * 0.2, 0.0, 1.0);
            patch.ampEnvelope.release = (0.42 + t * 0.82) * keyScaleFactor;
            patch.ampEnvelopeCurve = 3;
            patch.velocityToAmp = 0.48;
            patch.velocityToFilter = 0.18;
            patch.reverbMix = 0.22 + t * 0.28;
            patch.reverbSize = 0.72 + t * 0.22;
            patch.reverbDamping = 0.32 + t * 0.18;
            patch.reverbDiffusion = 0.58 + t * 0.22;
            patch.reverbEarlyMix = 0.22 + t * 0.28;
            patch.analogColor = 0.42 + t * 0.18;
            patch.gain = std::clamp(0.35 + t * 0.12, 0.0, 1.2);
            break;
        case 12: // Synth FX
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.oscillatorC = Waveform::Noise;
            patch.oscillatorCEnabled = true;
            patch.oscillatorCMix = 0.1 + t * 0.38;
            patch.oscillatorD = Waveform::Sine;
            patch.oscillatorDEnabled = variant >= 4;
            patch.oscillatorDMix = 0.06 + t * 0.18;
            patch.fmEnabled = true;
            patch.fmAmount = 0.18 + t * 0.58;
            patch.fmRatio = 2.0 + t * 4.5;
            patch.fmFeedback = 0.1 + t * 0.35;
            patch.fmAlgorithm = variant >= 3 ? 3 : 2;
            patch.ringEnabled = variant >= 3;
            patch.ringMod = std::clamp((t - 0.15) * 0.85, 0.0, 1.0);
            patch.bitCrushEnabled = variant >= 4;
            patch.bitCrush = std::clamp((t - 0.35) * 0.7, 0.0, 1.0);
            patch.sampleRateReduction = std::clamp((t - 0.4) * 0.85, 0.0, 1.0);
            patch.wavefold = std::clamp((t - 0.2) * 0.45, 0.0, 1.0);
            patch.combMix = std::clamp((t - 0.15) * 0.55, 0.0, 1.0);
            patch.combFeedback = 0.18 + t * 0.52;
            patch.ampEnvelope.attack = 0.004 + t * 0.045;
            patch.ampEnvelope.decay = 0.14 + t * 0.35;
            patch.ampEnvelope.sustain = std::clamp(0.3 + t * 0.42, 0.0, 1.0);
            patch.ampEnvelope.release = 0.14 + t * 0.48;
            patch.velocityToAmp = 0.55;
            patch.velocityToFilter = 0.25;
            patch.reverbMix = 0.12 + t * 0.2;
            patch.reverbSize = 0.62 + t * 0.25;
            patch.analogColor = 0.55 + t * 0.25;
            patch.gain = std::clamp(0.42 + t * 0.14, 0.0, 1.2);
            break;
        case 13: // Ethnic
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Saw;
            patch.oscillatorC = Waveform::Triangle;
            patch.oscillatorCEnabled = variant >= 2;
            patch.oscillatorCMix = 0.08 + t * 0.18;
            patch.pulseWidth = 0.32 + t * 0.28;
            patch.pwmDepth = 0.02 + t * 0.08;
            patch.filterEnvelopeAmount = 0.25 + t * 0.35;
            patch.filterKeytrack = 0.3 + t * 0.22;
            patch.cutoff = 0.48 + t * 0.28;
            patch.resonance = 0.16 + t * 0.22;
            patch.ampEnvelope.attack = (0.003 + t * 0.035) * keyScaleFactor;
            patch.ampEnvelope.decay = (0.1 + t * 0.32) * keyScaleFactor;
            patch.ampEnvelope.sustain = std::clamp(0.22 + t * 0.52, 0.0, 1.0);
            patch.ampEnvelope.release = (0.08 + t * 0.28) * keyScaleFactor;
            patch.ampEnvelopeCurve = 2;
            patch.transientShape = 0.2 + t * 0.4;
            patch.transientNoise = 0.02 + t * 0.04;
            patch.velocityToAmp = 0.72;
            patch.velocityToFilter = 0.35;
            patch.reverbMix = 0.08 + t * 0.14;
            patch.reverbSize = 0.55 + t * 0.22;
            patch.analogColor = 0.48 + t * 0.22;
            patch.gain = std::clamp(0.48 + t * 0.14, 0.0, 1.2);
            break;
        case 15: // SFX
            patch.oscillatorA = variant < 4 ? Waveform::Noise : Waveform::Saw;
            patch.oscillatorB = Waveform::Sine;
            patch.oscillatorC = Waveform::Square;
            patch.oscillatorCEnabled = true;
            patch.oscillatorCMix = 0.18 + t * 0.42;
            patch.fmEnabled = true;
            patch.fmAmount = 0.28 + t * 0.65;
            patch.fmRatio = 1.5 + t * 5.5;
            patch.fmFeedback = 0.12 + t * 0.42;
            patch.fmAlgorithm = 3;
            patch.ringEnabled = true;
            patch.ringMod = 0.22 + t * 0.68;
            patch.hardSyncEnabled = variant >= 3;
            patch.hardSync = std::clamp((t - 0.15) * 0.95, 0.0, 1.0);
            patch.bitCrushEnabled = true;
            patch.bitCrush = std::clamp((t * 0.85), 0.0, 1.0);
            patch.sampleRateReduction = std::clamp((t * 0.8), 0.0, 1.0);
            patch.wavefold = std::clamp(t * 0.5, 0.0, 1.0);
            patch.transientNoise = 0.28 + t * 0.58;
            patch.transientShape = 0.35 + t * 0.55;
            patch.transientPitchSemitones = -28.0 + t * 52.0;
            patch.transientDecay = 0.012 + t * 0.1;
            patch.ampEnvelope.attack = 0.001 + t * 0.065;
            patch.ampEnvelope.decay = 0.06 + t * 0.42;
            patch.ampEnvelope.sustain = std::clamp(0.06 + t * 0.48, 0.0, 1.0);
            patch.ampEnvelope.release = 0.06 + t * 0.55;
            patch.velocityToAmp = 0.45;
            patch.velocityToFilter = 0.2;
            patch.reverbMix = 0.1 + t * 0.18;
            patch.reverbSize = 0.68 + t * 0.22;
            patch.analogColor = 0.55 + t * 0.25;
            patch.gain = std::clamp(0.4 + t * 0.14, 0.0, 1.2);
            break;
        default:
            break;
    }

    // Program-aware refinements to increase GM patch identity.
    switch (program) {
        // === PIANOS (0-7) ===
        case 0: // Acoustic Grand Piano
            patch.transientShape = std::max(patch.transientShape, 0.42);
            patch.transientTone = std::max(patch.transientTone, 0.62);
            patch.click = std::max(patch.click, 0.28);
            patch.filterEnvelopeAmount = std::max(patch.filterEnvelopeAmount, 0.28);
            patch.filterKeytrack = std::max(patch.filterKeytrack, 0.55);
            patch.ampEnvelope.attack = std::min(patch.ampEnvelope.attack, 0.002);
            patch.ampEnvelope.decay = std::max(patch.ampEnvelope.decay, 0.18);
            patch.ampEnvelope.sustain = std::max(patch.ampEnvelope.sustain, 0.32);
            patch.ampEnvelope.release = std::max(patch.ampEnvelope.release, 0.32);
            patch.combMix = std::max(patch.combMix, 0.1);
            patch.combFeedback = std::max(patch.combFeedback, 0.28);
            patch.reverbMix = std::max(patch.reverbMix, 0.12);
            patch.reverbSize = std::max(patch.reverbSize, 0.62);
            patch.analogColor = std::max(patch.analogColor, 0.48);
            break;
        case 1: // Bright Acoustic Piano
            patch.transientShape = std::max(patch.transientShape, 0.38);
            patch.click = std::max(patch.click, 0.22);
            patch.cutoff = std::max(patch.cutoff, 0.72);
            patch.filterEnvelopeAmount = std::max(patch.filterEnvelopeAmount, 0.32);
            patch.ampEnvelope.attack = std::min(patch.ampEnvelope.attack, 0.002);
            patch.ampEnvelope.release = std::max(patch.ampEnvelope.release, 0.28);
            patch.toneTilt = std::max(patch.toneTilt, 0.08);
            break;
        case 2: // Electric Grand Piano
            patch.fmEnabled = true;
            patch.fmAmount = std::max(patch.fmAmount, 0.22);
            patch.fmRatio = 2.4;
            patch.transientShape = std::max(patch.transientShape, 0.32);
            patch.click = std::max(patch.click, 0.18);
            patch.cutoff = std::max(patch.cutoff, 0.68);
            patch.chorusEnabled = true;
            patch.chorusMix = std::max(patch.chorusMix, 0.18);
            break;
        case 3: // Honky-tonk Piano
            patch.transientShape = std::max(patch.transientShape, 0.45);
            patch.transientNoise = std::max(patch.transientNoise, 0.06);
            patch.click = std::max(patch.click, 0.12);
            patch.cutoff = std::max(patch.cutoff, 0.58);
            patch.resonance = std::max(patch.resonance, 0.18);
            patch.combMix = std::max(patch.combMix, 0.12);
            patch.combFeedback = std::max(patch.combFeedback, 0.18);
            patch.analogColor = std::max(patch.analogColor, 0.55);
            break;
        case 4: // Electric Piano 1 (Rhodes)
            patch.fmEnabled = true;
            patch.fmAmount = 0.28;
            patch.fmRatio = 2.0;
            patch.fmFeedback = 0.15;
            patch.oscillatorA = Waveform::Triangle;
            patch.oscillatorB = Waveform::Sine;
            patch.transientShape = 0.22;
            patch.click = 0.08;
            patch.cutoff = 0.62;
            patch.resonance = 0.12;
            patch.filterEnvelopeAmount = 0.22;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.28;
            patch.chorusRate = 0.45;
            patch.chorusDepth = 0.38;
            patch.ampEnvelope.attack = 0.002;
            patch.ampEnvelope.decay = 0.18;
            patch.ampEnvelope.sustain = 0.42;
            patch.ampEnvelope.release = 0.22;
            patch.velocityToAmp = 0.88;
            patch.velocityToFilter = 0.52;
            patch.reverbMix = 0.1;
            patch.analogColor = 0.48;
            patch.gain = 0.58;
            break;
        case 5: // Electric Piano 2 (FM/DX)
            patch.fmEnabled = true;
            patch.fmAmount = 0.42;
            patch.fmRatio = 3.5;
            patch.fmFeedback = 0.28;
            patch.fmAlgorithm = 2;
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Sine;
            patch.transientShape = 0.15;
            patch.click = 0.05;
            patch.cutoff = 0.72;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.22;
            patch.chorusRate = 0.52;
            patch.chorusDepth = 0.42;
            patch.ampEnvelope.attack = 0.001;
            patch.ampEnvelope.decay = 0.22;
            patch.ampEnvelope.sustain = 0.38;
            patch.ampEnvelope.release = 0.18;
            patch.velocityToAmp = 0.92;
            patch.velocityToFilter = 0.62;
            patch.reverbMix = 0.08;
            patch.gain = 0.52;
            break;
        case 6: // Harpsichord
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Square;
            patch.pulseWidth = 0.28;
            patch.pwmDepth = 0.0;
            patch.transientShape = 0.45;
            patch.transientNoise = 0.06;
            patch.click = 0.05;
            patch.cutoff = 0.48;
            patch.resonance = 0.22;
            patch.filterEnvelopeAmount = 0.15;
            patch.filterKeytrack = 0.35;
            patch.ampEnvelope.attack = 0.001;
            patch.ampEnvelope.decay = 0.08;
            patch.ampEnvelope.sustain = 0.05;
            patch.ampEnvelope.release = 0.12;
            patch.ampEnvelopeCurve = 2;
            patch.combMix = 0.15;
            patch.combTime = 0.025;
            patch.combFeedback = 0.22;
            patch.velocityToAmp = 0.85;
            patch.velocityToAttack = 0.45;
            patch.analogColor = 0.42;
            patch.gain = 0.48;
            break;
        case 7: // Clavinet
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Saw;
            patch.pulseWidth = 0.32;
            patch.pwmDepth = 0.08;
            patch.transientShape = 0.35;
            patch.click = 0.22;
            patch.cutoff = 0.55;
            patch.resonance = 0.18;
            patch.filterEnvelopeAmount = 0.28;
            patch.filterKeytrack = 0.42;
            patch.ampEnvelope.attack = 0.001;
            patch.ampEnvelope.decay = 0.12;
            patch.ampEnvelope.sustain = 0.22;
            patch.ampEnvelope.release = 0.14;
            patch.drive = 0.1;
            patch.velocityToAmp = 0.9;
            patch.velocityToFilter = 0.58;
            patch.analogColor = 0.52;
            patch.gain = 0.55;
            break;

        // === CHROMATIC PERCUSSION (8-15) ===
        case 8: // Celesta
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Triangle;
            patch.oscillatorC = Waveform::Sine;
            patch.oscillatorCEnabled = true;
            patch.fmEnabled = true;
            patch.fmAmount = 0.15;
            patch.fmRatio = 4.0;
            patch.transientShape = 0.35;
            patch.click = 0.1;
            patch.cutoff = 0.62;
            patch.resonance = 0.28;
            patch.ampEnvelope.attack = 0.0005;
            patch.ampEnvelope.decay = 0.28;
            patch.ampEnvelope.sustain = 0.02;
            patch.ampEnvelope.release = 0.18;
            patch.combMix = 0.12;
            patch.combFeedback = 0.25;
            patch.reverbMix = 0.12;
            patch.reverbSize = 0.55;
            patch.gain = 0.42;
            break;
        case 9: // Glockenspiel
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Sine;
            patch.fmEnabled = true;
            patch.fmAmount = 0.22;
            patch.fmRatio = 5.0;
            patch.transientShape = 0.38;
            patch.click = 0.12;
            patch.cutoff = 0.72;
            patch.resonance = 0.32;
            patch.ampEnvelope.attack = 0.0003;
            patch.ampEnvelope.decay = 0.35;
            patch.ampEnvelope.sustain = 0.0;
            patch.ampEnvelope.release = 0.22;
            patch.combMix = 0.18;
            patch.combFeedback = 0.18;
            patch.reverbMix = 0.15;
            patch.reverbSize = 0.62;
            patch.gain = 0.38;
            break;
        case 10: // Music Box
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Triangle;
            patch.transientShape = 0.3;
            patch.click = 0.1;
            patch.cutoff = 0.58;
            patch.resonance = 0.25;
            patch.ampEnvelope.attack = 0.0005;
            patch.ampEnvelope.decay = 0.42;
            patch.ampEnvelope.sustain = 0.0;
            patch.ampEnvelope.release = 0.28;
            patch.combMix = 0.1;
            patch.combFeedback = 0.22;
            patch.reverbMix = 0.12;
            patch.gain = 0.4;
            break;
        case 11: // Vibraphone
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Triangle;
            patch.oscillatorC = Waveform::Sine;
            patch.oscillatorCEnabled = true;
            patch.fmEnabled = true;
            patch.fmAmount = 0.12;
            patch.fmRatio = 2.8;
            patch.transientShape = 0.32;
            patch.click = 0.08;
            patch.cutoff = 0.65;
            patch.resonance = 0.22;
            patch.ampEnvelope.attack = 0.004;
            patch.ampEnvelope.decay = 0.35;
            patch.ampEnvelope.sustain = 0.18;
            patch.ampEnvelope.release = 0.32;
            patch.tremoloDepth = 0.35;
            patch.lfoRate = 5.5;
            patch.lfoRate = 5.5;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.15;
            patch.chorusRate = 5.5;
            patch.reverbMix = 0.15;
            patch.reverbSize = 0.58;
            patch.gain = 0.45;
            break;
        case 12: // Marimba
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Triangle;
            patch.transientShape = 0.45;
            patch.transientNoise = 0.22;
            patch.click = 0.1;
            patch.cutoff = 0.52;
            patch.resonance = 0.18;
            patch.ampEnvelope.attack = 0.0003;
            patch.ampEnvelope.decay = 0.22;
            patch.ampEnvelope.sustain = 0.0;
            patch.ampEnvelope.release = 0.12;
            patch.combMix = 0.08;
            patch.combFeedback = 0.2;
            patch.gain = 0.48;
            break;
        case 13: // Xylophone
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Triangle;
            patch.transientShape = 0.52;
            patch.transientNoise = 0.28;
            patch.click = 0.15;
            patch.cutoff = 0.62;
            patch.resonance = 0.28;
            patch.ampEnvelope.attack = 0.0002;
            patch.ampEnvelope.decay = 0.18;
            patch.ampEnvelope.sustain = 0.0;
            patch.ampEnvelope.release = 0.08;
            patch.combMix = 0.12;
            patch.combFeedback = 0.25;
            patch.gain = 0.42;
            break;
        case 14: // Tubular Bells
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Sine;
            patch.oscillatorC = Waveform::Triangle;
            patch.oscillatorCEnabled = true;
            patch.fmEnabled = true;
            patch.fmAmount = 0.18;
            patch.fmRatio = 3.2;
            patch.transientShape = 0.32;
            patch.click = 0.12;
            patch.cutoff = 0.55;
            patch.resonance = 0.22;
            patch.ampEnvelope.attack = 0.002;
            patch.ampEnvelope.decay = 0.55;
            patch.ampEnvelope.sustain = 0.12;
            patch.ampEnvelope.release = 0.55;
            patch.combMix = 0.15;
            patch.combFeedback = 0.18;
            patch.reverbMix = 0.18;
            patch.reverbSize = 0.68;
            patch.gain = 0.45;
            break;
        case 15: // Dulcimer
            patch.oscillatorA = Waveform::Triangle;
            patch.oscillatorB = Waveform::Square;
            patch.pulseWidth = 0.35;
            patch.transientShape = 0.28;
            patch.click = 0.1;
            patch.cutoff = 0.48;
            patch.resonance = 0.2;
            patch.ampEnvelope.attack = 0.001;
            patch.ampEnvelope.decay = 0.28;
            patch.ampEnvelope.sustain = 0.08;
            patch.ampEnvelope.release = 0.22;
            patch.combMix = 0.1;
            patch.combFeedback = 0.2;
            patch.gain = 0.46;
            break;

        // === ORGANS (16-23) ===
        case 16: // Drawbar Organ
            patch.subEnabled = true;
            patch.subOscillator = 0.28;
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Square;
            patch.oscillatorC = Waveform::Saw;
            patch.oscillatorCEnabled = true;
            patch.oscillatorCMix = 0.22;
            patch.cutoff = 0.85;
            patch.resonance = 0.05;
            patch.ampEnvelope.attack = 0.008;
            patch.ampEnvelope.decay = 0.04;
            patch.ampEnvelope.sustain = 0.92;
            patch.ampEnvelope.release = 0.04;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.22;
            patch.chorusRate = 0.42;
            patch.chorusDepth = 0.32;
            patch.chorusEnsemble = 0.35;
            patch.vibratoCents = 2.5;
            patch.lfoRate = 5.2;
            patch.tremoloDepth = 0.08;
            patch.velocityToAmp = 0.55;
            patch.reverbMix = 0.15;
            patch.reverbSize = 0.68;
            patch.analogColor = 0.62;
            patch.gain = 0.42;
            break;
        case 17: // Percussive Organ
            patch.subEnabled = true;
            patch.subOscillator = 0.22;
            patch.transientShape = 0.32;
            patch.click = 0.06;
            patch.cutoff = 0.78;
            patch.resonance = 0.08;
            patch.ampEnvelope.attack = 0.004;
            patch.ampEnvelope.decay = 0.06;
            patch.ampEnvelope.sustain = 0.85;
            patch.ampEnvelope.release = 0.06;
            patch.drive = 0.06;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.18;
            patch.vibratoCents = 1.8;
            patch.velocityToAmp = 0.68;
            patch.gain = 0.48;
            break;
        case 18: // Rock Organ
            patch.subEnabled = true;
            patch.subOscillator = 0.25;
            patch.drive = 0.06;
            patch.cutoff = 0.72;
            patch.resonance = 0.12;
            patch.filterEnvelopeAmount = 0.12;
            patch.ampEnvelope.attack = 0.006;
            patch.ampEnvelope.decay = 0.05;
            patch.ampEnvelope.sustain = 0.88;
            patch.ampEnvelope.release = 0.05;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.2;
            patch.chorusRate = 0.38;
            patch.vibratoCents = 2.2;
            patch.lfoRate = 5.8;
            patch.velocityToAmp = 0.72;
            patch.reverbMix = 0.12;
            patch.analogColor = 0.68;
            patch.gain = 0.52;
            break;
        case 19: // Church Organ
            patch.subEnabled = true;
            patch.subOscillator = 0.35;
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Square;
            patch.oscillatorC = Waveform::Saw;
            patch.oscillatorCEnabled = true;
            patch.oscillatorCMix = 0.18;
            patch.oscillatorD = Waveform::Sine;
            patch.oscillatorDEnabled = true;
            patch.oscillatorDMix = 0.12;
            patch.cutoff = 0.88;
            patch.resonance = 0.04;
            patch.ampEnvelope.attack = 0.025;
            patch.ampEnvelope.decay = 0.04;
            patch.ampEnvelope.sustain = 0.95;
            patch.ampEnvelope.release = 0.06;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.28;
            patch.chorusRate = 0.22;
            patch.chorusDepth = 0.45;
            patch.chorusEnsemble = 0.45;
            patch.unisonVoices = 3;
            patch.unisonDetuneCents = 5.0;
            patch.stereoSpread = 0.42;
            patch.reverbMix = 0.28;
            patch.reverbSize = 0.78;
            patch.reverbDamping = 0.28;
            patch.reverbDiffusion = 0.62;
            patch.velocityToAmp = 0.45;
            patch.analogColor = 0.55;
            patch.gain = 0.38;
            break;
        case 20: // Reed Organ
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Triangle;
            patch.pulseWidth = 0.42;
            patch.cutoff = 0.68;
            patch.resonance = 0.1;
            patch.ampEnvelope.attack = 0.012;
            patch.ampEnvelope.decay = 0.06;
            patch.ampEnvelope.sustain = 0.82;
            patch.ampEnvelope.release = 0.08;
            patch.vibratoCents = 2.0;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.12;
            patch.velocityToAmp = 0.62;
            patch.gain = 0.46;
            break;
        case 21: // Accordion
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Square;
            patch.pulseWidth = 0.38;
            patch.pwmDepth = 0.06;
            patch.oscillatorC = Waveform::Saw;
            patch.oscillatorCEnabled = true;
            patch.oscillatorCMix = 0.12;
            patch.cutoff = 0.62;
            patch.resonance = 0.12;
            patch.ampEnvelope.attack = 0.008;
            patch.ampEnvelope.decay = 0.05;
            patch.ampEnvelope.sustain = 0.85;
            patch.ampEnvelope.release = 0.06;
            patch.vibratoCents = 2.8;
            patch.lfoRate = 5.5;
            patch.tremoloDepth = 0.12;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.15;
            patch.velocityToAmp = 0.68;
            patch.reverbMix = 0.08;
            patch.analogColor = 0.48;
            patch.gain = 0.5;
            break;
        case 22: // Harmonica
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Triangle;
            patch.pulseWidth = 0.45;
            patch.pwmDepth = 0.08;
            patch.cutoff = 0.55;
            patch.resonance = 0.18;
            patch.filterEnvelopeAmount = 0.22;
            patch.filterKeytrack = 0.35;
            patch.ampEnvelope.attack = 0.006;
            patch.ampEnvelope.decay = 0.1;
            patch.ampEnvelope.sustain = 0.72;
            patch.ampEnvelope.release = 0.12;
            patch.vibratoCents = 3.5;
            patch.lfoRate = 4.8;
            patch.noiseEnabled = true;
            patch.noise = 0.04;
            patch.noiseTone = 0.62;
            patch.velocityToAmp = 0.78;
            patch.velocityToFilter = 0.42;
            patch.reverbMix = 0.06;
            patch.gain = 0.55;
            break;
        case 23: // Tango Accordion
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Square;
            patch.pulseWidth = 0.35;
            patch.pwmDepth = 0.08;
            patch.cutoff = 0.58;
            patch.resonance = 0.14;
            patch.ampEnvelope.attack = 0.006;
            patch.ampEnvelope.decay = 0.06;
            patch.ampEnvelope.sustain = 0.82;
            patch.ampEnvelope.release = 0.06;
            patch.vibratoCents = 3.2;
            patch.lfoRate = 5.8;
            patch.tremoloDepth = 0.15;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.12;
            patch.velocityToAmp = 0.72;
            patch.gain = 0.52;
            break;

        // === GUITARS (24-31) ===
        case 24: // Nylon String Guitar
            patch.oscillatorA = Waveform::Triangle;
            patch.oscillatorB = Waveform::Saw;
            patch.pulseWidth = 0.42;
            patch.transientShape = 0.28;
            patch.transientNoise = 0.04;
            patch.click = 0.08;
            patch.cutoff = 0.52;
            patch.resonance = 0.12;
            patch.filterEnvelopeAmount = 0.18;
            patch.filterKeytrack = 0.32;
            patch.ampEnvelope.attack = 0.004;
            patch.ampEnvelope.decay = 0.18;
            patch.ampEnvelope.sustain = 0.42;
            patch.ampEnvelope.release = 0.22;
            patch.combMix = 0.08;
            patch.combFeedback = 0.28;
            patch.velocityToAmp = 0.82;
            patch.velocityToFilter = 0.38;
            patch.reverbMix = 0.1;
            patch.analogColor = 0.48;
            patch.gain = 0.52;
            break;
        case 25: // Steel String Guitar
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Saw;
            patch.pulseWidth = 0.38;
            patch.transientShape = 0.32;
            patch.click = 0.1;
            patch.cutoff = 0.58;
            patch.resonance = 0.14;
            patch.filterEnvelopeAmount = 0.22;
            patch.ampEnvelope.attack = 0.003;
            patch.ampEnvelope.decay = 0.22;
            patch.ampEnvelope.sustain = 0.48;
            patch.ampEnvelope.release = 0.28;
            patch.combMix = 0.1;
            patch.combFeedback = 0.18;
            patch.velocityToAmp = 0.85;
            patch.reverbMix = 0.08;
            patch.analogColor = 0.52;
            patch.gain = 0.55;
            break;
        case 26: // Jazz Guitar
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Saw;
            patch.pulseWidth = 0.35;
            patch.transientShape = 0.32;
            patch.click = 0.18;
            patch.cutoff = 0.48;
            patch.resonance = 0.1;
            patch.filterEnvelopeAmount = 0.25;
            patch.filterKeytrack = 0.38;
            patch.ampEnvelope.attack = 0.004;
            patch.ampEnvelope.decay = 0.15;
            patch.ampEnvelope.sustain = 0.52;
            patch.ampEnvelope.release = 0.18;
            patch.drive = 0.05;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.12;
            patch.velocityToAmp = 0.78;
            patch.velocityToFilter = 0.45;
            patch.reverbMix = 0.08;
            patch.gain = 0.5;
            break;
        case 27: // Clean Guitar
            patch.oscillatorA = Waveform::Triangle;
            patch.oscillatorB = Waveform::Saw;
            patch.pulseWidth = 0.45;
            patch.transientShape = 0.28;
            patch.click = 0.05;
            patch.cutoff = 0.62;
            patch.resonance = 0.08;
            patch.filterEnvelopeAmount = 0.15;
            patch.ampEnvelope.attack = 0.003;
            patch.ampEnvelope.decay = 0.2;
            patch.ampEnvelope.sustain = 0.55;
            patch.ampEnvelope.release = 0.22;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.15;
            patch.chorusRate = 0.32;
            patch.chorusDepth = 0.28;
            patch.velocityToAmp = 0.72;
            patch.reverbMix = 0.1;
            patch.gain = 0.48;
            break;
        case 28: // Muted Guitar
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Triangle;
            patch.pulseWidth = 0.32;
            patch.transientShape = 0.3;
            patch.transientNoise = 0.04;
            patch.click = 0.08;
            patch.cutoff = 0.35;
            patch.resonance = 0.08;
            patch.filterEnvelopeAmount = 0.12;
            patch.ampEnvelope.attack = 0.001;
            patch.ampEnvelope.decay = 0.08;
            patch.ampEnvelope.sustain = 0.15;
            patch.ampEnvelope.release = 0.08;
            patch.highPass = 0.18;
            patch.velocityToAmp = 0.88;
            patch.gain = 0.45;
            break;
        case 29: // Overdriven Guitar
            patch.drive = 0.3;
            patch.wavefold = 0.08;
            patch.cutoff = 0.58;
            patch.resonance = 0.15;
            patch.filterEnvelopeAmount = 0.28;
            patch.ampEnvelope.attack = 0.002;
            patch.ampEnvelope.decay = 0.18;
            patch.ampEnvelope.sustain = 0.58;
            patch.ampEnvelope.release = 0.15;
            patch.highPass = 0.15;
            patch.filterNonlinearity = 0.38;
            patch.velocityToAmp = 0.88;
            patch.velocityToFilter = 0.48;
            patch.reverbMix = 0.06;
            patch.analogColor = 0.62;
            patch.gain = 0.58;
            break;
        case 30: // Distortion Guitar
            patch.drive = 0.15;
            patch.wavefold = 0.1;
            patch.cutoff = 0.52;
            patch.resonance = 0.18;
            patch.filterEnvelopeAmount = 0.32;
            patch.ampEnvelope.attack = 0.001;
            patch.ampEnvelope.decay = 0.15;
            patch.ampEnvelope.sustain = 0.62;
            patch.ampEnvelope.release = 0.12;
            patch.highPass = 0.22;
            patch.filterNonlinearity = 0.48;
            patch.bitCrushEnabled = true;
            patch.bitCrush = 0.08;
            patch.velocityToAmp = 0.92;
            patch.velocityToFilter = 0.52;
            patch.reverbMix = 0.04;
            patch.analogColor = 0.72;
            patch.gain = 0.52;
            break;
        case 31: // Guitar Harmonics
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Triangle;
            patch.oscillatorC = Waveform::Sine;
            patch.oscillatorCEnabled = true;
            patch.fmEnabled = true;
            patch.fmAmount = 0.35;
            patch.fmRatio = 3.5;
            patch.transientShape = 0.28;
            patch.click = 0.05;
            patch.cutoff = 0.72;
            patch.resonance = 0.22;
            patch.ampEnvelope.attack = 0.003;
            patch.ampEnvelope.decay = 0.28;
            patch.ampEnvelope.sustain = 0.12;
            patch.ampEnvelope.release = 0.32;
            patch.combMix = 0.12;
            patch.combFeedback = 0.22;
            patch.reverbMix = 0.15;
            patch.gain = 0.38;
            break;

        // === BASSES (32-39) ===
        case 32: // Acoustic Bass
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.subEnabled = true;
            patch.subOscillator = 0.38;
            patch.transientShape = 0.22;
            patch.transientNoise = 0.03;
            patch.click = 0.06;
            patch.cutoff = 0.22;
            patch.resonance = 0.15;
            patch.filterEnvelopeAmount = 0.18;
            patch.filterKeytrack = 0.48;
            patch.ampEnvelope.attack = 0.002;
            patch.ampEnvelope.decay = 0.12;
            patch.ampEnvelope.sustain = 0.55;
            patch.ampEnvelope.release = 0.1;
            patch.velocityToAmp = 0.82;
            patch.velocityToFilter = 0.48;
            patch.reverbMix = 0.06;
            patch.analogColor = 0.55;
            patch.gain = 0.62;
            break;
        case 33: // Electric Bass (finger)
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.subEnabled = true;
            patch.subOscillator = 0.42;
            patch.transientShape = 0.28;
            patch.click = 0.15;
            patch.cutoff = 0.18;
            patch.resonance = 0.18;
            patch.filterEnvelopeAmount = 0.25;
            patch.filterKeytrack = 0.55;
            patch.ampEnvelope.attack = 0.001;
            patch.ampEnvelope.decay = 0.1;
            patch.ampEnvelope.sustain = 0.62;
            patch.ampEnvelope.release = 0.08;
            patch.drive = 0.15;
            patch.velocityToAmp = 0.88;
            patch.velocityToFilter = 0.55;
            patch.lowPunch = 0.68;
            patch.analogColor = 0.65;
            patch.gain = 0.68;
            break;
        case 34: // Electric Bass (pick)
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.subEnabled = true;
            patch.subOscillator = 0.35;
            patch.transientShape = 0.42;
            patch.click = 0.1;
            patch.cutoff = 0.22;
            patch.resonance = 0.15;
            patch.filterEnvelopeAmount = 0.22;
            patch.ampEnvelope.attack = 0.001;
            patch.ampEnvelope.decay = 0.08;
            patch.ampEnvelope.sustain = 0.58;
            patch.ampEnvelope.release = 0.06;
            patch.drive = 0.08;
            patch.highPass = 0.12;
            patch.velocityToAmp = 0.9;
            patch.velocityToFilter = 0.52;
            patch.lowPunch = 0.72;
            patch.analogColor = 0.62;
            patch.gain = 0.65;
            break;
        case 35: // Fretless Bass
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Triangle;
            patch.subEnabled = true;
            patch.subOscillator = 0.32;
            patch.cutoff = 0.28;
            patch.resonance = 0.2;
            patch.filterEnvelopeAmount = 0.2;
            patch.filterKeytrack = 0.42;
            patch.ampEnvelope.attack = 0.004;
            patch.ampEnvelope.decay = 0.12;
            patch.ampEnvelope.sustain = 0.68;
            patch.ampEnvelope.release = 0.15;
            patch.vibratoCents = 2.2;
            patch.lfoRate = 4.2;
            patch.velocityToAmp = 0.78;
            patch.velocityToFilter = 0.42;
            patch.analogColor = 0.58;
            patch.gain = 0.58;
            break;
        case 36: // Slap Bass 1
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.subEnabled = true;
            patch.subOscillator = 0.35;
            patch.transientShape = 0.35;
            patch.transientNoise = 0.06;
            patch.click = 0.12;
            patch.cutoff = 0.25;
            patch.resonance = 0.22;
            patch.filterEnvelopeAmount = 0.32;
            patch.ampEnvelope.attack = 0.0005;
            patch.ampEnvelope.decay = 0.06;
            patch.ampEnvelope.sustain = 0.48;
            patch.ampEnvelope.release = 0.06;
            patch.drive = 0.08;
            patch.velocityToAmp = 0.92;
            patch.velocityToFilter = 0.58;
            patch.lowPunch = 0.78;
            patch.analogColor = 0.68;
            patch.gain = 0.72;
            break;
        case 37: // Slap Bass 2
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.subEnabled = true;
            patch.subOscillator = 0.38;
            patch.transientShape = 0.48;
            patch.click = 0.1;
            patch.cutoff = 0.22;
            patch.resonance = 0.25;
            patch.filterEnvelopeAmount = 0.28;
            patch.ampEnvelope.attack = 0.0005;
            patch.ampEnvelope.decay = 0.08;
            patch.ampEnvelope.sustain = 0.52;
            patch.ampEnvelope.release = 0.08;
            patch.drive = 0.12;
            patch.velocityToAmp = 0.9;
            patch.lowPunch = 0.75;
            patch.analogColor = 0.72;
            patch.gain = 0.68;
            break;
        case 38: // Synth Bass 1
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.subEnabled = true;
            patch.subOscillator = 0.48;
            patch.cutoff = 0.15;
            patch.resonance = 0.28;
            patch.filterEnvelopeAmount = 0.42;
            patch.filterKeytrack = 0.62;
            patch.ampEnvelope.attack = 0.001;
            patch.ampEnvelope.decay = 0.12;
            patch.ampEnvelope.sustain = 0.55;
            patch.ampEnvelope.release = 0.08;
            patch.drive = 0.15;
            patch.unisonVoices = 2;
            patch.unisonDetuneCents = 8.0;
            patch.filterNonlinearity = 0.42;
            patch.velocityToAmp = 0.85;
            patch.velocityToFilter = 0.62;
            patch.lowPunch = 0.82;
            patch.analogColor = 0.75;
            patch.gain = 0.72;
            break;
        case 39: // Synth Bass 2
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Saw;
            patch.subEnabled = true;
            patch.subOscillator = 0.52;
            patch.cutoff = 0.12;
            patch.resonance = 0.32;
            patch.filterEnvelopeAmount = 0.48;
            patch.filterKeytrack = 0.68;
            patch.ampEnvelope.attack = 0.001;
            patch.ampEnvelope.decay = 0.15;
            patch.ampEnvelope.sustain = 0.48;
            patch.ampEnvelope.release = 0.1;
            patch.drive = 0.08;
            patch.unisonVoices = 2;
            patch.unisonDetuneCents = 12.0;
            patch.filterNonlinearity = 0.48;
            patch.velocityToAmp = 0.88;
            patch.velocityToFilter = 0.68;
            patch.lowPunch = 0.85;
            patch.analogColor = 0.78;
            patch.gain = 0.68;
            break;

        // === SOLO STRINGS (40-47) ===
        case 40: // Violin
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Triangle;
            patch.unisonVoices = 2;
            patch.unisonDetuneCents = 4.0;
            patch.stereoSpread = 0.22;
            patch.cutoff = 0.55;
            patch.resonance = 0.12;
            patch.filterEnvelopeAmount = 0.15;
            patch.filterKeytrack = 0.32;
            patch.ampEnvelope.attack = 0.08;
            patch.ampEnvelope.decay = 0.22;
            patch.ampEnvelope.sustain = 0.75;
            patch.ampEnvelope.release = 0.38;
            patch.vibratoCents = 3.2;
            patch.lfoRate = 5.5;
            patch.lfoPanDepth = 0.06;
            patch.velocityToAmp = 0.82;
            patch.velocityToFilter = 0.38;
            patch.reverbMix = 0.12;
            patch.reverbSize = 0.62;
            patch.gain = 0.48;
            break;
        case 41: // Viola
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Triangle;
            patch.unisonVoices = 2;
            patch.unisonDetuneCents = 3.5;
            patch.cutoff = 0.48;
            patch.resonance = 0.1;
            patch.ampEnvelope.attack = 0.065;
            patch.ampEnvelope.decay = 0.2;
            patch.ampEnvelope.sustain = 0.78;
            patch.ampEnvelope.release = 0.32;
            patch.vibratoCents = 2.8;
            patch.lfoRate = 5.0;
            patch.velocityToAmp = 0.78;
            patch.reverbMix = 0.1;
            patch.gain = 0.5;
            break;
        case 42: // Cello
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Triangle;
            patch.unisonVoices = 2;
            patch.unisonDetuneCents = 3.0;
            patch.cutoff = 0.42;
            patch.resonance = 0.1;
            patch.filterEnvelopeAmount = 0.12;
            patch.ampEnvelope.attack = 0.085;
            patch.ampEnvelope.decay = 0.18;
            patch.ampEnvelope.sustain = 0.82;
            patch.ampEnvelope.release = 0.42;
            patch.vibratoCents = 2.5;
            patch.lfoRate = 4.5;
            patch.velocityToAmp = 0.75;
            patch.reverbMix = 0.12;
            patch.gain = 0.55;
            break;
        case 43: // Contrabass
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.subEnabled = true;
            patch.subOscillator = 0.28;
            patch.cutoff = 0.28;
            patch.resonance = 0.08;
            patch.ampEnvelope.attack = 0.1;
            patch.ampEnvelope.decay = 0.15;
            patch.ampEnvelope.sustain = 0.85;
            patch.ampEnvelope.release = 0.38;
            patch.velocityToAmp = 0.72;
            patch.lowPunch = 0.62;
            patch.gain = 0.62;
            break;
        case 44: // Tremolo Strings
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Triangle;
            patch.unisonVoices = 3;
            patch.unisonDetuneCents = 5.0;
            patch.stereoSpread = 0.38;
            patch.tremoloDepth = 0.38;
            patch.lfoRate = 5.5;
            patch.lfoRate = 5.5;
            patch.ampEnvelope.attack = 0.12;
            patch.ampEnvelope.decay = 0.22;
            patch.ampEnvelope.sustain = 0.72;
            patch.ampEnvelope.release = 0.48;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.18;
            patch.velocityToAmp = 0.55;
            patch.reverbMix = 0.15;
            patch.reverbSize = 0.68;
            patch.gain = 0.42;
            break;
        case 45: // Pizzicato Strings
            patch.oscillatorA = Waveform::Triangle;
            patch.oscillatorB = Waveform::Saw;
            patch.transientShape = 0.3;
            patch.transientNoise = 0.04;
            patch.click = 0.08;
            patch.cutoff = 0.52;
            patch.resonance = 0.15;
            patch.ampEnvelope.attack = 0.001;
            patch.ampEnvelope.decay = 0.08;
            patch.ampEnvelope.sustain = 0.0;
            patch.ampEnvelope.release = 0.08;
            patch.combMix = 0.08;
            patch.combFeedback = 0.28;
            patch.velocityToAmp = 0.88;
            patch.velocityToAttack = 0.48;
            patch.gain = 0.52;
            break;
        case 46: // Orchestral Harp
            patch.oscillatorA = Waveform::Triangle;
            patch.oscillatorB = Waveform::Sine;
            patch.oscillatorC = Waveform::Sine;
            patch.oscillatorCEnabled = true;
            patch.transientShape = 0.42;
            patch.click = 0.1;
            patch.cutoff = 0.58;
            patch.resonance = 0.18;
            patch.ampEnvelope.attack = 0.003;
            patch.ampEnvelope.decay = 0.42;
            patch.ampEnvelope.sustain = 0.18;
            patch.ampEnvelope.release = 0.48;
            patch.combMix = 0.12;
            patch.combFeedback = 0.25;
            patch.reverbMix = 0.18;
            patch.reverbSize = 0.72;
            patch.gain = 0.42;
            break;
        case 47: // Timpani
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Triangle;
            patch.transientShape = 0.35;
            patch.transientNoise = 0.08;
            patch.click = 0.12;
            patch.cutoff = 0.42;
            patch.resonance = 0.15;
            patch.ampEnvelope.attack = 0.002;
            patch.ampEnvelope.decay = 0.35;
            patch.ampEnvelope.sustain = 0.05;
            patch.ampEnvelope.release = 0.28;
            patch.combMix = 0.1;
            patch.combFeedback = 0.2;
            patch.velocityToAmp = 0.85;
            patch.gain = 0.58;
            break;

        // === ENSEMBLE (48-55) ===
        case 48: // String Ensemble 1
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Triangle;
            patch.oscillatorC = Waveform::Sine;
            patch.oscillatorCEnabled = true;
            patch.unisonVoices = 5;
            patch.unisonDetuneCents = 8.0;
            patch.stereoSpread = 0.55;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.32;
            patch.chorusRate = 0.18;
            patch.chorusDepth = 0.42;
            patch.chorusEnsemble = 0.38;
            patch.cutoff = 0.48;
            patch.resonance = 0.08;
            patch.ampEnvelope.attack = 0.08;
            patch.ampEnvelope.decay = 0.22;
            patch.ampEnvelope.sustain = 0.75;
            patch.ampEnvelope.release = 0.55;
            patch.velocityToAmp = 0.48;
            patch.reverbMix = 0.2;
            patch.reverbSize = 0.72;
            patch.reverbDiffusion = 0.62;
            patch.gain = 0.38;
            break;
        case 49: // String Ensemble 2
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.unisonVoices = 4;
            patch.unisonDetuneCents = 10.0;
            patch.stereoSpread = 0.62;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.38;
            patch.chorusRate = 0.22;
            patch.chorusDepth = 0.48;
            patch.chorusEnsemble = 0.42;
            patch.cutoff = 0.42;
            patch.resonance = 0.06;
            patch.ampEnvelope.attack = 0.1;
            patch.ampEnvelope.decay = 0.25;
            patch.ampEnvelope.sustain = 0.72;
            patch.ampEnvelope.release = 0.62;
            patch.velocityToAmp = 0.42;
            patch.reverbMix = 0.22;
            patch.reverbSize = 0.78;
            patch.gain = 0.35;
            break;
        case 50: // SynthStrings 1
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Triangle;
            patch.unisonVoices = 4;
            patch.unisonDetuneCents = 7.0;
            patch.stereoSpread = 0.48;
            patch.cutoff = 0.52;
            patch.resonance = 0.1;
            patch.filterEnvelopeAmount = 0.15;
            patch.lfoFilterDepth = 0.12;
            patch.lfoRate = 2.8;
            patch.ampEnvelope.attack = 0.12;
            patch.ampEnvelope.decay = 0.28;
            patch.ampEnvelope.sustain = 0.68;
            patch.ampEnvelope.release = 0.48;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.28;
            patch.velocityToAmp = 0.45;
            patch.reverbMix = 0.18;
            patch.reverbSize = 0.68;
            patch.gain = 0.4;
            break;
        case 51: // SynthStrings 2
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Saw;
            patch.unisonVoices = 3;
            patch.unisonDetuneCents = 9.0;
            patch.stereoSpread = 0.55;
            patch.cutoff = 0.45;
            patch.resonance = 0.12;
            patch.filterEnvelopeAmount = 0.18;
            patch.lfoFilterDepth = 0.15;
            patch.lfoRate = 3.2;
            patch.ampEnvelope.attack = 0.15;
            patch.ampEnvelope.decay = 0.32;
            patch.ampEnvelope.sustain = 0.65;
            patch.ampEnvelope.release = 0.55;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.32;
            patch.chorusDepth = 0.42;
            patch.velocityToAmp = 0.42;
            patch.reverbMix = 0.2;
            patch.gain = 0.38;
            break;
        case 52: // Choir Aahs
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.oscillatorD = Waveform::Sine;
            patch.oscillatorDEnabled = true;
            patch.oscillatorDMix = 0.18;
            patch.unisonVoices = 5;
            patch.unisonDetuneCents = 8.0;
            patch.stereoSpread = 0.52;
            patch.cutoff = 0.42;
            patch.resonance = 0.08;
            patch.ampEnvelope.attack = 0.18;
            patch.ampEnvelope.decay = 0.25;
            patch.ampEnvelope.sustain = 0.78;
            patch.ampEnvelope.release = 0.55;
            patch.vibratoCents = 2.2;
            patch.lfoRate = 3.8;
            patch.lfoPanDepth = 0.08;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.35;
            patch.chorusDepth = 0.48;
            patch.chorusEnsemble = 0.42;
            patch.velocityToAmp = 0.38;
            patch.reverbMix = 0.22;
            patch.reverbSize = 0.75;
            patch.reverbDiffusion = 0.65;
            patch.gain = 0.35;
            break;
        case 53: // Voice Oohs
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Triangle;
            patch.unisonVoices = 4;
            patch.unisonDetuneCents = 6.0;
            patch.stereoSpread = 0.45;
            patch.cutoff = 0.38;
            patch.resonance = 0.06;
            patch.ampEnvelope.attack = 0.15;
            patch.ampEnvelope.decay = 0.22;
            patch.ampEnvelope.sustain = 0.72;
            patch.ampEnvelope.release = 0.48;
            patch.vibratoCents = 1.8;
            patch.lfoRate = 3.5;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.28;
            patch.velocityToAmp = 0.42;
            patch.reverbMix = 0.2;
            patch.gain = 0.38;
            break;
        case 54: // Synth Voice
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.unisonVoices = 3;
            patch.unisonDetuneCents = 7.0;
            patch.stereoSpread = 0.42;
            patch.cutoff = 0.48;
            patch.resonance = 0.1;
            patch.filterEnvelopeAmount = 0.18;
            patch.ampEnvelope.attack = 0.12;
            patch.ampEnvelope.decay = 0.2;
            patch.ampEnvelope.sustain = 0.68;
            patch.ampEnvelope.release = 0.42;
            patch.vibratoCents = 2.5;
            patch.lfoRate = 4.2;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.25;
            patch.chorusDepth = 0.38;
            patch.velocityToAmp = 0.45;
            patch.reverbMix = 0.15;
            patch.gain = 0.42;
            break;
        case 55: // Orchestra Hit
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.oscillatorC = Waveform::Noise;
            patch.oscillatorCEnabled = true;
            patch.oscillatorCMix = 0.22;
            patch.transientShape = 0.35;
            patch.transientNoise = 0.08;
            patch.click = 0.12;
            patch.cutoff = 0.52;
            patch.resonance = 0.2;
            patch.filterEnvelopeAmount = 0.38;
            patch.ampEnvelope.attack = 0.001;
            patch.ampEnvelope.decay = 0.18;
            patch.ampEnvelope.sustain = 0.0;
            patch.ampEnvelope.release = 0.15;
            patch.combMix = 0.15;
            patch.combFeedback = 0.25;
            patch.reverbMix = 0.22;
            patch.reverbSize = 0.72;
            patch.velocityToAmp = 0.92;
            patch.velocityToAttack = 0.55;
            patch.gain = 0.48;
            break;

        // === BRASS (56-63) ===
        case 56: // Trumpet
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.oscillatorC = Waveform::Saw;
            patch.oscillatorCEnabled = true;
            patch.oscillatorCMix = 0.18;
            patch.cutoff = 0.38;
            patch.resonance = 0.25;
            patch.filterEnvelopeAmount = 0.38;
            patch.filterKeytrack = 0.45;
            patch.ampEnvelope.attack = 0.015;
            patch.ampEnvelope.decay = 0.15;
            patch.ampEnvelope.sustain = 0.62;
            patch.ampEnvelope.release = 0.15;
            patch.drive = 0.1;
            patch.filterNonlinearity = 0.28;
            patch.vibratoCents = 2.8;
            patch.lfoRate = 5.2;
            patch.noiseEnabled = true;
            patch.noise = 0.005;
            patch.noiseTone = 0.68;
            patch.velocityToAmp = 0.85;
            patch.velocityToFilter = 0.58;
            patch.reverbMix = 0.08;
            patch.gain = 0.55;
            break;
        case 57: // Trombone
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.cutoff = 0.32;
            patch.resonance = 0.2;
            patch.filterEnvelopeAmount = 0.32;
            patch.ampEnvelope.attack = 0.025;
            patch.ampEnvelope.decay = 0.12;
            patch.ampEnvelope.sustain = 0.72;
            patch.ampEnvelope.release = 0.18;
            patch.drive = 0.06;
            patch.vibratoCents = 2.2;
            patch.lfoRate = 4.5;
            patch.velocityToAmp = 0.82;
            patch.gain = 0.58;
            break;
        case 58: // Tuba
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.subEnabled = true;
            patch.subOscillator = 0.28;
            patch.cutoff = 0.22;
            patch.resonance = 0.12;
            patch.filterEnvelopeAmount = 0.18;
            patch.ampEnvelope.attack = 0.04;
            patch.ampEnvelope.decay = 0.1;
            patch.ampEnvelope.sustain = 0.82;
            patch.ampEnvelope.release = 0.2;
            patch.velocityToAmp = 0.78;
            patch.lowPunch = 0.58;
            patch.gain = 0.62;
            break;
        case 59: // Muted Trumpet
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Triangle;
            patch.pulseWidth = 0.42;
            patch.cutoff = 0.32;
            patch.resonance = 0.18;
            patch.filterEnvelopeAmount = 0.22;
            patch.ampEnvelope.attack = 0.012;
            patch.ampEnvelope.decay = 0.12;
            patch.ampEnvelope.sustain = 0.55;
            patch.ampEnvelope.release = 0.12;
            patch.highPass = 0.18;
            patch.velocityToAmp = 0.82;
            patch.gain = 0.48;
            break;
        case 60: // French Horn
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Triangle;
            patch.unisonVoices = 2;
            patch.unisonDetuneCents = 3.0;
            patch.cutoff = 0.35;
            patch.resonance = 0.15;
            patch.filterEnvelopeAmount = 0.22;
            patch.ampEnvelope.attack = 0.055;
            patch.ampEnvelope.decay = 0.14;
            patch.ampEnvelope.sustain = 0.78;
            patch.ampEnvelope.release = 0.22;
            patch.vibratoCents = 1.8;
            patch.lfoRate = 4.0;
            patch.velocityToAmp = 0.72;
            patch.reverbMix = 0.1;
            patch.gain = 0.52;
            break;
        case 61: // Brass Section
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.oscillatorC = Waveform::Saw;
            patch.oscillatorCEnabled = true;
            patch.unisonVoices = 3;
            patch.unisonDetuneCents = 5.0;
            patch.stereoSpread = 0.38;
            patch.cutoff = 0.35;
            patch.resonance = 0.18;
            patch.filterEnvelopeAmount = 0.32;
            patch.ampEnvelope.attack = 0.025;
            patch.ampEnvelope.decay = 0.15;
            patch.ampEnvelope.sustain = 0.72;
            patch.ampEnvelope.release = 0.2;
            patch.drive = 0.08;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.15;
            patch.velocityToAmp = 0.68;
            patch.reverbMix = 0.12;
            patch.reverbSize = 0.62;
            patch.gain = 0.48;
            break;
        case 62: // Synth Brass 1
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Saw;
            patch.unisonVoices = 3;
            patch.unisonDetuneCents = 8.0;
            patch.cutoff = 0.28;
            patch.resonance = 0.25;
            patch.filterEnvelopeAmount = 0.42;
            patch.filterKeytrack = 0.48;
            patch.ampEnvelope.attack = 0.015;
            patch.ampEnvelope.decay = 0.12;
            patch.ampEnvelope.sustain = 0.62;
            patch.ampEnvelope.release = 0.15;
            patch.drive = 0.12;
            patch.filterNonlinearity = 0.32;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.18;
            patch.velocityToAmp = 0.72;
            patch.velocityToFilter = 0.55;
            patch.reverbMix = 0.1;
            patch.analogColor = 0.68;
            patch.gain = 0.52;
            break;
        case 63: // Synth Brass 2
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.unisonVoices = 4;
            patch.unisonDetuneCents = 10.0;
            patch.cutoff = 0.25;
            patch.resonance = 0.28;
            patch.filterEnvelopeAmount = 0.48;
            patch.ampEnvelope.attack = 0.02;
            patch.ampEnvelope.decay = 0.15;
            patch.ampEnvelope.sustain = 0.58;
            patch.ampEnvelope.release = 0.18;
            patch.drive = 0.14;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.22;
            patch.chorusDepth = 0.35;
            patch.velocityToAmp = 0.68;
            patch.velocityToFilter = 0.58;
            patch.analogColor = 0.72;
            patch.gain = 0.5;
            break;

        // === REEDS (64-71) ===
        case 64: // Soprano Sax
        case 65: // Alto Sax
        case 66: // Tenor Sax
        case 67: // Baritone Sax
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Triangle;
            patch.pulseWidth = 0.42;
            patch.pwmDepth = 0.06;
            patch.cutoff = 0.58;
            patch.resonance = 0.18;
            patch.filterEnvelopeAmount = 0.22;
            patch.filterKeytrack = 0.35;
            patch.ampEnvelope.attack = 0.035;
            patch.ampEnvelope.decay = 0.12;
            patch.ampEnvelope.sustain = 0.68;
            patch.ampEnvelope.release = 0.12;
            patch.vibratoCents = 3.2;
            patch.lfoRate = 4.8;
            patch.noiseEnabled = true;
            patch.noise = 0.005;
            patch.noiseTone = 0.62;
            patch.velocityToAmp = 0.82;
            patch.velocityToFilter = 0.48;
            patch.reverbMix = 0.08;
            patch.gain = 0.52;
            break;
        case 68: // Oboe
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Triangle;
            patch.pulseWidth = 0.45;
            patch.cutoff = 0.52;
            patch.resonance = 0.15;
            patch.filterEnvelopeAmount = 0.18;
            patch.ampEnvelope.attack = 0.045;
            patch.ampEnvelope.decay = 0.1;
            patch.ampEnvelope.sustain = 0.72;
            patch.ampEnvelope.release = 0.12;
            patch.vibratoCents = 2.5;
            patch.lfoRate = 4.5;
            patch.velocityToAmp = 0.78;
            patch.gain = 0.48;
            break;
        case 69: // English Horn
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Triangle;
            patch.cutoff = 0.45;
            patch.resonance = 0.12;
            patch.ampEnvelope.attack = 0.055;
            patch.ampEnvelope.decay = 0.1;
            patch.ampEnvelope.sustain = 0.75;
            patch.ampEnvelope.release = 0.15;
            patch.vibratoCents = 2.2;
            patch.lfoRate = 4.2;
            patch.velocityToAmp = 0.75;
            patch.gain = 0.5;
            break;
        case 70: // Bassoon
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Saw;
            patch.cutoff = 0.32;
            patch.resonance = 0.1;
            patch.ampEnvelope.attack = 0.065;
            patch.ampEnvelope.decay = 0.12;
            patch.ampEnvelope.sustain = 0.78;
            patch.ampEnvelope.release = 0.18;
            patch.velocityToAmp = 0.72;
            patch.gain = 0.55;
            break;
        case 71: // Clarinet
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Sine;
            patch.pulseWidth = 0.48;
            patch.cutoff = 0.55;
            patch.resonance = 0.12;
            patch.filterEnvelopeAmount = 0.15;
            patch.ampEnvelope.attack = 0.04;
            patch.ampEnvelope.decay = 0.1;
            patch.ampEnvelope.sustain = 0.72;
            patch.ampEnvelope.release = 0.1;
            patch.vibratoCents = 2.0;
            patch.lfoRate = 4.0;
            patch.velocityToAmp = 0.78;
            patch.gain = 0.5;
            break;

        // === PIPES (72-79) ===
        case 72: // Piccolo
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Triangle;
            patch.oscillatorBEnabled = true;
            patch.oscillatorMix = 0.22;
            patch.cutoff = 0.78;
            patch.resonance = 0.1;
            patch.ampEnvelope.attack = 0.025;
            patch.ampEnvelope.decay = 0.08;
            patch.ampEnvelope.sustain = 0.68;
            patch.ampEnvelope.release = 0.12;
            patch.vibratoCents = 2.8;
            patch.lfoRate = 5.5;
            patch.noiseEnabled = false;
            patch.velocityToAmp = 0.72;
            patch.gain = 0.42;
            break;
        case 73: // Flute
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Triangle;
            patch.oscillatorBEnabled = true;
            patch.oscillatorMix = 0.18;
            patch.cutoff = 0.72;
            patch.resonance = 0.08;
            patch.ampEnvelope.attack = 0.035;
            patch.ampEnvelope.decay = 0.1;
            patch.ampEnvelope.sustain = 0.72;
            patch.ampEnvelope.release = 0.15;
            patch.vibratoCents = 2.2;
            patch.lfoRate = 4.8;
            patch.noiseEnabled = true;
            patch.noise = 0.005;
            patch.noiseTone = 0.72;
            patch.velocityToAmp = 0.68;
            patch.reverbMix = 0.12;
            patch.reverbSize = 0.62;
            patch.gain = 0.45;
            break;
        case 74: // Recorder
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Triangle;
            patch.oscillatorBEnabled = true;
            patch.oscillatorMix = 0.15;
            patch.cutoff = 0.65;
            patch.resonance = 0.06;
            patch.ampEnvelope.attack = 0.025;
            patch.ampEnvelope.decay = 0.08;
            patch.ampEnvelope.sustain = 0.68;
            patch.ampEnvelope.release = 0.1;
            patch.vibratoCents = 1.8;
            patch.lfoRate = 4.2;
            patch.velocityToAmp = 0.65;
            patch.gain = 0.48;
            break;
        case 75: // Pan Flute
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Sine;
            patch.cutoff = 0.58;
            patch.resonance = 0.05;
            patch.ampEnvelope.attack = 0.045;
            patch.ampEnvelope.decay = 0.08;
            patch.ampEnvelope.sustain = 0.65;
            patch.ampEnvelope.release = 0.12;
            patch.vibratoCents = 2.5;
            patch.lfoRate = 3.8;
            patch.noiseEnabled = false;
            patch.velocityToAmp = 0.62;
            patch.reverbMix = 0.15;
            patch.reverbSize = 0.68;
            patch.gain = 0.42;
            break;
        case 76: // Blown Bottle
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Triangle;
            patch.oscillatorBEnabled = true;
            patch.oscillatorMix = 0.12;
            patch.fmEnabled = true;
            patch.fmAmount = 0.12;
            patch.fmRatio = 2.5;
            patch.cutoff = 0.52;
            patch.resonance = 0.08;
            patch.ampEnvelope.attack = 0.035;
            patch.ampEnvelope.decay = 0.08;
            patch.ampEnvelope.sustain = 0.62;
            patch.ampEnvelope.release = 0.1;
            patch.vibratoCents = 1.5;
            patch.lfoRate = 3.5;
            patch.velocityToAmp = 0.58;
            patch.gain = 0.45;
            break;
        case 77: // Shakuhachi
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Triangle;
            patch.oscillatorBEnabled = true;
            patch.oscillatorMix = 0.22;
            patch.cutoff = 0.48;
            patch.resonance = 0.12;
            patch.ampEnvelope.attack = 0.065;
            patch.ampEnvelope.decay = 0.12;
            patch.ampEnvelope.sustain = 0.68;
            patch.ampEnvelope.release = 0.18;
            patch.vibratoCents = 3.5;
            patch.lfoRate = 3.2;
            patch.noiseEnabled = true;
            patch.noise = 0.008;
            patch.noiseTone = 0.68;
            patch.velocityToAmp = 0.68;
            patch.reverbMix = 0.18;
            patch.reverbSize = 0.72;
            patch.gain = 0.42;
            break;
        case 78: // Whistle
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Sine;
            patch.cutoff = 0.55;
            patch.resonance = 0.06;
            patch.ampEnvelope.attack = 0.015;
            patch.ampEnvelope.decay = 0.06;
            patch.ampEnvelope.sustain = 0.62;
            patch.ampEnvelope.release = 0.08;
            patch.vibratoCents = 1.2;
            patch.lfoRate = 4.0;
            patch.velocityToAmp = 0.55;
            patch.gain = 0.48;
            break;
        case 79: // Ocarina
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Triangle;
            patch.oscillatorBEnabled = true;
            patch.oscillatorMix = 0.1;
            patch.cutoff = 0.48;
            patch.resonance = 0.08;
            patch.ampEnvelope.attack = 0.025;
            patch.ampEnvelope.decay = 0.08;
            patch.ampEnvelope.sustain = 0.65;
            patch.ampEnvelope.release = 0.1;
            patch.vibratoCents = 1.8;
            patch.lfoRate = 3.8;
            patch.velocityToAmp = 0.62;
            patch.gain = 0.45;
            break;

        // === SYNTH LEADS (80-87) ===
        case 80: // Lead 1 (square)
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Square;
            patch.pulseWidth = 0.43;
            patch.pwmDepth = 0.08;
            patch.cutoff = 0.62;
            patch.resonance = 0.15;
            patch.filterEnvelopeAmount = 0.28;
            patch.vibratoCents = 5.5;
            patch.lfoRate = 5.8;
            patch.unisonVoices = 2;
            patch.unisonDetuneCents = 6.0;
            patch.ampEnvelope.attack = 0.002;
            patch.ampEnvelope.decay = 0.08;
            patch.ampEnvelope.sustain = 0.58;
            patch.ampEnvelope.release = 0.1;
            patch.drive = 0.18;
            patch.velocityToAmp = 0.72;
            patch.velocityToFilter = 0.42;
            patch.reverbMix = 0.06;
            patch.gain = 0.48;
            break;
        case 81: // Lead 2 (sawtooth)
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Saw;
            patch.cutoff = 0.58;
            patch.resonance = 0.18;
            patch.filterEnvelopeAmount = 0.32;
            patch.unisonVoices = 3;
            patch.unisonDetuneCents = 8.0;
            patch.ampEnvelope.attack = 0.002;
            patch.ampEnvelope.decay = 0.08;
            patch.ampEnvelope.sustain = 0.55;
            patch.ampEnvelope.release = 0.08;
            patch.drive = 0.1;
            patch.velocityToAmp = 0.75;
            patch.velocityToFilter = 0.48;
            patch.reverbMix = 0.05;
            patch.gain = 0.52;
            break;
        case 82: // Lead 3 (calliope)
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Triangle;
            patch.fmEnabled = true;
            patch.fmAmount = 0.22;
            patch.fmRatio = 2.5;
            patch.cutoff = 0.68;
            patch.resonance = 0.12;
            patch.ampEnvelope.attack = 0.003;
            patch.ampEnvelope.decay = 0.1;
            patch.ampEnvelope.sustain = 0.52;
            patch.ampEnvelope.release = 0.12;
            patch.vibratoCents = 4.2;
            patch.lfoRate = 5.2;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.18;
            patch.gain = 0.45;
            break;
        case 83: // Lead 4 (chiff)
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.transientShape = 0.28;
            patch.transientNoise = 0.05;
            patch.click = 0.08;
            patch.cutoff = 0.55;
            patch.resonance = 0.2;
            patch.filterEnvelopeAmount = 0.35;
            patch.ampEnvelope.attack = 0.002;
            patch.ampEnvelope.decay = 0.08;
            patch.ampEnvelope.sustain = 0.48;
            patch.ampEnvelope.release = 0.1;
            patch.velocityToAmp = 0.78;
            patch.velocityToFilter = 0.48;
            patch.gain = 0.5;
            break;
        case 84: // Lead 5 (charang)
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.pulseWidth = 0.38;
            patch.pwmDepth = 0.12;
            patch.cutoff = 0.52;
            patch.resonance = 0.18;
            patch.filterEnvelopeAmount = 0.3;
            patch.ampEnvelope.attack = 0.002;
            patch.ampEnvelope.decay = 0.08;
            patch.ampEnvelope.sustain = 0.52;
            patch.ampEnvelope.release = 0.1;
            patch.drive = 0.08;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.15;
            patch.gain = 0.48;
            break;
        case 85: // Lead 6 (voice)
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Triangle;
            patch.unisonVoices = 2;
            patch.unisonDetuneCents = 5.0;
            patch.cutoff = 0.48;
            patch.resonance = 0.12;
            patch.ampEnvelope.attack = 0.035;
            patch.ampEnvelope.decay = 0.1;
            patch.ampEnvelope.sustain = 0.62;
            patch.ampEnvelope.release = 0.15;
            patch.vibratoCents = 3.2;
            patch.lfoRate = 4.5;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.22;
            patch.chorusDepth = 0.35;
            patch.gain = 0.45;
            break;
        case 86: // Lead 7 (fifths)
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Saw;
            patch.detuneCCents = 7.02; // Perfect fifth
            patch.cutoff = 0.55;
            patch.resonance = 0.15;
            patch.filterEnvelopeAmount = 0.28;
            patch.ampEnvelope.attack = 0.002;
            patch.ampEnvelope.decay = 0.08;
            patch.ampEnvelope.sustain = 0.55;
            patch.ampEnvelope.release = 0.08;
            patch.drive = 0.08;
            patch.unisonVoices = 2;
            patch.unisonDetuneCents = 6.0;
            patch.gain = 0.5;
            break;
        case 87: // Lead 8 (bass + lead)
            patch.subEnabled = true;
            patch.subOscillator = 0.42;
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.cutoff = 0.48;
            patch.resonance = 0.18;
            patch.filterEnvelopeAmount = 0.38;
            patch.filterKeytrack = 0.45;
            patch.ampEnvelope.attack = 0.001;
            patch.ampEnvelope.decay = 0.1;
            patch.ampEnvelope.sustain = 0.58;
            patch.ampEnvelope.release = 0.08;
            patch.drive = 0.08;
            patch.velocityToAmp = 0.78;
            patch.velocityToFilter = 0.52;
            patch.lowPunch = 0.62;
            patch.gain = 0.55;
            break;

        // === PADS (88-95) ===
        case 88: // Pad 1 (new age)
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Triangle;
            patch.oscillatorC = Waveform::Sine;
            patch.oscillatorCEnabled = true;
            patch.unisonVoices = 4;
            patch.unisonDetuneCents = 6.0;
            patch.stereoSpread = 0.48;
            patch.cutoff = 0.55;
            patch.resonance = 0.08;
            patch.ampEnvelope.attack = 0.18;
            patch.ampEnvelope.decay = 0.32;
            patch.ampEnvelope.sustain = 0.72;
            patch.ampEnvelope.release = 0.72;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.32;
            patch.chorusRate = 0.12;
            patch.chorusDepth = 0.42;
            patch.chorusEnsemble = 0.35;
            patch.lfoFilterDepth = 0.12;
            patch.lfoRate = 1.8;
            patch.velocityToAmp = 0.42;
            patch.reverbMix = 0.25;
            patch.reverbSize = 0.78;
            patch.reverbDiffusion = 0.65;
            patch.gain = 0.32;
            break;
        case 89: // Pad 2 (warm)
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Triangle;
            patch.unisonVoices = 4;
            patch.unisonDetuneCents = 7.0;
            patch.stereoSpread = 0.55;
            patch.cutoff = 0.42;
            patch.resonance = 0.1;
            patch.ampEnvelope.attack = 0.22;
            patch.ampEnvelope.decay = 0.38;
            patch.ampEnvelope.sustain = 0.75;
            patch.ampEnvelope.release = 0.82;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.38;
            patch.chorusRate = 0.15;
            patch.chorusDepth = 0.48;
            patch.chorusEnsemble = 0.42;
            patch.lfoFilterDepth = 0.15;
            patch.lfoRate = 2.2;
            patch.velocityToAmp = 0.38;
            patch.reverbMix = 0.28;
            patch.reverbSize = 0.82;
            patch.gain = 0.3;
            break;
        case 90: // Pad 3 (polysynth)
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.unisonVoices = 3;
            patch.unisonDetuneCents = 8.0;
            patch.stereoSpread = 0.48;
            patch.cutoff = 0.48;
            patch.resonance = 0.12;
            patch.filterEnvelopeAmount = 0.2;
            patch.ampEnvelope.attack = 0.12;
            patch.ampEnvelope.decay = 0.28;
            patch.ampEnvelope.sustain = 0.68;
            patch.ampEnvelope.release = 0.55;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.32;
            patch.chorusDepth = 0.42;
            patch.lfoFilterDepth = 0.18;
            patch.lfoRate = 2.8;
            patch.velocityToAmp = 0.45;
            patch.reverbMix = 0.2;
            patch.gain = 0.38;
            break;
        case 91: // Pad 4 (choir)
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Triangle;
            patch.oscillatorD = Waveform::Sine;
            patch.oscillatorDEnabled = true;
            patch.unisonVoices = 5;
            patch.unisonDetuneCents = 7.0;
            patch.stereoSpread = 0.52;
            patch.cutoff = 0.4;
            patch.resonance = 0.06;
            patch.ampEnvelope.attack = 0.28;
            patch.ampEnvelope.decay = 0.32;
            patch.ampEnvelope.sustain = 0.78;
            patch.ampEnvelope.release = 0.68;
            patch.vibratoCents = 2.0;
            patch.lfoRate = 2.5;
            patch.lfoPanDepth = 0.08;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.38;
            patch.chorusDepth = 0.52;
            patch.chorusEnsemble = 0.45;
            patch.velocityToAmp = 0.35;
            patch.reverbMix = 0.25;
            patch.reverbSize = 0.78;
            patch.gain = 0.28;
            break;
        case 92: // Pad 5 (bowed)
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Triangle;
            patch.unisonVoices = 3;
            patch.unisonDetuneCents = 5.0;
            patch.cutoff = 0.38;
            patch.resonance = 0.1;
            patch.ampEnvelope.attack = 0.35;
            patch.ampEnvelope.decay = 0.28;
            patch.ampEnvelope.sustain = 0.72;
            patch.ampEnvelope.release = 0.55;
            patch.vibratoCents = 1.5;
            patch.lfoRate = 2.2;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.25;
            patch.velocityToAmp = 0.42;
            patch.reverbMix = 0.22;
            patch.gain = 0.35;
            break;
        case 93: // Pad 6 (metallic)
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.fmEnabled = true;
            patch.fmAmount = 0.18;
            patch.fmRatio = 3.2;
            patch.unisonVoices = 3;
            patch.unisonDetuneCents = 6.0;
            patch.cutoff = 0.52;
            patch.resonance = 0.15;
            patch.ampEnvelope.attack = 0.15;
            patch.ampEnvelope.decay = 0.32;
            patch.ampEnvelope.sustain = 0.65;
            patch.ampEnvelope.release = 0.48;
            patch.combMix = 0.12;
            patch.combFeedback = 0.22;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.22;
            patch.velocityToAmp = 0.4;
            patch.reverbMix = 0.2;
            patch.gain = 0.38;
            break;
        case 94: // Pad 7 (halo)
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Triangle;
            patch.oscillatorC = Waveform::Sine;
            patch.oscillatorCEnabled = true;
            patch.unisonVoices = 4;
            patch.unisonDetuneCents = 5.0;
            patch.stereoSpread = 0.48;
            patch.cutoff = 0.58;
            patch.resonance = 0.08;
            patch.ampEnvelope.attack = 0.25;
            patch.ampEnvelope.decay = 0.38;
            patch.ampEnvelope.sustain = 0.72;
            patch.ampEnvelope.release = 0.78;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.35;
            patch.chorusRate = 0.08;
            patch.chorusDepth = 0.48;
            patch.lfoFilterDepth = 0.1;
            patch.lfoRate = 1.5;
            patch.velocityToAmp = 0.32;
            patch.reverbMix = 0.28;
            patch.reverbSize = 0.82;
            patch.gain = 0.28;
            break;
        case 95: // Pad 8 (sweep)
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Triangle;
            patch.unisonVoices = 3;
            patch.unisonDetuneCents = 6.0;
            patch.cutoff = 0.35;
            patch.resonance = 0.12;
            patch.filterEnvelopeAmount = 0.32;
            patch.lfoFilterDepth = 0.22;
            patch.lfoRate = 0.5;
            patch.ampEnvelope.attack = 0.32;
            patch.ampEnvelope.decay = 0.42;
            patch.ampEnvelope.sustain = 0.68;
            patch.ampEnvelope.release = 0.62;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.28;
            patch.chorusDepth = 0.42;
            patch.velocityToAmp = 0.38;
            patch.reverbMix = 0.22;
            patch.gain = 0.32;
            break;

        // === FX (96-103) ===
        case 96: // FX 1 (rain)
            patch.oscillatorA = Waveform::Noise;
            patch.oscillatorB = Waveform::Sine;
            patch.oscillatorC = Waveform::Noise;
            patch.oscillatorCEnabled = true;
            patch.oscillatorCMix = 0.35;
            patch.cutoff = 0.72;
            patch.resonance = 0.15;
            patch.ampEnvelope.attack = 0.025;
            patch.ampEnvelope.decay = 0.38;
            patch.ampEnvelope.sustain = 0.0;
            patch.ampEnvelope.release = 0.32;
            patch.combMix = 0.28;
            patch.combFeedback = 0.25;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.32;
            patch.reverbMix = 0.25;
            patch.reverbSize = 0.72;
            patch.velocityToAmp = 0.45;
            patch.gain = 0.35;
            break;
        case 97: // FX 2 (soundtrack)
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Triangle;
            patch.oscillatorC = Waveform::Sine;
            patch.oscillatorCEnabled = true;
            patch.unisonVoices = 4;
            patch.unisonDetuneCents = 8.0;
            patch.cutoff = 0.42;
            patch.resonance = 0.1;
            patch.filterEnvelopeAmount = 0.28;
            patch.lfoFilterDepth = 0.18;
            patch.lfoRate = 0.35;
            patch.ampEnvelope.attack = 0.42;
            patch.ampEnvelope.decay = 0.55;
            patch.ampEnvelope.sustain = 0.62;
            patch.ampEnvelope.release = 0.82;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.35;
            patch.chorusDepth = 0.48;
            patch.reverbMix = 0.32;
            patch.reverbSize = 0.85;
            patch.velocityToAmp = 0.35;
            patch.gain = 0.3;
            break;
        case 98: // FX 3 (crystal)
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Sine;
            patch.fmEnabled = true;
            patch.fmAmount = 0.32;
            patch.fmRatio = 4.0;
            patch.fmFeedback = 0.22;
            patch.ampEnvelope.attack = 0.015;
            patch.ampEnvelope.decay = 0.42;
            patch.ampEnvelope.sustain = 0.12;
            patch.ampEnvelope.release = 0.48;
            patch.combMix = 0.22;
            patch.combFeedback = 0.28;
            patch.reverbMix = 0.28;
            patch.reverbSize = 0.78;
            patch.velocityToAmp = 0.42;
            patch.gain = 0.32;
            break;
        case 99: // FX 4 (atmosphere)
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Noise;
            patch.oscillatorC = Waveform::Sine;
            patch.oscillatorCEnabled = true;
            patch.unisonVoices = 3;
            patch.unisonDetuneCents = 6.0;
            patch.cutoff = 0.38;
            patch.resonance = 0.08;
            patch.ampEnvelope.attack = 0.55;
            patch.ampEnvelope.decay = 0.62;
            patch.ampEnvelope.sustain = 0.55;
            patch.ampEnvelope.release = 0.92;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.28;
            patch.reverbMix = 0.35;
            patch.reverbSize = 0.88;
            patch.velocityToAmp = 0.3;
            patch.gain = 0.28;
            break;
        case 100: // FX 5 (brightness)
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Saw;
            patch.oscillatorC = Waveform::Noise;
            patch.oscillatorCEnabled = true;
            patch.cutoff = 0.72;
            patch.resonance = 0.22;
            patch.filterEnvelopeAmount = 0.35;
            patch.ampEnvelope.attack = 0.015;
            patch.ampEnvelope.decay = 0.28;
            patch.ampEnvelope.sustain = 0.42;
            patch.ampEnvelope.release = 0.38;
            patch.drive = 0.08;
            patch.combMix = 0.15;
            patch.combFeedback = 0.2;
            patch.reverbMix = 0.18;
            patch.gain = 0.42;
            break;
        case 101: // FX 6 (goblins)
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.fmEnabled = true;
            patch.fmAmount = 0.42;
            patch.fmRatio = 2.2;
            patch.fmFeedback = 0.28;
            patch.cutoff = 0.32;
            patch.resonance = 0.18;
            patch.ampEnvelope.attack = 0.35;
            patch.ampEnvelope.decay = 0.48;
            patch.ampEnvelope.sustain = 0.48;
            patch.ampEnvelope.release = 0.62;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.22;
            patch.reverbMix = 0.25;
            patch.reverbSize = 0.78;
            patch.velocityToAmp = 0.38;
            patch.gain = 0.35;
            break;
        case 102: // FX 7 (echoes)
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Triangle;
            patch.oscillatorC = Waveform::Sine;
            patch.oscillatorCEnabled = true;
            patch.cutoff = 0.48;
            patch.resonance = 0.1;
            patch.ampEnvelope.attack = 0.08;
            patch.ampEnvelope.decay = 0.55;
            patch.ampEnvelope.sustain = 0.32;
            patch.ampEnvelope.release = 0.72;
            patch.combMix = 0.18;
            patch.combFeedback = 0.25;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.25;
            patch.reverbMix = 0.32;
            patch.reverbSize = 0.82;
            patch.velocityToAmp = 0.35;
            patch.gain = 0.32;
            break;
        case 103: // FX 8 (sci-fi)
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Square;
            patch.oscillatorC = Waveform::Noise;
            patch.oscillatorCEnabled = true;
            patch.fmEnabled = true;
            patch.fmAmount = 0.48;
            patch.fmRatio = 3.5;
            patch.fmFeedback = 0.35;
            patch.ringEnabled = true;
            patch.ringMod = 0.45;
            patch.cutoff = 0.58;
            patch.resonance = 0.2;
            patch.ampEnvelope.attack = 0.015;
            patch.ampEnvelope.decay = 0.32;
            patch.ampEnvelope.sustain = 0.38;
            patch.ampEnvelope.release = 0.42;
            patch.bitCrushEnabled = true;
            patch.bitCrush = 0.12;
            patch.sampleRateReduction = 0.15;
            patch.reverbMix = 0.2;
            patch.gain = 0.4;
            break;

        // === ETHNIC (104-111) ===
        case 104: // Sitar
            patch.oscillatorA = Waveform::Triangle;
            patch.oscillatorB = Waveform::Saw;
            patch.pulseWidth = 0.32;
            patch.transientShape = 0.28;
            patch.transientNoise = 0.04;
            patch.click = 0.08;
            patch.cutoff = 0.42;
            patch.resonance = 0.2;
            patch.filterEnvelopeAmount = 0.22;
            patch.ampEnvelope.attack = 0.003;
            patch.ampEnvelope.decay = 0.15;
            patch.ampEnvelope.sustain = 0.42;
            patch.ampEnvelope.release = 0.18;
            patch.combMix = 0.15;
            patch.combFeedback = 0.25;
            patch.velocityToAmp = 0.82;
            patch.reverbMix = 0.12;
            patch.gain = 0.48;
            break;
        case 105: // Banjo
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Saw;
            patch.pulseWidth = 0.35;
            patch.transientShape = 0.52;
            patch.click = 0.12;
            patch.cutoff = 0.48;
            patch.resonance = 0.15;
            patch.ampEnvelope.attack = 0.001;
            patch.ampEnvelope.decay = 0.12;
            patch.ampEnvelope.sustain = 0.18;
            patch.ampEnvelope.release = 0.12;
            patch.combMix = 0.1;
            patch.combFeedback = 0.18;
            patch.velocityToAmp = 0.85;
            patch.gain = 0.52;
            break;
        case 106: // Shamisen
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Triangle;
            patch.pulseWidth = 0.38;
            patch.transientShape = 0.42;
            patch.click = 0.08;
            patch.cutoff = 0.45;
            patch.resonance = 0.12;
            patch.ampEnvelope.attack = 0.002;
            patch.ampEnvelope.decay = 0.1;
            patch.ampEnvelope.sustain = 0.28;
            patch.ampEnvelope.release = 0.1;
            patch.velocityToAmp = 0.82;
            patch.gain = 0.5;
            break;
        case 107: // Koto
            patch.oscillatorA = Waveform::Triangle;
            patch.oscillatorB = Waveform::Sine;
            patch.transientShape = 0.45;
            patch.click = 0.1;
            patch.cutoff = 0.52;
            patch.resonance = 0.15;
            patch.ampEnvelope.attack = 0.002;
            patch.ampEnvelope.decay = 0.18;
            patch.ampEnvelope.sustain = 0.22;
            patch.ampEnvelope.release = 0.18;
            patch.combMix = 0.12;
            patch.combFeedback = 0.22;
            patch.velocityToAmp = 0.78;
            patch.reverbMix = 0.1;
            patch.gain = 0.48;
            break;
        case 108: // Kalimba
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Triangle;
            patch.transientShape = 0.55;
            patch.click = 0.15;
            patch.cutoff = 0.62;
            patch.resonance = 0.22;
            patch.ampEnvelope.attack = 0.001;
            patch.ampEnvelope.decay = 0.28;
            patch.ampEnvelope.sustain = 0.0;
            patch.ampEnvelope.release = 0.22;
            patch.combMix = 0.15;
            patch.combFeedback = 0.28;
            patch.velocityToAmp = 0.88;
            patch.gain = 0.42;
            break;
        case 109: // Bagpipe
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Saw;
            patch.pulseWidth = 0.42;
            patch.cutoff = 0.38;
            patch.resonance = 0.18;
            patch.ampEnvelope.attack = 0.08;
            patch.ampEnvelope.decay = 0.08;
            patch.ampEnvelope.sustain = 0.88;
            patch.ampEnvelope.release = 0.12;
            patch.vibratoCents = 3.5;
            patch.lfoRate = 4.5;
            patch.noiseEnabled = true;
            patch.noise = 0.008;
            patch.noiseTone = 0.58;
            patch.velocityToAmp = 0.72;
            patch.reverbMix = 0.12;
            patch.gain = 0.52;
            break;
        case 110: // Fiddle
            patch.oscillatorA = Waveform::Saw;
            patch.oscillatorB = Waveform::Triangle;
            patch.unisonVoices = 2;
            patch.unisonDetuneCents = 4.0;
            patch.cutoff = 0.48;
            patch.resonance = 0.12;
            patch.ampEnvelope.attack = 0.015;
            patch.ampEnvelope.decay = 0.12;
            patch.ampEnvelope.sustain = 0.68;
            patch.ampEnvelope.release = 0.15;
            patch.vibratoCents = 3.8;
            patch.lfoRate = 5.5;
            patch.velocityToAmp = 0.85;
            patch.gain = 0.55;
            break;
        case 111: // Shanai
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Saw;
            patch.pulseWidth = 0.38;
            patch.cutoff = 0.42;
            patch.resonance = 0.22;
            patch.filterEnvelopeAmount = 0.28;
            patch.ampEnvelope.attack = 0.025;
            patch.ampEnvelope.decay = 0.12;
            patch.ampEnvelope.sustain = 0.72;
            patch.ampEnvelope.release = 0.15;
            patch.vibratoCents = 4.2;
            patch.lfoRate = 5.2;
            patch.noiseEnabled = true;
            patch.noise = 0.005;
            patch.noiseTone = 0.62;
            patch.velocityToAmp = 0.82;
            patch.gain = 0.5;
            break;

        // === PERCUSSION (112-119) ===
        case 112: // Tinkle Bell
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Sine;
            patch.fmEnabled = true;
            patch.fmAmount = 0.28;
            patch.fmRatio = 5.5;
            patch.transientShape = 0.55;
            patch.click = 0.15;
            patch.cutoff = 0.78;
            patch.resonance = 0.28;
            patch.ampEnvelope.attack = 0.0003;
            patch.ampEnvelope.decay = 0.42;
            patch.ampEnvelope.sustain = 0.0;
            patch.ampEnvelope.release = 0.32;
            patch.combMix = 0.18;
            patch.combFeedback = 0.3;
            patch.reverbMix = 0.15;
            patch.gain = 0.35;
            break;
        case 113: // Agogo
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Triangle;
            patch.transientShape = 0.48;
            patch.click = 0.12;
            patch.cutoff = 0.65;
            patch.resonance = 0.2;
            patch.ampEnvelope.attack = 0.0005;
            patch.ampEnvelope.decay = 0.32;
            patch.ampEnvelope.sustain = 0.0;
            patch.ampEnvelope.release = 0.22;
            patch.combMix = 0.12;
            patch.combFeedback = 0.42;
            patch.gain = 0.42;
            break;
        case 114: // Steel Drums
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Triangle;
            patch.fmEnabled = true;
            patch.fmAmount = 0.22;
            patch.fmRatio = 3.5;
            patch.transientShape = 0.52;
            patch.click = 0.12;
            patch.cutoff = 0.58;
            patch.resonance = 0.22;
            patch.ampEnvelope.attack = 0.001;
            patch.ampEnvelope.decay = 0.38;
            patch.ampEnvelope.sustain = 0.05;
            patch.ampEnvelope.release = 0.32;
            patch.combMix = 0.15;
            patch.combFeedback = 0.28;
            patch.reverbMix = 0.12;
            patch.gain = 0.4;
            break;
        case 115: // Woodblock
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Triangle;
            patch.transientShape = 0.35;
            patch.transientNoise = 0.06;
            patch.click = 0.12;
            patch.cutoff = 0.55;
            patch.resonance = 0.15;
            patch.ampEnvelope.attack = 0.0002;
            patch.ampEnvelope.decay = 0.08;
            patch.ampEnvelope.sustain = 0.0;
            patch.ampEnvelope.release = 0.06;
            patch.combMix = 0.1;
            patch.combFeedback = 0.2;
            patch.gain = 0.55;
            break;
        case 116: // Taiko Drum
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Saw;
            patch.subEnabled = true;
            patch.subOscillator = 0.42;
            patch.transientShape = 0.35;
            patch.transientNoise = 0.06;
            patch.click = 0.12;
            patch.cutoff = 0.32;
            patch.resonance = 0.12;
            patch.ampEnvelope.attack = 0.001;
            patch.ampEnvelope.decay = 0.42;
            patch.ampEnvelope.sustain = 0.0;
            patch.ampEnvelope.release = 0.32;
            patch.lowPunch = 0.88;
            patch.velocityToAmp = 0.92;
            patch.gain = 0.68;
            break;
        case 117: // Melodic Tom
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Triangle;
            patch.transientShape = 0.48;
            patch.click = 0.35;
            patch.cutoff = 0.42;
            patch.resonance = 0.15;
            patch.ampEnvelope.attack = 0.001;
            patch.ampEnvelope.decay = 0.28;
            patch.ampEnvelope.sustain = 0.0;
            patch.ampEnvelope.release = 0.18;
            patch.combMix = 0.08;
            patch.combFeedback = 0.32;
            patch.velocityToAmp = 0.85;
            patch.gain = 0.58;
            break;
        case 118: // Synth Drum
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Saw;
            patch.fmEnabled = true;
            patch.fmAmount = 0.35;
            patch.fmRatio = 2.2;
            patch.transientShape = 0.55;
            patch.click = 0.42;
            patch.cutoff = 0.38;
            patch.resonance = 0.18;
            patch.ampEnvelope.attack = 0.001;
            patch.ampEnvelope.decay = 0.22;
            patch.ampEnvelope.sustain = 0.0;
            patch.ampEnvelope.release = 0.15;
            patch.drive = 0.08;
            patch.velocityToAmp = 0.88;
            patch.gain = 0.55;
            break;
        case 119: // Reverse Cymbal
            patch.oscillatorA = Waveform::Noise;
            patch.oscillatorB = Waveform::Sine;
            patch.oscillatorC = Waveform::Noise;
            patch.oscillatorCEnabled = true;
            patch.cutoff = 0.68;
            patch.resonance = 0.15;
            patch.ampEnvelope.attack = 0.55;
            patch.ampEnvelope.decay = 0.08;
            patch.ampEnvelope.sustain = 0.0;
            patch.ampEnvelope.release = 0.08;
            patch.combMix = 0.18;
            patch.combFeedback = 0.22;
            patch.reverbMix = 0.25;
            patch.reverbSize = 0.78;
            patch.velocityToAmp = 0.55;
            patch.gain = 0.42;
            break;

        // === SFX (120-127) ===
        case 120: // Guitar Fret Noise
            patch.oscillatorA = Waveform::Noise;
            patch.oscillatorB = Waveform::Sine;
            patch.transientShape = 0.42;
            patch.cutoff = 0.52;
            patch.resonance = 0.12;
            patch.ampEnvelope.attack = 0.001;
            patch.ampEnvelope.decay = 0.06;
            patch.ampEnvelope.sustain = 0.0;
            patch.ampEnvelope.release = 0.04;
            patch.highPass = 0.25;
            patch.gain = 0.35;
            break;
        case 121: // Breath Noise
            patch.oscillatorA = Waveform::Noise;
            patch.cutoff = 0.48;
            patch.resonance = 0.1;
            patch.ampEnvelope.attack = 0.08;
            patch.ampEnvelope.decay = 0.15;
            patch.ampEnvelope.sustain = 0.0;
            patch.ampEnvelope.release = 0.1;
            patch.noiseTone = 0.72;
            patch.gain = 0.32;
            break;
        case 122: // Seashore
            patch.oscillatorA = Waveform::Noise;
            patch.oscillatorB = Waveform::Sine;
            patch.cutoff = 0.38;
            patch.resonance = 0.08;
            patch.ampEnvelope.attack = 0.82;
            patch.ampEnvelope.decay = 0.55;
            patch.ampEnvelope.sustain = 0.0;
            patch.ampEnvelope.release = 0.42;
            patch.chorusEnabled = true;
            patch.chorusMix = 0.22;
            patch.reverbMix = 0.32;
            patch.reverbSize = 0.88;
            patch.gain = 0.28;
            break;
        case 123: // Bird Tweet
            patch.oscillatorA = Waveform::Sine;
            patch.oscillatorB = Waveform::Triangle;
            patch.fmEnabled = true;
            patch.fmAmount = 0.45;
            patch.fmRatio = 3.5;
            patch.cutoff = 0.62;
            patch.resonance = 0.18;
            patch.ampEnvelope.attack = 0.005;
            patch.ampEnvelope.decay = 0.08;
            patch.ampEnvelope.sustain = 0.0;
            patch.ampEnvelope.release = 0.06;
            patch.gain = 0.38;
            break;
        case 124: // Telephone Ring
            patch.oscillatorA = Waveform::Square;
            patch.oscillatorB = Waveform::Square;
            patch.pulseWidth = 0.48;
            patch.fmEnabled = true;
            patch.fmAmount = 0.22;
            patch.fmRatio = 2.8;
            patch.cutoff = 0.55;
            patch.resonance = 0.15;
            patch.ampEnvelope.attack = 0.001;
            patch.ampEnvelope.decay = 0.42;
            patch.ampEnvelope.sustain = 0.55;
            patch.ampEnvelope.release = 0.08;
            patch.gain = 0.45;
            break;
        case 125: // Helicopter
            patch.oscillatorA = Waveform::Noise;
            patch.oscillatorB = Waveform::Saw;
            patch.cutoff = 0.28;
            patch.resonance = 0.12;
            patch.ampEnvelope.attack = 0.15;
            patch.ampEnvelope.decay = 0.08;
            patch.ampEnvelope.sustain = 0.88;
            patch.ampEnvelope.release = 0.15;
            patch.lfoRate = 8.5;
            patch.lfoFilterDepth = 0.22;
            patch.gain = 0.48;
            break;
        case 126: // Applause
            patch.oscillatorA = Waveform::Noise;
            patch.oscillatorB = Waveform::Noise;
            patch.cutoff = 0.62;
            patch.resonance = 0.1;
            patch.ampEnvelope.attack = 0.12;
            patch.ampEnvelope.decay = 0.55;
            patch.ampEnvelope.sustain = 0.0;
            patch.ampEnvelope.release = 0.32;
            patch.reverbMix = 0.22;
            patch.reverbSize = 0.72;
            patch.gain = 0.42;
            break;
        case 127: // Gunshot
            patch.oscillatorA = Waveform::Noise;
            patch.oscillatorB = Waveform::Saw;
            patch.transientShape = 0.35;
            patch.transientNoise = 0.08;
            patch.click = 0.12;
            patch.cutoff = 0.45;
            patch.resonance = 0.15;
            patch.ampEnvelope.attack = 0.0005;
            patch.ampEnvelope.decay = 0.12;
            patch.ampEnvelope.sustain = 0.0;
            patch.ampEnvelope.release = 0.08;
            patch.drive = 0.35;
            patch.reverbMix = 0.15;
            patch.reverbSize = 0.68;
            patch.gain = 0.55;
            break;
        default:
            break;
    }

    // Pad-wide refinements
    if (program >= 88 && program <= 95) {
        patch.chorusEnabled = true;
        patch.chorusMix = std::max(patch.chorusMix, 0.28);
        patch.stereoSpread = std::max(patch.stereoSpread, 0.45);
    }
    if (program >= 80 && program <= 87) {
        patch.vibratoCents = std::max(patch.vibratoCents, 4.0);
        patch.drive = std::max(patch.drive, 0.06);
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
    const bool percussive = laneLower.find("drum") != std::string::npos
        || laneLower.find("perc") != std::string::npos
        || laneLower.find("hat") != std::string::npos
        || laneLower.find("kick") != std::string::npos
        || laneLower.find("snare") != std::string::npos;
    applyCompetitionPresetQuality(
        patch,
        percussive ? kCompetitionMidiQualityPercussive : kCompetitionMidiQuality,
        percussive);
    patch.hifiExciter = std::clamp(patch.hifiExciter, 0.0, 0.18);
    patch.outputTransformer = std::clamp(patch.outputTransformer, 0.0, 0.22);
    patch.outputSoftClip = std::clamp(patch.outputSoftClip, 0.0, 0.18);
    patch.outputGlue = std::clamp(patch.outputGlue, 0.0, 0.26);
    patch.analogColor = std::clamp(patch.analogColor, 0.0, 0.82);
    patch.analogWarmth = std::clamp(patch.analogWarmth, 0.0, 0.90);
    patch.toneTilt = std::clamp(patch.toneTilt, -0.42, 0.55);

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
