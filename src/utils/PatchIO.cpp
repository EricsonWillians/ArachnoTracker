#include "PatchIO.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

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
} // namespace

void savePatch(const SynthPatch& patch, const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("failed to open patch for writing: " + path);
    }

    out << "arachno_patch " << patchFileVersion << "\n";
    out << "name " << std::quoted(patch.name) << "\n";
    out << "oscillators " << waveformName(patch.oscillatorA)
        << " " << waveformName(patch.oscillatorB)
        << " " << waveformName(patch.oscillatorC)
        << " " << waveformName(patch.oscillatorD) << "\n";
    out << "params"
        << " " << (patch.oscillatorAEnabled ? 1.0 : 0.0)
        << " " << (patch.oscillatorBEnabled ? 1.0 : 0.0)
        << " " << (patch.oscillatorCEnabled ? 1.0 : 0.0)
        << " " << (patch.oscillatorDEnabled ? 1.0 : 0.0)
        << " " << patch.oscillatorMix
        << " " << patch.oscillatorCMix
        << " " << patch.oscillatorDMix
        << " " << patch.detuneCents
        << " " << patch.detuneCCents
        << " " << patch.detuneDCents
        << " " << patch.pulseWidth
        << " " << patch.pwmDepth
        << " " << (patch.fmEnabled ? 1.0 : 0.0)
        << " " << patch.fmAmount
        << " " << patch.fmRatio
        << " " << patch.fmFeedback
        << " " << (patch.chorusEnabled ? 1.0 : 0.0)
        << " " << patch.chorusMix
        << " " << patch.chorusRate
        << " " << patch.chorusDepth
        << " " << patch.unisonVoices
        << " " << patch.unisonDetuneCents
        << " " << patch.stereoSpread
        << " " << (patch.subEnabled ? 1.0 : 0.0)
        << " " << patch.subOscillator
        << " " << (patch.noiseEnabled ? 1.0 : 0.0)
        << " " << patch.noise
        << " " << patch.noiseTone
        << " " << patch.cutoff
        << " " << patch.resonance
        << " " << patch.filterEnvelopeAmount
        << " " << patch.lfoFilterDepth
        << " " << patch.lfoPanDepth
        << " " << patch.pitchEnvelopeSemitones
        << " " << patch.pitchEnvelopeDecay
        << " " << patch.lfoRate
        << " " << patch.vibratoCents
        << " " << patch.tremoloDepth
        << " " << (patch.ringEnabled ? 1.0 : 0.0)
        << " " << patch.ringMod
        << " " << (patch.hardSyncEnabled ? 1.0 : 0.0)
        << " " << patch.hardSync
        << " " << patch.drive
        << " " << patch.wavefold
        << " " << (patch.bitCrushEnabled ? 1.0 : 0.0)
        << " " << patch.bitCrush
        << " " << patch.sampleRateReduction
        << " " << patch.combMix
        << " " << patch.combTime
        << " " << patch.combFeedback
        << " " << patch.highPass
        << " " << patch.click
        << " " << patch.transientShape
        << " " << patch.transientNoise
        << " " << patch.transientPitchSemitones
        << " " << patch.transientPitchDecay
        << " " << patch.transientBurstCount
        << " " << patch.transientBurstSpacing
        << " " << patch.transientBurstDecay
        << " " << patch.transientTone
        << " " << patch.transientDecay
        << " " << patch.gain
        << " " << patch.pan
        << "\n";
    out << "amp"
        << " " << patch.ampEnvelope.attack
        << " " << patch.ampEnvelope.decay
        << " " << patch.ampEnvelope.sustain
        << " " << patch.ampEnvelope.release
        << "\n";
    out << "filter"
        << " " << patch.filterEnvelope.attack
        << " " << patch.filterEnvelope.decay
        << " " << patch.filterEnvelope.sustain
        << " " << patch.filterEnvelope.release
        << "\n";
    out << "end_patch\n";
}

