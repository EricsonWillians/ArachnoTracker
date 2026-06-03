#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "AudioEngine.h"
#include "Synthesizer.h"
#include "Tracker.h"

namespace arachno {

enum class TransportState {
    Stopped,
    Playing,
    Paused
};

struct PlaybackLoop {
    bool enabled = false;
    double startRow = 0.0;
    double endRow = 0.0;
};

struct PlaybackPosition {
    double absoluteRow = 0.0;
    double seconds = 0.0;
    int orderIndex = -1;
    int pattern = -1;
    int patternRow = 0;
    bool validPattern = false;
};

struct PlaybackSnapshot {
    bool hasSong = false;
    TransportState state = TransportState::Stopped;
    PlaybackPosition position;
    PlaybackLoop loop;
    bool followCursor = true;
    bool previewActive = false;
    int sampleRate = 48000;
    SynthRenderTelemetry synth;
};

struct AuditionRequest {
    Note note;
    SynthPatch patch;
    double gateSeconds = 0.35;
    double pan = 0.0;
    std::string label;
};

struct AuditionResult {
    bool ok = false;
    std::string message;
    std::string error;
    AuditionRequest request;
};

class RealtimePlaybackSession {
public:
    explicit RealtimePlaybackSession(int sampleRate = 48000);

    void setSong(const Song* song);
    const Song* song() const { return song_; }

    void play();
    void pause();
    void stop();
    void seekRows(double absoluteRow);
    void setLoopRows(double startRow, double endRow);
    void clearLoop();
    void setFollowCursor(bool followCursor);

    bool previewNote(const Note& note, const SynthPatch& patch, double gateSeconds = 0.35, double pan = 0.0);
    bool previewInstrument(int instrumentIndex, int midiNote = 60, float velocity = 0.8f, double gateSeconds = 0.35);
    bool previewStep(int patternIndex, int row, int track);
    AuditionResult audition(const AuditionRequest& request);
    AuditionResult auditionPatch(
        const SynthPatch& patch,
        int midiNote = 60,
        float velocity = 0.8f,
        double gateSeconds = 0.35,
        double pan = 0.0);
    AuditionResult auditionDrumPatch(
        const SynthPatch& patch,
        int midiNote = 36,
        float velocity = 0.95f,
        double gateSeconds = 0.2,
        double pan = 0.0);
    AuditionResult auditionInstrument(
        int instrumentIndex,
        int midiNote = 60,
        float velocity = 0.8f,
        double gateSeconds = 0.35);
    void applyLiveInstrumentWaveformChange(int instrumentIndex, const std::string& oscillator, Waveform waveform);
    void applyLiveInstrumentParameterChange(int instrumentIndex, const std::string& parameter, double value);
    AuditionResult auditionStep(int patternIndex, int row, int track);
    RenderedAudio renderAuditionClip(const AuditionRequest& request, double durationSeconds = 1.0) const;

    void render(float* left, float* right, int sampleCount);
    PlaybackSnapshot snapshot() const;

private:
    struct MixBusState {
        double gain = 1.0;
        double dcInputLeft = 0.0;
        double dcOutputLeft = 0.0;
        double dcInputRight = 0.0;
        double dcOutputRight = 0.0;
    };

    struct RowLocation {
        int orderIndex = -1;
        int pattern = -1;
        int row = 0;
        int patternStartRow = 0;
        bool valid = false;
    };

    struct PreparedEvent {
        double row = 0.0;
        Note note;
        int instrumentIndex = -1;
        int trackIndex = -1;
        int patchOverrideIndex = -1;
        double pan = 0.0;
        double gateSeconds = 0.2;
        double priority = 0.0;
    };

    RowLocation locateRow(int absoluteRow) const;
    PlaybackPosition currentPosition() const;
    void renderSegment(float* left, float* right, int frameOffset, int frameCount);
    void rebuildRowMap();
    void rebuildPreparedEvents();
    std::uint64_t computePreparationSignature() const;
    void syncPreparedEventCursor();
    void resetMixBus();
    void applyMixBus(float* left, float* right, int sampleCount);
    double playbackHeadroomGain() const;
    double totalRows() const;
    double secondsPerRow() const;

    const Song* song_ = nullptr;
    int sampleRate_ = 48000;
    TransportState state_ = TransportState::Stopped;
    PlaybackLoop loop_;
    bool followCursor_ = true;
    double playheadRows_ = 0.0;
    Synthesizer synth_;
    MixBusState mixBus_;
    std::vector<RowLocation> rowMap_;
    std::vector<PreparedEvent> preparedEvents_;
    std::vector<SynthPatch> preparedPatchOverrides_;
    // Reused per-segment bookkeeping to avoid allocations on the realtime audio path.
    std::vector<int> trackFrameCountersScratch_;
    std::vector<int> trackFrameStampsScratch_;
    std::vector<int> trackSegmentCountersScratch_;
    std::vector<SynthPatch> loadSafeInstrumentPatchesScratch_;
    std::vector<unsigned char> loadSafeInstrumentPatchValidScratch_;
    std::vector<SynthPatch> loadSafeOverridePatchesScratch_;
    std::vector<unsigned char> loadSafeOverridePatchValidScratch_;
    std::size_t preparedEventCursor_ = 0;
    std::uint64_t preparedSignature_ = 0;
    int signatureCheckCooldownFrames_ = 0;
};

const char* transportStateName(TransportState state);

} // namespace arachno
