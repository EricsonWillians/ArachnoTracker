#include "ProjectIO.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace arachno {

namespace {
constexpr double kCompetitionPresetQualityProject = 0.82;
constexpr double kCompetitionPresetQualityPercussiveProject = 0.76;

bool looksLikePercussivePatch(const std::string& name) {
    std::string lowerName;
    lowerName.reserve(name.size());
    for (unsigned char c : name) {
        lowerName.push_back(static_cast<char>(std::tolower(c)));
    }
    constexpr const char* kPercussiveTokens[] = {
        "kick",
        "snare",
        "hat",
        "clap",
        "tom",
        "ride",
        "rim",
        "cym",
        "shaker",
        "drum",
        "perc",
        "noisehit"};

    return std::any_of(std::begin(kPercussiveTokens), std::end(kPercussiveTokens), [&](const char* token) {
        return lowerName.find(token) != std::string::npos;
    });
}

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

int waveformToIndex(Waveform waveform) {
    switch (waveform) {
        case Waveform::Sine:
            return 0;
        case Waveform::Square:
            return 1;
        case Waveform::Saw:
            return 2;
        case Waveform::Triangle:
            return 3;
        case Waveform::Noise:
            return 4;
        case Waveform::SuperSaw:
            return 5;
    }
    return 2;
}

Waveform waveformFromIndex(int value) {
    switch (value) {
        case 0:
            return Waveform::Sine;
        case 1:
            return Waveform::Square;
        case 2:
            return Waveform::Saw;
        case 3:
            return Waveform::Triangle;
        case 4:
            return Waveform::Noise;
        case 5:
            return Waveform::SuperSaw;
        default:
            return Waveform::Saw;
    }
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
    out << std::setprecision(std::numeric_limits<double>::max_digits10);

    out << "arachno_project " << projectFileVersion << "\n";
    out << "title " << std::quoted(song.title) << "\n";
    out << "author " << std::quoted(song.author) << "\n";
    out << "description " << std::quoted(song.description) << "\n";
    out << "notes " << std::quoted(song.notes) << "\n";
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
            << " " << patch.pitchEnvelopeSemitones
            << " " << patch.pitchEnvelopeDecay
            << " " << patch.ringMod
            << " " << patch.hardSync
            << " " << patch.bitCrush
            << " " << patch.sampleRateReduction
            << " " << patch.highPass
            << " " << patch.unisonVoices
            << " " << patch.unisonDetuneCents
            << " " << patch.stereoSpread
            << " " << patch.click
            << " " << patch.transientNoise
            << " " << patch.transientDecay
            << " " << patch.pulseWidth
            << " " << patch.pwmDepth
            << " " << patch.fmAmount
            << " " << patch.fmRatio
            << " " << patch.chorusMix
            << " " << patch.chorusRate
            << " " << patch.chorusDepth
            << " " << waveformToIndex(patch.oscillatorC)
            << " " << (patch.oscillatorAEnabled ? 1.0 : 0.0)
            << " " << (patch.oscillatorBEnabled ? 1.0 : 0.0)
            << " " << (patch.oscillatorCEnabled ? 1.0 : 0.0)
            << " " << patch.oscillatorCMix
            << " " << patch.detuneCCents
            << " " << (patch.subEnabled ? 1.0 : 0.0)
            << " " << (patch.noiseEnabled ? 1.0 : 0.0)
            << " " << (patch.fmEnabled ? 1.0 : 0.0)
            << " " << patch.fmFeedback
            << " " << (patch.ringEnabled ? 1.0 : 0.0)
            << " " << (patch.hardSyncEnabled ? 1.0 : 0.0)
            << " " << (patch.chorusEnabled ? 1.0 : 0.0)
            << " " << (patch.bitCrushEnabled ? 1.0 : 0.0)
            << " " << patch.wavefold
            << " " << patch.combMix
            << " " << patch.combTime
            << " " << patch.combFeedback
            << " " << patch.lfoFilterDepth
            << " " << patch.lfoPanDepth
            << " " << waveformToIndex(patch.oscillatorD)
            << " " << (patch.oscillatorDEnabled ? 1.0 : 0.0)
            << " " << patch.oscillatorDMix
            << " " << patch.detuneDCents
            << " " << patch.noiseTone
            << " " << patch.transientShape
            << " " << patch.transientPitchSemitones
            << " " << patch.transientPitchDecay
            << " " << patch.transientBurstCount
            << " " << patch.transientBurstSpacing
            << " " << patch.transientBurstDecay
            << " " << patch.transientTone
            << " " << patch.fmAlgorithm
            << " " << patch.filterMode
            << " " << patch.filterDrive
            << " " << patch.filterKeytrack
            << " " << patch.analogColor
            << " " << patch.toneTilt
            << " " << patch.oscALevel
            << " " << patch.oscBLevel
            << " " << patch.oscCLevel
            << " " << patch.oscDLevel
            << " " << patch.oscADetuneCents
            << " " << patch.oscBDetuneCents
            << " " << patch.oscCDetuneCents
            << " " << patch.oscDDetuneCents
            << " " << patch.oscAPulseWidth
            << " " << patch.oscBPulseWidth
            << " " << patch.oscCPulseWidth
            << " " << patch.oscDPulseWidth
            << " " << patch.oscAPwmDepth
            << " " << patch.oscBPwmDepth
            << " " << patch.oscCPwmDepth
            << " " << patch.oscDPwmDepth
            << " " << patch.oscADrive
            << " " << patch.oscBDrive
            << " " << patch.oscCDrive
            << " " << patch.oscDDrive
            << " " << patch.delayMix
            << " " << patch.delayTime
            << " " << patch.delayFeedback
            << " " << patch.delayTone
            << " " << patch.reverbMix
            << " " << patch.reverbSize
            << " " << patch.reverbDamping
            << " " << patch.reverbPreDelay
            << " " << patch.vintageDrift
            << " " << patch.wowFlutter
            << " " << patch.chorusFeedback
            << " " << patch.chorusDelay
            << " " << patch.chorusWidth
            << " " << patch.chorusEnsemble
            << " " << patch.delayStereo
            << " " << patch.delayModDepth
            << " " << patch.delayDrive
            << " " << patch.delayDucking
            << " " << patch.reverbDiffusion
            << " " << patch.reverbWidth
            << " " << patch.reverbShimmer
            << " " << patch.reverbModDepth
            << " " << patch.tapeColor
            << " " << patch.airBoost
            << " " << patch.lowPunch
            << " " << patch.analogWarmth
            << " " << patch.voiceSlop
            << " " << patch.phaseScatter
            << " " << patch.chorusTone
            << " " << patch.delayDiffusion
            << " " << patch.reverbDecay
            << " " << patch.reverbEarlyMix
            << " " << patch.consoleCrosstalk
            << " " << patch.outputGlue
            << " " << patch.unisonWarp
            << " " << patch.unisonHumanize
            << " " << patch.fmColor
            << " " << patch.fmSpread
            << " " << patch.chorusJitter
            << " " << patch.chorusSaturation
            << " " << patch.delayWow
            << " " << patch.delayCrossfeed
            << " " << patch.reverbTone
            << " " << patch.reverbChorus
            << " " << patch.reverbBloom
            << " " << patch.stereoDepth
            << " " << patch.hifiExciter
            << " " << patch.outputTransformer
            << " " << patch.outputSoftClip
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
                out << " " << step.effects.size();
                for (const EffectCommand& effect : step.effects) {
                    out << " " << std::quoted(effect.name)
                        << " " << effect.parameters.size();
                    for (const auto& [parameter, parameterValue] : effect.parameters) {
                        out << " " << std::quoted(parameter)
                            << " " << parameterValue;
                    }
                }
                out << " " << (step.probability.has_value() ? 1 : 0)
                    << " " << step.probability.value_or(1.0)
                    << " " << step.retriggerCount
                    << " " << step.retriggerSpacingRows
                    << " " << step.retriggerVelocityDecay;
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
    if (version < minimumProjectFileVersion || version > projectFileVersion) {
        throw std::runtime_error("unsupported project version: " + std::to_string(version));
    }

    Song song;
    expectToken(in, "title");
    song.title = readQuoted(in, "title");

    std::string token;
    in >> token;
    if (token == "author") {
        song.author = readQuoted(in, "author");
        expectToken(in, "description");
        song.description = readQuoted(in, "description");
        expectToken(in, "notes");
        song.notes = readQuoted(in, "notes");
        expectToken(in, "bpm");
    } else if (token != "bpm") {
        throw std::runtime_error("expected token 'author' or 'bpm', got '" + token + "'");
    }

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
        std::string extra;
        std::getline(in, extra);
        std::istringstream extraIn(extra);
        (void)(extraIn >> patch.pitchEnvelopeSemitones
            >> patch.pitchEnvelopeDecay
            >> patch.ringMod
            >> patch.hardSync
            >> patch.bitCrush
            >> patch.sampleRateReduction
            >> patch.highPass
            >> patch.unisonVoices
            >> patch.unisonDetuneCents
            >> patch.stereoSpread
            >> patch.click
            >> patch.transientNoise
            >> patch.transientDecay
            >> patch.pulseWidth
            >> patch.pwmDepth
            >> patch.fmAmount
            >> patch.fmRatio
            >> patch.chorusMix
            >> patch.chorusRate
            >> patch.chorusDepth);
        int oscCIndex = waveformToIndex(patch.oscillatorC);
        double oscAEnabled = patch.oscillatorAEnabled ? 1.0 : 0.0;
        double oscBEnabled = patch.oscillatorBEnabled ? 1.0 : 0.0;
        double oscCEnabled = patch.oscillatorCEnabled ? 1.0 : 0.0;
        double oscDEnabled = patch.oscillatorDEnabled ? 1.0 : 0.0;
        double subEnabled = patch.subEnabled ? 1.0 : 0.0;
        double noiseEnabled = patch.noiseEnabled ? 1.0 : 0.0;
        double fmEnabled = patch.fmEnabled ? 1.0 : 0.0;
        double ringEnabled = patch.ringEnabled ? 1.0 : 0.0;
        double hardSyncEnabled = patch.hardSyncEnabled ? 1.0 : 0.0;
        double chorusEnabled = patch.chorusEnabled ? 1.0 : 0.0;
        double bitCrushEnabled = patch.bitCrushEnabled ? 1.0 : 0.0;
        int oscDIndex = waveformToIndex(patch.oscillatorD);
        (void)(extraIn >> oscCIndex
            >> oscAEnabled
            >> oscBEnabled
            >> oscCEnabled
            >> patch.oscillatorCMix
            >> patch.detuneCCents
            >> subEnabled
            >> noiseEnabled
            >> fmEnabled
            >> patch.fmFeedback
            >> ringEnabled
            >> hardSyncEnabled
            >> chorusEnabled
            >> bitCrushEnabled
            >> patch.wavefold
            >> patch.combMix
            >> patch.combTime
            >> patch.combFeedback
            >> patch.lfoFilterDepth
            >> patch.lfoPanDepth);
        (void)(extraIn >> oscDIndex
            >> oscDEnabled
            >> patch.oscillatorDMix
            >> patch.detuneDCents);
        (void)(extraIn >> patch.noiseTone
            >> patch.transientShape
            >> patch.transientPitchSemitones
            >> patch.transientPitchDecay
            >> patch.transientBurstCount
            >> patch.transientBurstSpacing
            >> patch.transientBurstDecay
            >> patch.transientTone);
        (void)(extraIn >> patch.fmAlgorithm
            >> patch.filterMode
            >> patch.filterDrive
            >> patch.filterKeytrack
            >> patch.analogColor
            >> patch.toneTilt);
        (void)(extraIn >> patch.oscALevel
            >> patch.oscBLevel
            >> patch.oscCLevel
            >> patch.oscDLevel
            >> patch.oscADetuneCents
            >> patch.oscBDetuneCents
            >> patch.oscCDetuneCents
            >> patch.oscDDetuneCents
            >> patch.oscAPulseWidth
            >> patch.oscBPulseWidth
            >> patch.oscCPulseWidth
            >> patch.oscDPulseWidth
            >> patch.oscAPwmDepth
            >> patch.oscBPwmDepth
            >> patch.oscCPwmDepth
            >> patch.oscDPwmDepth
            >> patch.oscADrive
            >> patch.oscBDrive
            >> patch.oscCDrive
            >> patch.oscDDrive);
        {
            double chorusFeedback = patch.chorusFeedback;
            double chorusDelay = patch.chorusDelay;
            double chorusWidth = patch.chorusWidth;
            double delayMix = patch.delayMix;
            double delayTime = patch.delayTime;
            double delayFeedback = patch.delayFeedback;
            double delayTone = patch.delayTone;
            double reverbMix = patch.reverbMix;
            double reverbSize = patch.reverbSize;
            double reverbDamping = patch.reverbDamping;
            double reverbPreDelay = patch.reverbPreDelay;
            double vintageDrift = patch.vintageDrift;
            double wowFlutter = patch.wowFlutter;
            if (extraIn >> delayMix
                >> delayTime
                >> delayFeedback
                >> delayTone
                >> reverbMix
                >> reverbSize
                >> reverbDamping
                >> reverbPreDelay
                >> vintageDrift
                >> wowFlutter
                >> chorusFeedback
                >> chorusDelay
                >> chorusWidth) {
                patch.chorusFeedback = chorusFeedback;
                patch.chorusDelay = chorusDelay;
                patch.chorusWidth = chorusWidth;
                patch.delayMix = delayMix;
                patch.delayTime = delayTime;
                patch.delayFeedback = delayFeedback;
                patch.delayTone = delayTone;
                patch.reverbMix = reverbMix;
                patch.reverbSize = reverbSize;
                patch.reverbDamping = reverbDamping;
                patch.reverbPreDelay = reverbPreDelay;
                patch.vintageDrift = vintageDrift;
                patch.wowFlutter = wowFlutter;
            }
            double chorusEnsemble = patch.chorusEnsemble;
            double delayStereo = patch.delayStereo;
            double delayModDepth = patch.delayModDepth;
            double delayDrive = patch.delayDrive;
            double delayDucking = patch.delayDucking;
            double reverbDiffusion = patch.reverbDiffusion;
            double reverbWidth = patch.reverbWidth;
            double reverbShimmer = patch.reverbShimmer;
            double reverbModDepth = patch.reverbModDepth;
            double tapeColor = patch.tapeColor;
            double airBoost = patch.airBoost;
            double lowPunch = patch.lowPunch;
            if (extraIn >> chorusEnsemble
                >> delayStereo
                >> delayModDepth
                >> delayDrive
                >> delayDucking
                >> reverbDiffusion
                >> reverbWidth
                >> reverbShimmer
                >> reverbModDepth
                >> tapeColor
                >> airBoost
                >> lowPunch) {
                patch.chorusEnsemble = chorusEnsemble;
                patch.delayStereo = delayStereo;
                patch.delayModDepth = delayModDepth;
                patch.delayDrive = delayDrive;
                patch.delayDucking = delayDucking;
                patch.reverbDiffusion = reverbDiffusion;
                patch.reverbWidth = reverbWidth;
                patch.reverbShimmer = reverbShimmer;
                patch.reverbModDepth = reverbModDepth;
                patch.tapeColor = tapeColor;
                patch.airBoost = airBoost;
                patch.lowPunch = lowPunch;
            }
            double analogWarmth = patch.analogWarmth;
            double voiceSlop = patch.voiceSlop;
            double phaseScatter = patch.phaseScatter;
            double chorusTone = patch.chorusTone;
            double delayDiffusion = patch.delayDiffusion;
            double reverbDecay = patch.reverbDecay;
            double reverbEarlyMix = patch.reverbEarlyMix;
            double consoleCrosstalk = patch.consoleCrosstalk;
            double outputGlue = patch.outputGlue;
            if (extraIn >> analogWarmth
                >> voiceSlop
                >> phaseScatter
                >> chorusTone
                >> delayDiffusion
                >> reverbDecay
                >> reverbEarlyMix
                >> consoleCrosstalk
                >> outputGlue) {
                patch.analogWarmth = analogWarmth;
                patch.voiceSlop = voiceSlop;
                patch.phaseScatter = phaseScatter;
                patch.chorusTone = chorusTone;
                patch.delayDiffusion = delayDiffusion;
                patch.reverbDecay = reverbDecay;
                patch.reverbEarlyMix = reverbEarlyMix;
                patch.consoleCrosstalk = consoleCrosstalk;
                patch.outputGlue = outputGlue;
            }
            double unisonWarp = patch.unisonWarp;
            double unisonHumanize = patch.unisonHumanize;
            double fmColor = patch.fmColor;
            double fmSpread = patch.fmSpread;
            double chorusJitter = patch.chorusJitter;
            double chorusSaturation = patch.chorusSaturation;
            double delayWow = patch.delayWow;
            double delayCrossfeed = patch.delayCrossfeed;
            double reverbTone = patch.reverbTone;
            double reverbChorus = patch.reverbChorus;
            double reverbBloom = patch.reverbBloom;
            double stereoDepth = patch.stereoDepth;
            double hifiExciter = patch.hifiExciter;
            double outputTransformer = patch.outputTransformer;
            double outputSoftClip = patch.outputSoftClip;
            if (extraIn >> unisonWarp
                >> unisonHumanize
                >> fmColor
                >> fmSpread
                >> chorusJitter
                >> chorusSaturation
                >> delayWow
                >> delayCrossfeed
                >> reverbTone
                >> reverbChorus
                >> reverbBloom
                >> stereoDepth
                >> hifiExciter
                >> outputTransformer
                >> outputSoftClip) {
                patch.unisonWarp = unisonWarp;
                patch.unisonHumanize = unisonHumanize;
                patch.fmColor = fmColor;
                patch.fmSpread = fmSpread;
                patch.chorusJitter = chorusJitter;
                patch.chorusSaturation = chorusSaturation;
                patch.delayWow = delayWow;
                patch.delayCrossfeed = delayCrossfeed;
                patch.reverbTone = reverbTone;
                patch.reverbChorus = reverbChorus;
                patch.reverbBloom = reverbBloom;
                patch.stereoDepth = stereoDepth;
                patch.hifiExciter = hifiExciter;
                patch.outputTransformer = outputTransformer;
                patch.outputSoftClip = outputSoftClip;
            }
        }
        patch.oscillatorC = waveformFromIndex(oscCIndex);
        patch.oscillatorD = waveformFromIndex(oscDIndex);
        patch.oscillatorAEnabled = oscAEnabled >= 0.5;
        patch.oscillatorBEnabled = oscBEnabled >= 0.5;
        patch.oscillatorCEnabled = oscCEnabled >= 0.5;
        patch.oscillatorDEnabled = oscDEnabled >= 0.5;
        patch.subEnabled = subEnabled >= 0.5;
        patch.noiseEnabled = noiseEnabled >= 0.5;
        patch.fmEnabled = fmEnabled >= 0.5;
        patch.ringEnabled = ringEnabled >= 0.5;
        patch.hardSyncEnabled = hardSyncEnabled >= 0.5;
        patch.chorusEnabled = chorusEnabled >= 0.5;
        patch.bitCrushEnabled = bitCrushEnabled >= 0.5;
        applyCompetitionPresetQuality(
            patch,
            looksLikePercussivePatch(patch.name)
                ? kCompetitionPresetQualityPercussiveProject
                : kCompetitionPresetQualityProject,
            looksLikePercussivePatch(patch.name));
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
            if (version >= 2) {
                const int effectCount = readValue<int>(in, "effect count");
                for (int effectIndex = 0; effectIndex < effectCount; ++effectIndex) {
                    EffectCommand effect;
                    effect.name = readQuoted(in, "effect name");
                    const int parameterCount = readValue<int>(in, "effect parameter count");
                    for (int parameterIndex = 0; parameterIndex < parameterCount; ++parameterIndex) {
                        const std::string parameter = readQuoted(in, "effect parameter");
                        const double parameterValue = readValue<double>(in, "effect parameter value");
                        effect.parameters[parameter] = parameterValue;
                    }
                    step.effects.push_back(effect);
                }
            }
            std::string extra;
            std::getline(in, extra);
            std::istringstream extraIn(extra);
            int hasProbability = 0;
            double probability = 1.0;
            if (extraIn >> hasProbability >> probability) {
                if (hasProbability != 0) {
                    step.probability = probability;
                }
                extraIn >> step.retriggerCount
                    >> step.retriggerSpacingRows
                    >> step.retriggerVelocityDecay;
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
