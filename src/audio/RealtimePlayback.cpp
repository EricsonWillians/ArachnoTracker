#include "RealtimePlayback.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
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
    const double softened = threshold + std::tanh(excess * 8.0) * 0.02;
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
    rebuildRowMap();
    rebuildPreparedEvents();
    preparedSignature_ = computePreparationSignature();
    playheadRows_ = clampRow(playheadRows_, totalRows());
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
    synth_.reset();
    resetMixBus();
}

void RealtimePlaybackSession::seekRows(double absoluteRow) {
    playheadRows_ = clampRow(absoluteRow, totalRows());
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
    synth_.noteOn(result.request.note, result.request.patch, result.request.pan, result.request.gateSeconds);
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
    result = auditionPatch(instrument.patch, midiNote, velocity, gateSeconds, instrument.patch.pan);
    result.request.label = instrument.patch.name.empty()
        ? "instrument " + std::to_string(instrumentIndex)
        : instrument.patch.name;
    if (result.ok) {
        result.message = "Auditioned " + result.request.label;
    }
    return result;
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
    synth.noteOn(sanitized.note, sanitized.patch, sanitized.pan, sanitized.gateSeconds);

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
        const std::uint64_t signature = computePreparationSignature();
        if (signature != preparedSignature_) {
            rebuildRowMap();
            rebuildPreparedEvents();
            preparedSignature_ = signature;
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
            synth_.reset();
        } else if (playheadRows_ >= totalRows()) {
            state_ = TransportState::Stopped;
            playheadRows_ = totalRows();
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

    scratchEvents_.clear();
    if (!preparedEvents_.empty()) {
        const auto begin = std::lower_bound(
            preparedEvents_.begin(),
            preparedEvents_.end(),
            startRow,
            [](const PreparedEvent& event, double row) { return event.row < row; });
        const auto end = std::lower_bound(
            begin,
            preparedEvents_.end(),
            endRow,
            [](const PreparedEvent& event, double row) { return event.row < row; });
        scratchEvents_.reserve(static_cast<std::size_t>(std::distance(begin, end)));
        const double rowDuration = secondsPerRow();
        for (auto it = begin; it != end; ++it) {
            const int frame = frameOffset + std::clamp(
                static_cast<int>(std::llround((it->row - startRow) * rowDuration * sampleRate_)),
                0,
                std::max(0, frameCount - 1));
            scratchEvents_.push_back({
                frame,
                it->note,
                it->patch,
                it->pan,
                it->gateSeconds
            });
        }
    }

    int cursor = 0;
    std::size_t nextEvent = 0;
    constexpr int minRenderChunkSamples = 8;
    while (cursor < frameCount) {
        const int absoluteFrame = frameOffset + cursor;
        while (nextEvent < scratchEvents_.size() && scratchEvents_[nextEvent].frame <= absoluteFrame) {
            synth_.noteOn(
                scratchEvents_[nextEvent].note,
                scratchEvents_[nextEvent].patch,
                scratchEvents_[nextEvent].pan,
                scratchEvents_[nextEvent].gateSeconds);
            ++nextEvent;
        }

        int segmentEnd = frameCount;
        if (nextEvent < scratchEvents_.size()) {
            const int nextBoundary = std::max(cursor + 1, scratchEvents_[nextEvent].frame - frameOffset);
            if (nextBoundary - cursor < minRenderChunkSamples && nextBoundary < frameCount) {
                segmentEnd = std::min(frameCount, cursor + minRenderChunkSamples);
            } else {
                segmentEnd = std::min(segmentEnd, nextBoundary);
            }
        }
        synth_.render(left + frameOffset + cursor, right + frameOffset + cursor, segmentEnd - cursor);
        cursor = segmentEnd;
    }

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
    if (song_ == nullptr) {
        return;
    }
    const bool soloActive = hasSoloTrack(*song_);
    const double rowDuration = secondsPerRow();
    const double total = totalRows();
    const int trackCount = static_cast<int>(song_->tracks.size());
    std::size_t estimated = 0;
    for (const RowLocation& location : rowMap_) {
        if (location.valid) {
            estimated += static_cast<std::size_t>(trackCount);
        }
    }
    preparedEvents_.reserve(estimated);

    for (int absoluteRow = 0; absoluteRow < static_cast<int>(rowMap_.size()); ++absoluteRow) {
        const RowLocation& location = rowMap_[static_cast<std::size_t>(absoluteRow)];
        if (!location.valid
            || location.pattern < 0
            || location.pattern >= static_cast<int>(song_->patterns.size())) {
            continue;
        }
        const Pattern& pattern = song_->patterns[static_cast<std::size_t>(location.pattern)];
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
            const int retriggerCount = std::max(1, step.retriggerCount);
            const double spacingRows = std::max(0.001, step.retriggerSpacingRows);
            double velocityScale = 1.0;
            for (int repeat = 0; repeat < retriggerCount; ++repeat) {
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
                preparedEvents_.push_back({
                    eventRow,
                    note,
                    state.patch,
                    state.pan,
                    gateRows * rowDuration
                });
                velocityScale *= std::clamp(step.retriggerVelocityDecay, 0.0, 1.0);
            }
        }
    }

    std::sort(preparedEvents_.begin(), preparedEvents_.end(), [](const PreparedEvent& left, const PreparedEvent& right) {
        if (left.row != right.row) {
            return left.row < right.row;
        }
        return left.note.midi < right.note.midi;
    });
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

void RealtimePlaybackSession::resetMixBus() {
    mixBus_ = MixBusState {};
}

void RealtimePlaybackSession::applyMixBus(float* left, float* right, int sampleCount) {
    if (sampleCount <= 0) {
        return;
    }
    const double inputTrim = playbackHeadroomGain();
    constexpr double limiterThreshold = 0.93;
    const double attackCoeff = std::exp(-1.0 / (sampleRate_ * 0.0006));
    const double releaseCoeff = std::exp(-1.0 / (sampleRate_ * 0.12));

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
    const double effectiveTracks = 1.0 + (trackCount - 1.0) * 0.18;
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
