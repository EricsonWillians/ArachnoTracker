#include "ProjectIO.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace arachno {

namespace {
void expectToken(std::istream& in, const std::string& expected) {
    std::string token;
    in >> token;
    if (token != expected) {
        throw std::runtime_error("expected token '" + expected + "', got '" + token + "'");
    }
}

template <typename T>
T readValue(std::istream& in, const std::string& field) {
    T value {};
    if (!(in >> value)) {
        throw std::runtime_error("failed to read " + field);
    }
    return value;
}

std::string readQuoted(std::istream& in, const std::string& field) {
    std::string value;
    if (!(in >> std::quoted(value))) {
        throw std::runtime_error("failed to read " + field);
    }
    return value;
}

void validateSong(const Song& song) {
    if (song.bpm <= 0.0) {
        throw std::runtime_error("project bpm must be positive");
    }
    if (song.rowsPerBeat <= 0) {
        throw std::runtime_error("project rows_per_beat must be positive");
    }
    if (song.sampleRate <= 0) {
        throw std::runtime_error("project sample_rate must be positive");
    }
    for (int patternIndex : song.order) {
        if (patternIndex < 0 || patternIndex >= static_cast<int>(song.patterns.size())) {
            throw std::runtime_error("project order contains an invalid pattern index");
        }
    }
}
} // namespace

void saveProject(const Song& song, const std::string& path) {
    validateSong(song);

    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("failed to open project for writing: " + path);
    }

    out << "arachno_project " << projectFileVersion << "\n";
    out << "title " << std::quoted(song.title) << "\n";
    out << "bpm " << song.bpm << "\n";
    out << "rows_per_beat " << song.rowsPerBeat << "\n";
    out << "sample_rate " << song.sampleRate << "\n";

    out << "tracks " << song.tracks.size() << "\n";
    for (const Track& track : song.tracks) {
        out << "track " << std::quoted(track.name)
            << " " << track.volume
            << " " << track.pan
            << " " << track.muted
            << " " << track.solo
            << "\n";
    }

    out << "instruments " << song.instruments.size() << "\n";
    for (const Instrument& instrument : song.instruments) {
        const SynthPatch& patch = instrument.patch;
        out << "instrument " << instrument.id
            << " " << std::quoted(patch.name)
            << " " << waveformName(patch.oscillatorA)
            << " " << waveformName(patch.oscillatorB)
            << " " << patch.oscillatorMix
            << " " << patch.detuneCents
            << " " << patch.subOscillator
            << " " << patch.noise
            << " " << patch.cutoff
            << " " << patch.resonance
            << " " << patch.filterEnvelopeAmount
            << " " << patch.lfoRate
            << " " << patch.vibratoCents
            << " " << patch.tremoloDepth
            << " " << patch.drive
            << " " << patch.gain
            << " " << patch.pan
            << " " << patch.ampEnvelope.attack
            << " " << patch.ampEnvelope.decay
            << " " << patch.ampEnvelope.sustain
            << " " << patch.ampEnvelope.release
            << " " << patch.filterEnvelope.attack
            << " " << patch.filterEnvelope.decay
            << " " << patch.filterEnvelope.sustain
            << " " << patch.filterEnvelope.release
            << "\n";
    }

    out << "patterns " << song.patterns.size() << "\n";
    for (const Pattern& pattern : song.patterns) {
        out << "pattern " << std::quoted(pattern.name())
            << " " << pattern.rowCount()
            << " " << pattern.trackCount()
            << "\n";

        for (int row = 0; row < pattern.rowCount(); ++row) {
            for (int track = 0; track < pattern.trackCount(); ++track) {
                const PatternStep& step = pattern.step(row, track);
                if (step.empty()) {
                    continue;
                }

                out << "step " << row
                    << " " << track
                    << " " << (step.note.has_value() ? 1 : 0);
                if (step.note.has_value()) {
                    out << " " << step.note->midi
                        << " " << step.note->velocity;
                }
                out << " " << step.instrument
                    << " " << step.gate
                    << " " << step.microOffsetRows
                    << " " << step.automation.size();
                for (const auto& [name, value] : step.automation) {
                    out << " " << std::quoted(name) << " " << value;
                }
                out << "\n";
            }
        }
        out << "end_pattern\n";
    }

    out << "order " << song.order.size();
    for (int patternIndex : song.order) {
        out << " " << patternIndex;
    }
    out << "\n";
    out << "end_project\n";
}

