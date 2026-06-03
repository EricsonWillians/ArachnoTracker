#include "RealtimePlayback.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

#include "StepEffects.h"

namespace arachno {

namespace {
float safetySaturate(double value) {
    const double threshold = 0.98;
    const double magnitude = std::abs(value);
    if (magnitude <= threshold) {
        return static_cast<float>(value);
    }
    const double excess = magnitude - threshold;
    const double softened = threshold + (excess / (1.0 + excess * 4.0)) * 0.02;
    return static_cast<float>(std::copysign(std::min(0.999, softened), value));
}

double approach(double current, double target, double coefficient) {
    return target + (current - target) * coefficient;
}

double dcBlock(double sample, double& previousInput, double& previousOutput) {
    constexpr double coefficient = 0.995;
    const double output = sample - previousInput + coefficient * previousOutput;
    previousInput = sample;
    previousOutput = output;
    if (std::abs(output) < 1e-20) {
        return 0.0;
    }
    return output;
}

double clampRow(double row, double totalRows) {
    if (totalRows <= 0.0) {
        return 0.0;
    }
    return std::clamp(row, 0.0, totalRows);
}

bool hasSoloTrack(const Song& song) {
    return std::any_of(song.tracks.begin(), song.tracks.end(), [](const Track& track) {
        return track.solo;
    });
}

bool shouldPlayTrack(const Song& song, int track, bool soloActive) {
    if (track < 0 || track >= static_cast<int>(song.tracks.size())) {
        return false;
    }
    const Track& trackInfo = song.tracks[static_cast<std::size_t>(track)];
    if (trackInfo.muted) {
        return false;
    }
    return !soloActive || trackInfo.solo;
}

std::uint32_t deterministicHash(int patternIndex, int globalRow, int row, int track) {
    std::uint32_t value = 2166136261u;
    value = (value ^ static_cast<std::uint32_t>(patternIndex + 4099)) * 16777619u;
    value = (value ^ static_cast<std::uint32_t>(globalRow + 8191)) * 16777619u;
    value = (value ^ static_cast<std::uint32_t>(row + 131071)) * 16777619u;
    value = (value ^ static_cast<std::uint32_t>(track + 524287)) * 16777619u;
    return value;
}

bool shouldTriggerStep(const PatternStep& step, int patternIndex, int globalRow, int row, int track) {
    if (!step.probability.has_value()) {
        return true;
    }
    const double probability = std::clamp(step.probability.value(), 0.0, 1.0);
    if (probability <= 0.0) {
        return false;
    }
    if (probability >= 1.0) {
        return true;
    }
    const double value = static_cast<double>(deterministicHash(patternIndex, globalRow, row, track) & 0x00ffffffu)
        / static_cast<double>(0x01000000u);
    return value < probability;
}
} // namespace

RealtimePlaybackSession::RealtimePlaybackSession(int sampleRate)
    : sampleRate_(sampleRate), synth_(static_cast<double>(sampleRate)) {
    if (sampleRate_ <= 0) {
        throw std::invalid_argument("sample rate must be positive");
    }
}

void RealtimePlaybackSession::setSong(const Song* song) {
    song_ = song;
    const int trackCount = song_ != nullptr ? static_cast<int>(song_->tracks.size()) : 0;
    if (static_cast<int>(trackFrameCountersScratch_.size()) < trackCount) {
        trackFrameCountersScratch_.resize(static_cast<std::size_t>(trackCount), 0);
        trackFrameStampsScratch_.resize(static_cast<std::size_t>(trackCount), -1);
        trackSegmentCountersScratch_.resize(static_cast<std::size_t>(trackCount), 0);
    }
    rebuildRowMap();
    rebuildPreparedEvents();
    preparedSignature_ = computePreparationSignature();
    signatureCheckCooldownFrames_ = 0;
    playheadRows_ = clampRow(playheadRows_, totalRows());
    syncPreparedEventCursor();
    resetMixBus();
    if (song_ == nullptr) {
        stop();
    }
}

void RealtimePlaybackSession::play() {
    if (song_ == nullptr || totalRows() <= 0.0) {
        return;
    }
    if (playheadRows_ >= totalRows()) {
        seekRows(loop_.enabled ? loop_.startRow : 0.0);
    }
    state_ = TransportState::Playing;
}

void RealtimePlaybackSession::pause() {
    if (state_ == TransportState::Playing) {
        state_ = TransportState::Paused;
    }
}

void RealtimePlaybackSession::stop() {
    state_ = TransportState::Stopped;
    playheadRows_ = loop_.enabled ? loop_.startRow : 0.0;
    syncPreparedEventCursor();
    synth_.reset();
    resetMixBus();
}

void RealtimePlaybackSession::seekRows(double absoluteRow) {
    playheadRows_ = clampRow(absoluteRow, totalRows());
    syncPreparedEventCursor();
    synth_.reset();
    resetMixBus();
}

void RealtimePlaybackSession::setLoopRows(double startRow, double endRow) {
    const double total = totalRows();
    const double start = clampRow(std::min(startRow, endRow), total);
    const double end = clampRow(std::max(startRow, endRow), total);
    if (end <= start) {
        clearLoop();
        return;
    }
    loop_.enabled = true;
    loop_.startRow = start;
    loop_.endRow = end;
    if (playheadRows_ < start || playheadRows_ >= end) {
        seekRows(start);
    }
}

void RealtimePlaybackSession::clearLoop() {
    loop_ = PlaybackLoop {};
}

void RealtimePlaybackSession::setFollowCursor(bool followCursor) {
    followCursor_ = followCursor;
}

bool RealtimePlaybackSession::previewNote(const Note& note, const SynthPatch& patch, double gateSeconds, double pan) {
    return auditionPatch(patch, note.midi, note.velocity, gateSeconds, pan).ok;
}

bool RealtimePlaybackSession::previewInstrument(int instrumentIndex, int midiNote, float velocity, double gateSeconds) {
    return auditionInstrument(instrumentIndex, midiNote, velocity, gateSeconds).ok;
}

bool RealtimePlaybackSession::previewStep(int patternIndex, int row, int track) {
    return auditionStep(patternIndex, row, track).ok;
}

AuditionResult RealtimePlaybackSession::audition(const AuditionRequest& request) {
    AuditionResult result;
    result.request = request;
    if (request.note.midi < 0 || request.note.midi > 127) {
        result.error = "audition MIDI note is outside 0..127";
        return result;
    }
    if (request.gateSeconds <= 0.0) {
        result.error = "audition gate must be positive";
        return result;
    }

    result.request.note.velocity = std::clamp(result.request.note.velocity, 0.0f, 1.0f);
    result.request.gateSeconds = std::max(0.01, request.gateSeconds);
    result.request.pan = std::clamp(request.pan, -1.0, 1.0);
    synth_.noteOn(result.request.note, result.request.patch, result.request.pan, result.request.gateSeconds, -1);
    result.ok = true;
    result.message = result.request.label.empty() ? "Auditioned note" : "Auditioned " + result.request.label;
    return result;
}

AuditionResult RealtimePlaybackSession::auditionPatch(
    const SynthPatch& patch,
    int midiNote,
    float velocity,
    double gateSeconds,
    double pan) {
    AuditionRequest request;
    request.note = Note(midiNote, velocity);
    request.patch = patch;
    request.gateSeconds = gateSeconds;
    request.pan = pan;
    request.label = patch.name.empty() ? "patch" : patch.name;
    return audition(request);
}

AuditionResult RealtimePlaybackSession::auditionDrumPatch(
    const SynthPatch& patch,
    int midiNote,
    float velocity,
    double gateSeconds,
    double pan) {
    AuditionRequest request;
    request.note = Note(midiNote, velocity);
    request.patch = patch;
    request.gateSeconds = gateSeconds;
    request.pan = pan;
    request.label = patch.name.empty() ? "drum patch" : patch.name;
    return audition(request);
}

AuditionResult RealtimePlaybackSession::auditionInstrument(
    int instrumentIndex,
    int midiNote,
    float velocity,
    double gateSeconds) {
    AuditionResult result;
    if (song_ == nullptr) {
        result.error = "no song is loaded";
        return result;
    }
    if (instrumentIndex < 0 || instrumentIndex >= static_cast<int>(song_->instruments.size())) {
        result.error = "instrument index is out of range";
        return result;
    }
    const Instrument& instrument = song_->instruments[static_cast<std::size_t>(instrumentIndex)];
    result.request.note = Note(
        std::clamp(midiNote, 0, 127),
        std::clamp(velocity, 0.0f, 1.0f));
    result.request.patch = instrument.patch;
    // `patch.pan` is already applied inside the synth voice path, so keep audition request pan neutral.
    result.request.pan = 0.0;
    result.request.gateSeconds = std::max(0.01, gateSeconds);
    result.request.label = instrument.patch.name.empty()
        ? "instrument " + std::to_string(instrumentIndex)
        : instrument.patch.name;
    synth_.noteOn(
        result.request.note,
        result.request.patch,
        result.request.pan,
        result.request.gateSeconds,
        instrumentIndex);
    result.ok = true;
    if (result.ok) {
        result.message = "Auditioned " + result.request.label;
    }
    return result;
}

void RealtimePlaybackSession::applyLiveInstrumentWaveformChange(int instrumentIndex, const std::string& oscillator, Waveform waveform) {
    synth_.applyInstrumentWaveformToActiveVoices(instrumentIndex, oscillator, waveform);
}

void RealtimePlaybackSession::applyLiveInstrumentParameterChange(int instrumentIndex, const std::string& parameter, double value) {
    synth_.applyInstrumentParameterToActiveVoices(instrumentIndex, parameter, value);
}

AuditionResult RealtimePlaybackSession::auditionStep(int patternIndex, int row, int track) {
    AuditionResult result;
    if (song_ == nullptr || patternIndex < 0 || patternIndex >= static_cast<int>(song_->patterns.size())) {
        result.error = song_ == nullptr ? "no song is loaded" : "pattern index is out of range";
        return result;
    }
    const Pattern& pattern = song_->patterns[static_cast<std::size_t>(patternIndex)];
    if (row < 0 || row >= pattern.rowCount() || track < 0 || track >= pattern.trackCount()) {
        result.error = "step location is out of range";
        return result;
    }
    const PatternStep& step = pattern.step(row, track);
    if (!step.note.has_value() || step.instrument < 0 || step.instrument >= static_cast<int>(song_->instruments.size())) {
        result.error = !step.note.has_value() ? "step has no note" : "step instrument is out of range";
        return result;
    }
    const Track* trackInfo = track < static_cast<int>(song_->tracks.size())
        ? &song_->tracks[static_cast<std::size_t>(track)]
        : nullptr;
    StepSynthesisState state;
    state.note = *step.note;
    state.patch = song_->instruments[static_cast<std::size_t>(step.instrument)].patch;
    state.gateRows = step.gate;
    state.microOffsetRows = step.microOffsetRows;
    state.pan = trackInfo != nullptr ? trackInfo->pan : 0.0;
    if (!applyStepSynthesisState(step, state) || state.muted) {
        result.error = "step effect configuration is invalid";
        return result;
    }
    if (trackInfo != nullptr) {
        state.patch.gain *= trackInfo->volume;
    }
    AuditionRequest request;
    request.note = state.note;
    request.patch = state.patch;
    request.gateSeconds = state.gateRows * secondsPerRow();
    request.pan = state.pan;
    request.label = "pattern " + std::to_string(patternIndex)
        + " row " + std::to_string(row)
        + " track " + std::to_string(track);
    return audition(request);
}

RenderedAudio RealtimePlaybackSession::renderAuditionClip(
    const AuditionRequest& request,
    double durationSeconds) const {
    const double duration = std::max(0.05, durationSeconds);
    const int totalFrames = std::max(1, static_cast<int>(std::ceil(duration * sampleRate_)));

    RenderedAudio rendered;
    rendered.sampleRate = sampleRate_;
    rendered.interleavedStereo.assign(static_cast<std::size_t>(totalFrames) * 2, 0.0f);

    AuditionRequest sanitized = request;
    sanitized.note.velocity = std::clamp(sanitized.note.velocity, 0.0f, 1.0f);
    sanitized.gateSeconds = std::max(0.01, sanitized.gateSeconds);
    sanitized.pan = std::clamp(sanitized.pan, -1.0, 1.0);

    Synthesizer synth(static_cast<double>(sampleRate_));
    synth.noteOn(sanitized.note, sanitized.patch, sanitized.pan, sanitized.gateSeconds, -1);

    constexpr int blockSize = 128;
    std::vector<float> left(blockSize, 0.0f);
    std::vector<float> right(blockSize, 0.0f);
    for (int frame = 0; frame < totalFrames; frame += blockSize) {
        const int framesThisBlock = std::min(blockSize, totalFrames - frame);
        std::fill(left.begin(), left.begin() + framesThisBlock, 0.0f);
        std::fill(right.begin(), right.begin() + framesThisBlock, 0.0f);
        synth.render(left.data(), right.data(), framesThisBlock);
        for (int index = 0; index < framesThisBlock; ++index) {
            const std::size_t output = static_cast<std::size_t>(frame + index) * 2;
            rendered.interleavedStereo[output] = safetySaturate(left[static_cast<std::size_t>(index)]);
            rendered.interleavedStereo[output + 1] = safetySaturate(right[static_cast<std::size_t>(index)]);
        }
    }
    return rendered;
}

void RealtimePlaybackSession::render(float* left, float* right, int sampleCount) {
    if (sampleCount <= 0) {
        return;
    }
    std::fill(left, left + sampleCount, 0.0f);
    std::fill(right, right + sampleCount, 0.0f);
    if (song_ != nullptr) {
        if (state_ != TransportState::Playing && signatureCheckCooldownFrames_ <= 0) {
            const std::uint64_t signature = computePreparationSignature();
            if (signature != preparedSignature_) {
                rebuildRowMap();
                rebuildPreparedEvents();
                preparedSignature_ = signature;
                syncPreparedEventCursor();
            }
            signatureCheckCooldownFrames_ = std::max(sampleRate_ / 5, 2048);
        } else {
            signatureCheckCooldownFrames_ = std::max(0, signatureCheckCooldownFrames_ - sampleCount);
        }
    }

    int renderedFrames = 0;
    while (renderedFrames < sampleCount) {
        if (state_ != TransportState::Playing || song_ == nullptr || totalRows() <= 0.0) {
            synth_.render(left + renderedFrames, right + renderedFrames, sampleCount - renderedFrames);
            break;
        }

        int framesThisSegment = sampleCount - renderedFrames;
        if (loop_.enabled && playheadRows_ < loop_.endRow) {
            const double rowsUntilLoopEnd = loop_.endRow - playheadRows_;
            const int framesUntilLoopEnd = std::max(1, static_cast<int>(std::ceil(rowsUntilLoopEnd * secondsPerRow() * sampleRate_)));
            framesThisSegment = std::min(framesThisSegment, framesUntilLoopEnd);
        }

        renderSegment(left, right, renderedFrames, framesThisSegment);
        renderedFrames += framesThisSegment;

        if (loop_.enabled && playheadRows_ >= loop_.endRow) {
            playheadRows_ = loop_.startRow;
            syncPreparedEventCursor();
            synth_.reset();
        } else if (playheadRows_ >= totalRows()) {
            state_ = TransportState::Stopped;
            playheadRows_ = totalRows();
            syncPreparedEventCursor();
        }
    }

    applyMixBus(left, right, sampleCount);
}

PlaybackSnapshot RealtimePlaybackSession::snapshot() const {
    PlaybackSnapshot result;
    result.hasSong = song_ != nullptr;
    result.state = state_;
    result.position = currentPosition();
    result.loop = loop_;
    result.followCursor = followCursor_;
    result.previewActive = synth_.active();
    result.sampleRate = sampleRate_;
    result.synth = synth_.telemetry();
    return result;
}

RealtimePlaybackSession::RowLocation RealtimePlaybackSession::locateRow(int absoluteRow) const {
    RowLocation result;
    if (song_ == nullptr || absoluteRow < 0) {
        return result;
    }
    if (absoluteRow >= 0 && absoluteRow < static_cast<int>(rowMap_.size())) {
        return rowMap_[static_cast<std::size_t>(absoluteRow)];
    }
    return result;
}

PlaybackPosition RealtimePlaybackSession::currentPosition() const {
    PlaybackPosition position;
    position.absoluteRow = playheadRows_;
    position.seconds = playheadRows_ * secondsPerRow();

    const RowLocation location = locateRow(static_cast<int>(std::floor(playheadRows_)));
    position.orderIndex = location.orderIndex;
    position.pattern = location.pattern;
    position.patternRow = location.row;
    position.validPattern = location.valid;
    return position;
}

void RealtimePlaybackSession::renderSegment(float* left, float* right, int frameOffset, int frameCount) {
    const double startRow = playheadRows_;
    const double endRow = std::min(totalRows(), startRow + static_cast<double>(frameCount) / (secondsPerRow() * sampleRate_));
    const auto cursorBegin = preparedEvents_.begin()
        + static_cast<std::ptrdiff_t>(std::min(preparedEventCursor_, preparedEvents_.size()));
    const auto end = std::lower_bound(
        cursorBegin,
        preparedEvents_.end(),
        endRow,
        [](const PreparedEvent& event, double row) { return event.row < row; });
    const int eventCountThisSegment = static_cast<int>(std::distance(cursorBegin, end));
    const int trackCountHint = song_ != nullptr ? static_cast<int>(song_->tracks.size()) : 0;
    const double eventDensity = frameCount > 0
        ? static_cast<double>(eventCountThisSegment) / static_cast<double>(frameCount)
        : 0.0;

    const SynthRenderTelemetry synthTelemetry = synth_.telemetry();
    const bool pressureHigh = synthTelemetry.underrunRisk >= 0.30
        || synthTelemetry.dspLoadPercent >= 44.0
        || synthTelemetry.activeVoices >= 12
        || trackCountHint > 8
        || eventDensity >= 0.20;
    const bool pressureCritical = synthTelemetry.underrunRisk >= 0.48
        || synthTelemetry.dspLoadPercent >= 58.0
        || synthTelemetry.activeVoices >= 16
        || trackCountHint > 12
        || eventDensity >= 0.32;
    const bool pressureEmergency = synthTelemetry.underrunRisk >= 0.68
        || synthTelemetry.dspLoadPercent >= 74.0
        || synthTelemetry.activeVoices >= 24
        || trackCountHint > 16
        || eventDensity >= 0.48;
    auto makeLoadSafePatch = [&](const SynthPatch& source) {
        if (!pressureHigh) {
            return source;
        }
        SynthPatch patch = source;
        auto tightenEnvelopes = [&](double releaseScale, double sustainScale) {
            patch.ampEnvelope.attack = std::min(patch.ampEnvelope.attack, 0.01);
            patch.ampEnvelope.decay = std::max(0.001, patch.ampEnvelope.decay * 0.55);
            patch.ampEnvelope.sustain = std::clamp(patch.ampEnvelope.sustain * sustainScale, 0.0, 1.0);
            patch.ampEnvelope.release = std::max(0.001, patch.ampEnvelope.release * releaseScale);
            patch.filterEnvelope.decay = std::max(0.001, patch.filterEnvelope.decay * 0.6);
            patch.filterEnvelope.release = std::max(0.001, patch.filterEnvelope.release * releaseScale);
            patch.transientDecay = std::max(0.001, patch.transientDecay * 0.7);
            patch.outputGlue *= 0.9;
            patch.consoleCrosstalk *= 0.9;
        };
        if (pressureEmergency) {
            patch.oscillatorBEnabled = false;
            patch.oscillatorCEnabled = false;
            patch.oscillatorDEnabled = false;
            patch.unisonVoices = 1;
            patch.unisonDetuneCents *= 0.12;
            patch.subEnabled = false;
            patch.noiseEnabled = false;
            patch.fmEnabled = false;
            patch.fmAmount = 0.0;
            patch.ringEnabled = false;
            patch.ringMod = 0.0;
            patch.hardSyncEnabled = false;
            patch.hardSync = 0.0;
            patch.chorusEnabled = false;
            patch.chorusMix = 0.0;
            patch.chorusEnsemble = 0.0;
            patch.bitCrushEnabled = false;
            patch.bitCrush = 0.0;
            patch.sampleRateReduction = 0.0;
            patch.combMix = 0.0;
            patch.delayMix = 0.0;
            patch.reverbMix = 0.0;
            patch.transientNoise = 0.0;
            patch.transientShape = 0.0;
            patch.transientPitchSemitones = 0.0;
            patch.transientBurstCount = 1;
            patch.click = 0.0;
            patch.chorusTone = 0.45;
            patch.delayDiffusion = 0.0;
            patch.reverbDecay = 0.22;
            patch.reverbEarlyMix = 0.08;
            patch.outputGlue = 0.06;
            patch.consoleCrosstalk = 0.0;
            tightenEnvelopes(0.16, 0.45);
            return patch;
        }
        if (pressureCritical) {
            patch.unisonVoices = std::min(2, std::max(1, patch.unisonVoices));
            patch.unisonDetuneCents *= 0.35;
            patch.fmAmount *= 0.30;
            if (patch.fmAmount < 0.015) {
                patch.fmEnabled = false;
                patch.fmAmount = 0.0;
            }
            patch.ringMod *= 0.25;
            if (patch.ringMod < 0.02) {
                patch.ringEnabled = false;
                patch.ringMod = 0.0;
            }
            patch.hardSync *= 0.35;
            if (patch.hardSync < 0.02) {
                patch.hardSyncEnabled = false;
                patch.hardSync = 0.0;
            }
            patch.chorusMix *= 0.22;
            patch.chorusEnsemble *= 0.25;
            patch.combMix *= 0.22;
            patch.delayMix *= 0.18;
            patch.reverbMix *= 0.14;
            patch.transientNoise *= 0.25;
            patch.transientShape *= 0.25;
            patch.transientBurstCount = std::clamp(patch.transientBurstCount, 1, 2);
            patch.bitCrush *= 0.2;
            patch.sampleRateReduction *= 0.2;
            patch.delayDiffusion *= 0.2;
            patch.reverbDecay *= 0.35;
            patch.reverbEarlyMix *= 0.45;
            patch.outputGlue *= 0.55;
            patch.consoleCrosstalk *= 0.45;
            tightenEnvelopes(0.28, 0.58);
            return patch;
        }
        patch.unisonVoices = std::min(3, std::max(1, patch.unisonVoices));
        patch.unisonDetuneCents *= 0.6;
        patch.fmAmount *= 0.6;
        patch.ringMod *= 0.6;
        patch.hardSync *= 0.6;
        patch.chorusMix *= 0.45;
        patch.chorusEnsemble *= 0.55;
        patch.combMix *= 0.5;
        patch.delayMix *= 0.42;
        patch.reverbMix *= 0.36;
        patch.transientNoise *= 0.6;
        patch.transientShape *= 0.6;
        patch.transientBurstCount = std::clamp(patch.transientBurstCount, 1, 4);
        patch.delayDiffusion *= 0.7;
        patch.reverbDecay *= 0.72;
        patch.reverbEarlyMix *= 0.82;
        patch.outputGlue *= 0.85;
        patch.consoleCrosstalk *= 0.8;
        tightenEnvelopes(0.5, 0.78);
        return patch;
    };

    if (pressureHigh && song_ != nullptr) {
        if (loadSafeInstrumentPatchesScratch_.size() < song_->instruments.size()) {
            loadSafeInstrumentPatchesScratch_.resize(song_->instruments.size());
            loadSafeInstrumentPatchValidScratch_.resize(song_->instruments.size(), 0);
        }
        std::fill_n(
            loadSafeInstrumentPatchValidScratch_.begin(),
            song_->instruments.size(),
            static_cast<unsigned char>(0));
        if (loadSafeOverridePatchesScratch_.size() < preparedPatchOverrides_.size()) {
            loadSafeOverridePatchesScratch_.resize(preparedPatchOverrides_.size());
            loadSafeOverridePatchValidScratch_.resize(preparedPatchOverrides_.size(), 0);
        }
        if (!preparedPatchOverrides_.empty()) {
            std::fill_n(
                loadSafeOverridePatchValidScratch_.begin(),
                preparedPatchOverrides_.size(),
                static_cast<unsigned char>(0));
        }
    }
    auto resolveEventPatch = [&](int instrumentIndex, int patchOverrideIndex) -> const SynthPatch& {
        const bool hasOverride = patchOverrideIndex >= 0
            && patchOverrideIndex < static_cast<int>(preparedPatchOverrides_.size());
        if (!pressureHigh) {
            return hasOverride
                ? preparedPatchOverrides_[static_cast<std::size_t>(patchOverrideIndex)]
                : song_->instruments[static_cast<std::size_t>(instrumentIndex)].patch;
        }
        if (hasOverride) {
            const std::size_t patchIndex = static_cast<std::size_t>(patchOverrideIndex);
            if (patchIndex >= loadSafeOverridePatchesScratch_.size()) {
                return preparedPatchOverrides_[patchIndex];
            }
            if (loadSafeOverridePatchValidScratch_[patchIndex] == 0) {
                loadSafeOverridePatchesScratch_[patchIndex] = makeLoadSafePatch(preparedPatchOverrides_[patchIndex]);
                loadSafeOverridePatchValidScratch_[patchIndex] = 1;
            }
            return loadSafeOverridePatchesScratch_[patchIndex];
        }
        const std::size_t patchIndex = static_cast<std::size_t>(instrumentIndex);
        if (patchIndex >= loadSafeInstrumentPatchesScratch_.size() || patchIndex >= song_->instruments.size()) {
            return song_->instruments.front().patch;
        }
        if (loadSafeInstrumentPatchValidScratch_[patchIndex] == 0) {
            loadSafeInstrumentPatchesScratch_[patchIndex] = makeLoadSafePatch(song_->instruments[patchIndex].patch);
            loadSafeInstrumentPatchValidScratch_[patchIndex] = 1;
        }
        return loadSafeInstrumentPatchesScratch_[patchIndex];
    };

    const double frameScale = secondsPerRow() * static_cast<double>(sampleRate_);
    const auto eventFrame = [&](const PreparedEvent& event) {
        return frameOffset + std::clamp(
            static_cast<int>(std::llround((event.row - startRow) * frameScale)),
            0,
            std::max(0, frameCount - 1));
    };

    int cursor = 0;
    auto nextEvent = cursorBegin;
    int nextEventAbsoluteFrame = nextEvent != end ? eventFrame(*nextEvent) : std::numeric_limits<int>::max();
    // Larger minimum segments reduce render-call overhead and help realtime stability.
    int minRenderChunkSamples = pressureEmergency
        ? 512
        : (pressureCritical ? 320 : (pressureHigh ? 192 : 96));
    int maxEventsPerFrame = pressureEmergency
        ? 1
        : (pressureCritical ? 2 : (pressureHigh ? 3 : 8));
    int maxEventsPerTrackFrame = pressureEmergency
        ? 1
        : (pressureCritical ? 1 : (pressureHigh ? 2 : 3));
    int maxEventsPerSegment = pressureEmergency
        ? std::max(6, frameCount / 96)
        : (pressureCritical
            ? std::max(10, frameCount / 56)
            : (pressureHigh ? std::max(16, frameCount / 36) : std::max(24, frameCount / 24)));
    const double priorityKeepThreshold = pressureEmergency
        ? 0.78
        : (pressureCritical ? 0.60 : (pressureHigh ? 0.42 : -1.0));
    const int trackCount = song_ != nullptr ? static_cast<int>(song_->tracks.size()) : 0;
    if (trackCount > 0 && static_cast<int>(trackFrameCountersScratch_.size()) < trackCount) {
        trackFrameCountersScratch_.resize(static_cast<std::size_t>(trackCount), 0);
        trackFrameStampsScratch_.resize(static_cast<std::size_t>(trackCount), -1);
        trackSegmentCountersScratch_.resize(static_cast<std::size_t>(trackCount), 0);
    }
    if (trackCount > 0) {
        std::fill_n(trackFrameCountersScratch_.begin(), trackCount, 0);
        std::fill_n(trackFrameStampsScratch_.begin(), trackCount, -1);
        std::fill_n(trackSegmentCountersScratch_.begin(), trackCount, 0);
    }
    int maxEventsPerTrackSegment = pressureEmergency
        ? 2
        : (pressureCritical
            ? 4
            : (pressureHigh
                ? std::max(5, maxEventsPerSegment / std::max(1, std::min(trackCount, 6)))
                : std::max(10, maxEventsPerSegment / std::max(1, std::min(trackCount, 8)))));
    if (eventDensity >= 0.35) {
        minRenderChunkSamples = std::max(minRenderChunkSamples, pressureEmergency ? 768 : (pressureCritical ? 448 : 256));
        maxEventsPerFrame = std::min(maxEventsPerFrame, pressureEmergency ? 1 : (pressureCritical ? 1 : 2));
        maxEventsPerTrackFrame = std::min(maxEventsPerTrackFrame, 1);
        maxEventsPerSegment = std::max(6, (maxEventsPerSegment * 3) / 5);
        maxEventsPerTrackSegment = std::max(2, (maxEventsPerTrackSegment * 2) / 3);
    }
    int frameStamp = 0;
    int eventsTriggeredInSegment = 0;
    const double gateScale = pressureEmergency ? 0.18 : (pressureCritical ? 0.32 : (pressureHigh ? 0.62 : 1.0));
    const double gateMinSeconds = pressureEmergency ? 0.006 : (pressureCritical ? 0.010 : 0.018);
    const double gateMaxSeconds = pressureEmergency ? 0.08 : (pressureCritical ? 0.16 : 0.42);
    while (cursor < frameCount) {
        const int absoluteFrame = frameOffset + cursor;
        ++frameStamp;
        int eventsTriggeredAtFrame = 0;
        while (nextEvent != end && nextEventAbsoluteFrame <= absoluteFrame) {
            const int instrumentIndex = nextEvent->instrumentIndex;
            if (eventsTriggeredAtFrame >= maxEventsPerFrame) {
                // Fast-forward dense event bursts scheduled for this same frame.
                const double rowUpper = startRow
                    + (static_cast<double>(absoluteFrame - frameOffset + 1) / frameScale);
                nextEvent = std::lower_bound(
                    nextEvent,
                    end,
                    rowUpper,
                    [](const PreparedEvent& event, double row) {
                        return event.row < row;
                    });
                nextEventAbsoluteFrame = nextEvent != end
                    ? eventFrame(*nextEvent)
                    : std::numeric_limits<int>::max();
                continue;
            }
            const int eventTrack = nextEvent->trackIndex;
            if (eventsTriggeredInSegment >= maxEventsPerSegment && nextEvent->priority < priorityKeepThreshold) {
                ++nextEvent;
                nextEventAbsoluteFrame = nextEvent != end ? eventFrame(*nextEvent) : std::numeric_limits<int>::max();
                continue;
            }
            if (eventTrack >= 0 && eventTrack < trackCount) {
                const std::size_t ti = static_cast<std::size_t>(eventTrack);
                if (trackFrameStampsScratch_[ti] != frameStamp) {
                    trackFrameStampsScratch_[ti] = frameStamp;
                    trackFrameCountersScratch_[ti] = 0;
                }
                if (trackFrameCountersScratch_[ti] >= maxEventsPerTrackFrame) {
                    ++nextEvent;
                    nextEventAbsoluteFrame = nextEvent != end ? eventFrame(*nextEvent) : std::numeric_limits<int>::max();
                    continue;
                }
                if (trackSegmentCountersScratch_[ti] >= maxEventsPerTrackSegment
                    && nextEvent->priority < priorityKeepThreshold) {
                    ++nextEvent;
                    nextEventAbsoluteFrame = nextEvent != end ? eventFrame(*nextEvent) : std::numeric_limits<int>::max();
                    continue;
                }
            }
            if (song_ == nullptr
                || instrumentIndex < 0
                || instrumentIndex >= static_cast<int>(song_->instruments.size())) {
                ++nextEvent;
                nextEventAbsoluteFrame = nextEvent != end ? eventFrame(*nextEvent) : std::numeric_limits<int>::max();
                continue;
            }
            const int patchOverrideIndex = nextEvent->patchOverrideIndex;
            const SynthPatch& patch = resolveEventPatch(instrumentIndex, patchOverrideIndex);
            const double safeGateSeconds = std::clamp(
                nextEvent->gateSeconds * gateScale,
                gateMinSeconds,
                gateMaxSeconds);
            synth_.noteOn(
                nextEvent->note,
                patch,
                nextEvent->pan,
                safeGateSeconds,
                instrumentIndex);
            ++eventsTriggeredAtFrame;
            ++eventsTriggeredInSegment;
            if (eventTrack >= 0 && eventTrack < trackCount) {
                const std::size_t ti = static_cast<std::size_t>(eventTrack);
                ++trackFrameCountersScratch_[ti];
                ++trackSegmentCountersScratch_[ti];
            }
            ++nextEvent;
            nextEventAbsoluteFrame = nextEvent != end ? eventFrame(*nextEvent) : std::numeric_limits<int>::max();
        }

        int segmentEnd = frameCount;
        if (nextEvent != end) {
            const int nextBoundary = std::max(cursor + 1, nextEventAbsoluteFrame - frameOffset);
            if (nextBoundary - cursor < minRenderChunkSamples && nextBoundary < frameCount) {
                segmentEnd = std::min(frameCount, cursor + minRenderChunkSamples);
            } else {
                segmentEnd = std::min(segmentEnd, nextBoundary);
            }
        }
        synth_.render(left + frameOffset + cursor, right + frameOffset + cursor, segmentEnd - cursor);
        cursor = segmentEnd;
    }

    preparedEventCursor_ = static_cast<std::size_t>(std::distance(preparedEvents_.begin(), nextEvent));
    playheadRows_ = endRow;
}

void RealtimePlaybackSession::rebuildRowMap() {
    rowMap_.clear();
    if (song_ == nullptr) {
        return;
    }
    int startRow = 0;
    for (std::size_t orderIndex = 0; orderIndex < song_->order.size(); ++orderIndex) {
        const int patternIndex = song_->order[orderIndex];
        if (patternIndex < 0 || patternIndex >= static_cast<int>(song_->patterns.size())) {
            continue;
        }
        const Pattern& pattern = song_->patterns[static_cast<std::size_t>(patternIndex)];
        for (int row = 0; row < pattern.rowCount(); ++row) {
            RowLocation location;
            location.orderIndex = static_cast<int>(orderIndex);
            location.pattern = patternIndex;
            location.row = row;
            location.patternStartRow = startRow;
            location.valid = true;
            rowMap_.push_back(location);
        }
        startRow += pattern.rowCount();
    }
}

void RealtimePlaybackSession::rebuildPreparedEvents() {
    preparedEvents_.clear();
    preparedPatchOverrides_.clear();
    if (song_ == nullptr) {
        return;
    }
    const bool soloActive = hasSoloTrack(*song_);
    const double rowDuration = secondsPerRow();
    const double rowsPerSecond = rowDuration > 0.0 ? (1.0 / rowDuration) : 0.0;
    const double minRetriggerSpacingRows = rowsPerSecond > 0.0
        ? (64.0 / (static_cast<double>(sampleRate_) * rowDuration))
        : 0.001;
    const double total = totalRows();
    const int trackCount = static_cast<int>(song_->tracks.size());
    const int maxEventsPerSourceRow = std::clamp(trackCount + (trackCount / 2), 8, 32);
    std::size_t estimated = 0;
    for (const RowLocation& location : rowMap_) {
        if (location.valid) {
            estimated += static_cast<std::size_t>(trackCount);
        }
    }
    preparedEvents_.reserve(estimated);
    preparedPatchOverrides_.reserve(std::min<std::size_t>(estimated, 2048));

    for (int absoluteRow = 0; absoluteRow < static_cast<int>(rowMap_.size()); ++absoluteRow) {
        const RowLocation& location = rowMap_[static_cast<std::size_t>(absoluteRow)];
        if (!location.valid
            || location.pattern < 0
            || location.pattern >= static_cast<int>(song_->patterns.size())) {
            continue;
        }
        const Pattern& pattern = song_->patterns[static_cast<std::size_t>(location.pattern)];
        std::vector<PreparedEvent> rowEvents;
        rowEvents.reserve(static_cast<std::size_t>(std::max(8, std::min(pattern.trackCount() * 2, 96))));
        for (int track = 0; track < pattern.trackCount(); ++track) {
            if (!shouldPlayTrack(*song_, track, soloActive)) {
                continue;
            }
            const PatternStep& step = pattern.step(location.row, track);
            if (!step.note.has_value() || step.instrument < 0 || step.instrument >= static_cast<int>(song_->instruments.size())) {
                continue;
            }
            if (!shouldTriggerStep(step, location.pattern, absoluteRow, location.row, track)) {
                continue;
            }
            const Track* trackInfo = track < static_cast<int>(song_->tracks.size())
                ? &song_->tracks[static_cast<std::size_t>(track)]
                : nullptr;
            StepSynthesisState state;
            state.note = *step.note;
            state.patch = song_->instruments[static_cast<std::size_t>(step.instrument)].patch;
            state.gateRows = step.gate;
            state.microOffsetRows = step.microOffsetRows;
            state.pan = trackInfo != nullptr ? trackInfo->pan : 0.0;
            if (!applyStepSynthesisState(step, state) || state.muted) {
                continue;
            }
            if (trackInfo != nullptr) {
                state.patch.gain *= trackInfo->volume;
            }
            const int retriggerCount = std::clamp(step.retriggerCount, 1, 4);
            const double spacingRows = std::max(minRetriggerSpacingRows, step.retriggerSpacingRows);
            const double baseVelocity = std::clamp(static_cast<double>(state.note.velocity), 0.0, 1.0);
            const double trackWeight = trackInfo != nullptr
                ? std::clamp(trackInfo->volume, 0.05, 2.0)
                : 1.0;
            const double bassWeight = state.note.midi <= 52 ? 1.18 : 1.0;
            const double gateWeight = std::clamp(state.gateRows, 0.05, 1.25);
            const double priorityBase = baseVelocity * trackWeight * bassWeight * gateWeight;
            double velocityScale = 1.0;
            for (int repeat = 0; repeat < retriggerCount; ++repeat) {
                if (rowEvents.size() >= static_cast<std::size_t>(maxEventsPerSourceRow * 4)) {
                    break;
                }
                const double eventRow = static_cast<double>(absoluteRow)
                    + state.microOffsetRows
                    + static_cast<double>(repeat) * spacingRows;
                if (eventRow < 0.0 || eventRow >= total) {
                    velocityScale *= std::clamp(step.retriggerVelocityDecay, 0.0, 1.0);
                    continue;
                }
                Note note = state.note;
                note.velocity = static_cast<float>(std::clamp(
                    static_cast<double>(note.velocity) * velocityScale,
                    0.0,
                    1.0));
                const double gateRows = retriggerCount > 1
                    ? std::min(state.gateRows, spacingRows * 0.85)
                    : state.gateRows;
                int patchOverrideIndex = -1;
                if (!step.automation.empty() || !step.effects.empty()) {
                    patchOverrideIndex = static_cast<int>(preparedPatchOverrides_.size());
                    preparedPatchOverrides_.push_back(state.patch);
                }
                rowEvents.push_back({
                    eventRow,
                    note,
                    step.instrument,
                    track,
                    patchOverrideIndex,
                    state.pan,
                    gateRows * rowDuration,
                    priorityBase * velocityScale * (repeat == 0 ? 1.0 : 0.72)
                });
                velocityScale *= std::clamp(step.retriggerVelocityDecay, 0.0, 1.0);
            }
        }
        if (!rowEvents.empty()) {
            std::sort(rowEvents.begin(), rowEvents.end(), [](const PreparedEvent& left, const PreparedEvent& right) {
                if (left.priority != right.priority) {
                    return left.priority > right.priority;
                }
                if (left.row != right.row) {
                    return left.row < right.row;
                }
                return left.note.midi < right.note.midi;
            });
            if (rowEvents.size() > static_cast<std::size_t>(maxEventsPerSourceRow)) {
                rowEvents.resize(static_cast<std::size_t>(maxEventsPerSourceRow));
            }
            preparedEvents_.insert(preparedEvents_.end(), rowEvents.begin(), rowEvents.end());
        }
    }

    std::sort(preparedEvents_.begin(), preparedEvents_.end(), [](const PreparedEvent& left, const PreparedEvent& right) {
        if (left.row != right.row) {
            return left.row < right.row;
        }
        if (left.priority != right.priority) {
            return left.priority > right.priority;
        }
        return left.note.midi < right.note.midi;
    });
    preparedEventCursor_ = 0;
}

std::uint64_t RealtimePlaybackSession::computePreparationSignature() const {
    if (song_ == nullptr) {
        return 0;
    }
    std::uint64_t value = 1469598103934665603ull;
    auto mix = [&](std::uint64_t part) {
        value ^= part;
        value *= 1099511628211ull;
    };
    mix(static_cast<std::uint64_t>(song_->order.size()));
    mix(static_cast<std::uint64_t>(song_->patterns.size()));
    mix(static_cast<std::uint64_t>(song_->tracks.size()));
    mix(static_cast<std::uint64_t>(song_->instruments.size()));
    mix(static_cast<std::uint64_t>(song_->totalRows()));
    for (const Track& track : song_->tracks) {
        const std::uint64_t volumeBits = static_cast<std::uint64_t>(std::llround(std::clamp(track.volume, 0.0, 4.0) * 1000.0));
        mix(volumeBits);
        mix(track.muted ? 1ull : 0ull);
        mix(track.solo ? 1ull : 0ull);
    }
    return value;
}

void RealtimePlaybackSession::syncPreparedEventCursor() {
    if (preparedEvents_.empty()) {
        preparedEventCursor_ = 0;
        return;
    }
    const auto it = std::lower_bound(
        preparedEvents_.begin(),
        preparedEvents_.end(),
        playheadRows_,
        [](const PreparedEvent& event, double row) { return event.row < row; });
    preparedEventCursor_ = static_cast<std::size_t>(std::distance(preparedEvents_.begin(), it));
}

void RealtimePlaybackSession::resetMixBus() {
    mixBus_ = MixBusState {};
}

void RealtimePlaybackSession::applyMixBus(float* left, float* right, int sampleCount) {
    if (sampleCount <= 0) {
        return;
    }
    const double inputTrim = playbackHeadroomGain();
    constexpr double limiterThreshold = 0.80;
    const double attackCoeff = std::exp(-1.0 / (sampleRate_ * 0.00018));
    const double releaseCoeff = std::exp(-1.0 / (sampleRate_ * 0.14));

    for (int sample = 0; sample < sampleCount; ++sample) {
        double inLeft = static_cast<double>(left[sample]) * inputTrim;
        double inRight = static_cast<double>(right[sample]) * inputTrim;
        const double peak = std::max(std::abs(inLeft), std::abs(inRight));
        const double targetGain = peak > limiterThreshold
            ? limiterThreshold / std::max(peak, 1e-9)
            : 1.0;
        const double coeff = targetGain < mixBus_.gain ? attackCoeff : releaseCoeff;
        mixBus_.gain = approach(mixBus_.gain, targetGain, coeff);
        inLeft *= mixBus_.gain;
        inRight *= mixBus_.gain;
        inLeft = dcBlock(inLeft, mixBus_.dcInputLeft, mixBus_.dcOutputLeft);
        inRight = dcBlock(inRight, mixBus_.dcInputRight, mixBus_.dcOutputRight);
        const double busPeak = std::max(std::abs(inLeft), std::abs(inRight));
        if (busPeak > 0.84) {
            const double busTrim = 0.84 / std::max(busPeak, 1e-9);
            inLeft *= busTrim;
            inRight *= busTrim;
        }
        inLeft = std::tanh(inLeft * 0.92) / 0.92;
        inRight = std::tanh(inRight * 0.92) / 0.92;
        left[sample] = safetySaturate(inLeft);
        right[sample] = safetySaturate(inRight);
    }
}

double RealtimePlaybackSession::playbackHeadroomGain() const {
    if (song_ == nullptr || song_->tracks.empty()) {
        return 1.0;
    }
    int audibleTracks = 0;
    const bool soloActive = hasSoloTrack(*song_);
    for (int track = 0; track < static_cast<int>(song_->tracks.size()); ++track) {
        if (shouldPlayTrack(*song_, track, soloActive)) {
            ++audibleTracks;
        }
    }
    const double trackCount = std::max(1.0, static_cast<double>(audibleTracks));
    const double effectiveTracks = 1.0 + (trackCount - 1.0) * 0.24;
    return 1.0 / std::sqrt(effectiveTracks);
}

double RealtimePlaybackSession::totalRows() const {
    return static_cast<double>(rowMap_.size());
}

double RealtimePlaybackSession::secondsPerRow() const {
    return song_ == nullptr ? 0.0 : song_->secondsPerRow();
}

const char* transportStateName(TransportState state) {
    switch (state) {
        case TransportState::Stopped:
            return "stopped";
        case TransportState::Playing:
            return "playing";
        case TransportState::Paused:
            return "paused";
    }
    return "unknown";
}

} // namespace arachno