SynthPatch loadPatch(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("failed to open patch for reading: " + path);
    }

    expectToken(in, "arachno_patch");
    const int version = readValue<int>(in, "patch version");
    if (version != patchFileVersion) {
        throw std::runtime_error("unsupported patch version: " + std::to_string(version));
    }

    SynthPatch patch;
    expectToken(in, "name");
    patch.name = readQuoted(in, "patch name");

    expectToken(in, "oscillators");
    std::string oscRest;
    std::getline(in, oscRest);
    std::istringstream oscIn(oscRest);
    const std::string oscAName = readValue<std::string>(oscIn, "oscillator A");
    const std::string oscBName = readValue<std::string>(oscIn, "oscillator B");
    patch.oscillatorA = waveformFromName(oscAName);
    patch.oscillatorB = waveformFromName(oscBName);
    std::string oscCName;
    if (oscIn >> oscCName) {
        patch.oscillatorC = waveformFromName(oscCName);
    }
    std::string oscDName;
    if (oscIn >> oscDName) {
        patch.oscillatorD = waveformFromName(oscDName);
    }

    expectToken(in, "params");
    std::string paramsRest;
    std::getline(in, paramsRest);
    std::istringstream paramsIn(paramsRest);
    std::vector<double> params;
    double param = 0.0;
    while (paramsIn >> param) {
        params.push_back(param);
    }
    if (params.size() >= 63) {
        patch.oscillatorAEnabled = params[0] >= 0.5;
        patch.oscillatorBEnabled = params[1] >= 0.5;
        patch.oscillatorCEnabled = params[2] >= 0.5;
        patch.oscillatorDEnabled = params[3] >= 0.5;
        patch.oscillatorMix = params[4];
        patch.oscillatorCMix = params[5];
        patch.oscillatorDMix = params[6];
        patch.detuneCents = params[7];
        patch.detuneCCents = params[8];
        patch.detuneDCents = params[9];
        patch.pulseWidth = params[10];
        patch.pwmDepth = params[11];
        patch.fmEnabled = params[12] >= 0.5;
        patch.fmAmount = params[13];
        patch.fmRatio = params[14];
        patch.fmFeedback = params[15];
        patch.chorusEnabled = params[16] >= 0.5;
        patch.chorusMix = params[17];
        patch.chorusRate = params[18];
        patch.chorusDepth = params[19];
        patch.unisonVoices = static_cast<int>(std::lround(params[20]));
        patch.unisonDetuneCents = params[21];
        patch.stereoSpread = params[22];
        patch.subEnabled = params[23] >= 0.5;
        patch.subOscillator = params[24];
        patch.noiseEnabled = params[25] >= 0.5;
        patch.noise = params[26];
        patch.noiseTone = params[27];
        patch.cutoff = params[28];
        patch.resonance = params[29];
        patch.filterEnvelopeAmount = params[30];
        patch.lfoFilterDepth = params[31];
        patch.lfoPanDepth = params[32];
        patch.pitchEnvelopeSemitones = params[33];
        patch.pitchEnvelopeDecay = params[34];
        patch.lfoRate = params[35];
        patch.vibratoCents = params[36];
        patch.tremoloDepth = params[37];
        patch.ringEnabled = params[38] >= 0.5;
        patch.ringMod = params[39];
        patch.hardSyncEnabled = params[40] >= 0.5;
        patch.hardSync = params[41];
        patch.drive = params[42];
        patch.wavefold = params[43];
        patch.bitCrushEnabled = params[44] >= 0.5;
        patch.bitCrush = params[45];
        patch.sampleRateReduction = params[46];
        patch.combMix = params[47];
        patch.combTime = params[48];
        patch.combFeedback = params[49];
        patch.highPass = params[50];
        patch.click = params[51];
        patch.transientShape = params[52];
        patch.transientNoise = params[53];
        patch.transientPitchSemitones = params[54];
        patch.transientPitchDecay = params[55];
        patch.transientBurstCount = static_cast<int>(std::lround(params[56]));
        patch.transientBurstSpacing = params[57];
        patch.transientBurstDecay = params[58];
        patch.transientTone = params[59];
        patch.transientDecay = params[60];
        patch.gain = params[61];
        patch.pan = params[62];
    } else if (params.size() >= 55) {
        patch.oscillatorAEnabled = params[0] >= 0.5;
        patch.oscillatorBEnabled = params[1] >= 0.5;
        patch.oscillatorCEnabled = params[2] >= 0.5;
        patch.oscillatorDEnabled = params[3] >= 0.5;
        patch.oscillatorMix = params[4];
        patch.oscillatorCMix = params[5];
        patch.oscillatorDMix = params[6];
        patch.detuneCents = params[7];
        patch.detuneCCents = params[8];
        patch.detuneDCents = params[9];
        patch.pulseWidth = params[10];
        patch.pwmDepth = params[11];
        patch.fmEnabled = params[12] >= 0.5;
        patch.fmAmount = params[13];
        patch.fmRatio = params[14];
        patch.fmFeedback = params[15];
        patch.chorusEnabled = params[16] >= 0.5;
        patch.chorusMix = params[17];
        patch.chorusRate = params[18];
        patch.chorusDepth = params[19];
        patch.unisonVoices = static_cast<int>(std::lround(params[20]));
        patch.unisonDetuneCents = params[21];
        patch.stereoSpread = params[22];
        patch.subEnabled = params[23] >= 0.5;
        patch.subOscillator = params[24];
        patch.noiseEnabled = params[25] >= 0.5;
        patch.noise = params[26];
        patch.cutoff = params[27];
        patch.resonance = params[28];
        patch.filterEnvelopeAmount = params[29];
        patch.lfoFilterDepth = params[30];
        patch.lfoPanDepth = params[31];
        patch.pitchEnvelopeSemitones = params[32];
        patch.pitchEnvelopeDecay = params[33];
        patch.lfoRate = params[34];
        patch.vibratoCents = params[35];
        patch.tremoloDepth = params[36];
        patch.ringEnabled = params[37] >= 0.5;
        patch.ringMod = params[38];
        patch.hardSyncEnabled = params[39] >= 0.5;
        patch.hardSync = params[40];
        patch.drive = params[41];
        patch.wavefold = params[42];
        patch.bitCrushEnabled = params[43] >= 0.5;
        patch.bitCrush = params[44];
        patch.sampleRateReduction = params[45];
        patch.combMix = params[46];
        patch.combTime = params[47];
        patch.combFeedback = params[48];
        patch.highPass = params[49];
        patch.click = params[50];
        patch.transientNoise = params[51];
        patch.transientDecay = params[52];
        patch.gain = params[53];
        patch.pan = params[54];
    } else if (params.size() >= 52) {
        patch.oscillatorAEnabled = params[0] >= 0.5;
        patch.oscillatorBEnabled = params[1] >= 0.5;
        patch.oscillatorCEnabled = params[2] >= 0.5;
        patch.oscillatorMix = params[3];
        patch.oscillatorCMix = params[4];
        patch.detuneCents = params[5];
        patch.detuneCCents = params[6];
        patch.pulseWidth = params[7];
        patch.pwmDepth = params[8];
        patch.fmEnabled = params[9] >= 0.5;
        patch.fmAmount = params[10];
        patch.fmRatio = params[11];
        patch.fmFeedback = params[12];
        patch.chorusEnabled = params[13] >= 0.5;
        patch.chorusMix = params[14];
        patch.chorusRate = params[15];
        patch.chorusDepth = params[16];
        patch.unisonVoices = static_cast<int>(std::lround(params[17]));
        patch.unisonDetuneCents = params[18];
        patch.stereoSpread = params[19];
        patch.subEnabled = params[20] >= 0.5;
        patch.subOscillator = params[21];
        patch.noiseEnabled = params[22] >= 0.5;
        patch.noise = params[23];
        patch.cutoff = params[24];
        patch.resonance = params[25];
        patch.filterEnvelopeAmount = params[26];
        patch.lfoFilterDepth = params[27];
        patch.lfoPanDepth = params[28];
        patch.pitchEnvelopeSemitones = params[29];
        patch.pitchEnvelopeDecay = params[30];
        patch.lfoRate = params[31];
        patch.vibratoCents = params[32];
        patch.tremoloDepth = params[33];
        patch.ringEnabled = params[34] >= 0.5;
        patch.ringMod = params[35];
        patch.hardSyncEnabled = params[36] >= 0.5;
        patch.hardSync = params[37];
        patch.drive = params[38];
        patch.wavefold = params[39];
        patch.bitCrushEnabled = params[40] >= 0.5;
        patch.bitCrush = params[41];
        patch.sampleRateReduction = params[42];
        patch.combMix = params[43];
        patch.combTime = params[44];
        patch.combFeedback = params[45];
        patch.highPass = params[46];
        patch.click = params[47];
        patch.transientNoise = params[48];
        patch.transientDecay = params[49];
        patch.gain = params[50];
        patch.pan = params[51];
    } else if (params.size() >= 33) {
        patch.oscillatorMix = params[0];
        patch.detuneCents = params[1];
        patch.pulseWidth = params[2];
        patch.pwmDepth = params[3];
        patch.fmAmount = params[4];
        patch.fmRatio = params[5];
        patch.chorusMix = params[6];
        patch.chorusRate = params[7];
        patch.chorusDepth = params[8];
        patch.unisonVoices = static_cast<int>(std::lround(params[9]));
        patch.unisonDetuneCents = params[10];
        patch.stereoSpread = params[11];
        patch.subOscillator = params[12];
        patch.noise = params[13];
        patch.cutoff = params[14];
        patch.resonance = params[15];
        patch.filterEnvelopeAmount = params[16];
        patch.pitchEnvelopeSemitones = params[17];
        patch.pitchEnvelopeDecay = params[18];
        patch.lfoRate = params[19];
        patch.vibratoCents = params[20];
        patch.tremoloDepth = params[21];
        patch.ringMod = params[22];
        patch.hardSync = params[23];
        patch.drive = params[24];
        patch.bitCrush = params[25];
        patch.sampleRateReduction = params[26];
        patch.highPass = params[27];
        patch.click = params[28];
        patch.transientNoise = params[29];
        patch.transientDecay = params[30];
        patch.gain = params[31];
        patch.pan = params[32];
    } else if (params.size() >= 30) {
        patch.oscillatorMix = params[0];
        patch.detuneCents = params[1];
        patch.pulseWidth = params[2];
        patch.pwmDepth = params[3];
        patch.fmAmount = params[4];
        patch.fmRatio = params[5];
        patch.unisonVoices = static_cast<int>(std::lround(params[6]));
        patch.unisonDetuneCents = params[7];
        patch.stereoSpread = params[8];
        patch.subOscillator = params[9];
        patch.noise = params[10];
        patch.cutoff = params[11];
        patch.resonance = params[12];
        patch.filterEnvelopeAmount = params[13];
        patch.pitchEnvelopeSemitones = params[14];
        patch.pitchEnvelopeDecay = params[15];
        patch.lfoRate = params[16];
        patch.vibratoCents = params[17];
        patch.tremoloDepth = params[18];
        patch.ringMod = params[19];
        patch.hardSync = params[20];
        patch.drive = params[21];
        patch.bitCrush = params[22];
        patch.sampleRateReduction = params[23];
        patch.highPass = params[24];
        patch.click = params[25];
        patch.transientNoise = params[26];
        patch.transientDecay = params[27];
        patch.gain = params[28];
        patch.pan = params[29];
    } else if (params.size() >= 26) {
        patch.oscillatorMix = params[0];
        patch.detuneCents = params[1];
        patch.unisonVoices = static_cast<int>(std::lround(params[2]));
        patch.unisonDetuneCents = params[3];
        patch.stereoSpread = params[4];
        patch.subOscillator = params[5];
        patch.noise = params[6];
        patch.cutoff = params[7];
        patch.resonance = params[8];
        patch.filterEnvelopeAmount = params[9];
        patch.pitchEnvelopeSemitones = params[10];
        patch.pitchEnvelopeDecay = params[11];
        patch.lfoRate = params[12];
        patch.vibratoCents = params[13];
        patch.tremoloDepth = params[14];
        patch.ringMod = params[15];
        patch.hardSync = params[16];
        patch.drive = params[17];
        patch.bitCrush = params[18];
        patch.sampleRateReduction = params[19];
        patch.highPass = params[20];
        patch.click = params[21];
        patch.transientNoise = params[22];
        patch.transientDecay = params[23];
        patch.gain = params[24];
        patch.pan = params[25];
    } else if (params.size() >= 20) {
        patch.oscillatorMix = params[0];
        patch.detuneCents = params[1];
        patch.subOscillator = params[2];
        patch.noise = params[3];
        patch.cutoff = params[4];
        patch.resonance = params[5];
        patch.filterEnvelopeAmount = params[6];
        patch.pitchEnvelopeSemitones = params[7];
        patch.pitchEnvelopeDecay = params[8];
        patch.lfoRate = params[9];
        patch.vibratoCents = params[10];
        patch.tremoloDepth = params[11];
        patch.ringMod = params[12];
        patch.hardSync = params[13];
        patch.drive = params[14];
        patch.bitCrush = params[15];
        patch.sampleRateReduction = params[16];
        patch.highPass = params[17];
        patch.gain = params[18];
        patch.pan = params[19];
    } else if (params.size() >= 13) {
        patch.oscillatorMix = params[0];
        patch.detuneCents = params[1];
        patch.subOscillator = params[2];
        patch.noise = params[3];
        patch.cutoff = params[4];
        patch.resonance = params[5];
        patch.filterEnvelopeAmount = params[6];
        patch.lfoRate = params[7];
        patch.vibratoCents = params[8];
        patch.tremoloDepth = params[9];
        patch.drive = params[10];
        patch.gain = params[11];
        patch.pan = params[12];
    } else {
        throw std::runtime_error("failed to read patch params");
    }

    expectToken(in, "amp");
    patch.ampEnvelope.attack = readValue<double>(in, "amp attack");
    patch.ampEnvelope.decay = readValue<double>(in, "amp decay");
    patch.ampEnvelope.sustain = readValue<double>(in, "amp sustain");
    patch.ampEnvelope.release = readValue<double>(in, "amp release");

    expectToken(in, "filter");
    patch.filterEnvelope.attack = readValue<double>(in, "filter attack");
    patch.filterEnvelope.decay = readValue<double>(in, "filter decay");
    patch.filterEnvelope.sustain = readValue<double>(in, "filter sustain");
    patch.filterEnvelope.release = readValue<double>(in, "filter release");

    expectToken(in, "end_patch");
    return patch;
}

} // namespace arachno