Song loadProject(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("failed to open project for reading: " + path);
    }

    expectToken(in, "arachno_project");
    const int version = readValue<int>(in, "project version");
    if (version != projectFileVersion) {
        throw std::runtime_error("unsupported project version: " + std::to_string(version));
    }

    Song song;
    expectToken(in, "title");
    song.title = readQuoted(in, "title");
    expectToken(in, "bpm");
    song.bpm = readValue<double>(in, "bpm");
    expectToken(in, "rows_per_beat");
    song.rowsPerBeat = readValue<int>(in, "rows_per_beat");
    expectToken(in, "sample_rate");
    song.sampleRate = readValue<int>(in, "sample_rate");

    expectToken(in, "tracks");
    const int trackCount = readValue<int>(in, "track count");
    song.tracks.clear();
    for (int i = 0; i < trackCount; ++i) {
        expectToken(in, "track");
        Track track;
        track.name = readQuoted(in, "track name");
        track.volume = readValue<double>(in, "track volume");
        track.pan = readValue<double>(in, "track pan");
        track.muted = readValue<bool>(in, "track muted");
        track.solo = readValue<bool>(in, "track solo");
        song.tracks.push_back(track);
    }

    expectToken(in, "instruments");
    const int instrumentCount = readValue<int>(in, "instrument count");
    song.instruments.clear();
    for (int i = 0; i < instrumentCount; ++i) {
        expectToken(in, "instrument");
        Instrument instrument;
        instrument.id = readValue<int>(in, "instrument id");
        SynthPatch& patch = instrument.patch;
        patch.name = readQuoted(in, "instrument name");
        patch.oscillatorA = waveformFromName(readValue<std::string>(in, "oscillator A"));
        patch.oscillatorB = waveformFromName(readValue<std::string>(in, "oscillator B"));
        patch.oscillatorMix = readValue<double>(in, "oscillator mix");
        patch.detuneCents = readValue<double>(in, "detune");
        patch.subOscillator = readValue<double>(in, "sub oscillator");
        patch.noise = readValue<double>(in, "noise");
        patch.cutoff = readValue<double>(in, "cutoff");
        patch.resonance = readValue<double>(in, "resonance");
        patch.filterEnvelopeAmount = readValue<double>(in, "filter envelope amount");
        patch.lfoRate = readValue<double>(in, "lfo rate");
        patch.vibratoCents = readValue<double>(in, "vibrato");
        patch.tremoloDepth = readValue<double>(in, "tremolo");
        patch.drive = readValue<double>(in, "drive");
        patch.gain = readValue<double>(in, "gain");
        patch.pan = readValue<double>(in, "pan");
        patch.ampEnvelope.attack = readValue<double>(in, "amp attack");
        patch.ampEnvelope.decay = readValue<double>(in, "amp decay");
        patch.ampEnvelope.sustain = readValue<double>(in, "amp sustain");
        patch.ampEnvelope.release = readValue<double>(in, "amp release");
        patch.filterEnvelope.attack = readValue<double>(in, "filter attack");
        patch.filterEnvelope.decay = readValue<double>(in, "filter decay");
        patch.filterEnvelope.sustain = readValue<double>(in, "filter sustain");
        patch.filterEnvelope.release = readValue<double>(in, "filter release");
        song.instruments.push_back(instrument);
    }

    expectToken(in, "patterns");
    const int patternCount = readValue<int>(in, "pattern count");
    song.patterns.clear();
    for (int patternIndex = 0; patternIndex < patternCount; ++patternIndex) {
        expectToken(in, "pattern");
        const std::string name = readQuoted(in, "pattern name");
        const int rows = readValue<int>(in, "pattern rows");
        const int tracks = readValue<int>(in, "pattern tracks");
        Pattern pattern(name, rows, tracks);

        std::string token;
        while (in >> token) {
            if (token == "end_pattern") {
                break;
            }
            if (token != "step") {
                throw std::runtime_error("expected step or end_pattern, got '" + token + "'");
            }

            const int row = readValue<int>(in, "step row");
            const int track = readValue<int>(in, "step track");
            const bool hasNote = readValue<int>(in, "step note flag") != 0;
            PatternStep& step = pattern.step(row, track);
            if (hasNote) {
                const int midi = readValue<int>(in, "step midi");
                const float velocity = readValue<float>(in, "step velocity");
                step.note = Note(midi, velocity);
            }
            step.instrument = readValue<int>(in, "step instrument");
            step.gate = readValue<double>(in, "step gate");
            step.microOffsetRows = readValue<double>(in, "step micro offset");
            const int automationCount = readValue<int>(in, "automation count");
            for (int automationIndex = 0; automationIndex < automationCount; ++automationIndex) {
                const std::string name = readQuoted(in, "automation name");
                const double value = readValue<double>(in, "automation value");
                step.automation[name] = value;
            }
        }
        song.patterns.push_back(pattern);
    }

    expectToken(in, "order");
    const int orderCount = readValue<int>(in, "order count");
    song.order.clear();
    for (int i = 0; i < orderCount; ++i) {
        song.order.push_back(readValue<int>(in, "order pattern index"));
    }
    expectToken(in, "end_project");

    validateSong(song);
    return song;
}

} // namespace arachno
