#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
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
    // When true the voice sustains at its envelope sustain level until auditionNoteOff().
    bool sustainUntilNoteOff = false;
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
        double pan = 0.0,
        bool sustainUntilNoteOff = false);
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
        double gateSeconds = 0.35,
        bool sustainUntilNoteOff = false);
    // Releases a sustained audition voice. instrumentIndex < 0 targets patch-preview voices.
    void auditionNoteOff(int midiNote, int instrumentIndex = -1);
    // Releases every gated voice (negative instrumentIndex = all voices).
    void releaseAllNotes(int instrumentIndex = -2);
    void applyLiveInstrumentWaveformChange(int instrumentIndex, const std::string& oscillator, Waveform waveform);
    void applyLiveInstrumentParameterChange(int instrumentIndex, const std::string& parameter, double value);
    AuditionResult auditionStep(int patternIndex, int row, int track);
    RenderedAudio renderAuditionClip(const AuditionRequest& request, double durationSeconds = 1.0) const;

    void render(float* left, float* right, int sampleCount);
    PlaybackSnapshot snapshot() const;

    // Single shared audio-state mutex (recursive, no nested third-party locks):
    // serializes render blocks (dedicated producer thread) against transport,
    // audition, and song-mutation calls (GUI thread). ApplicationSession exposes
    // it as audioStateMutex(); never hold it across waits or redraws.
    std::recursive_mutex& apiMutex() const { return apiMutex_; }

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
        // Note-off event: releases the last note started on this track.
        bool noteOff = false;
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

    // Written under apiMutex_ (GUI thread). The lock-free snapshot() never
    // dereferences it — it consults the hasSong_ flag and lookupSnapshot_.
    const Song* song_ = nullptr;
    std::atomic<bool> hasSong_ {false};
    mutable std::recursive_mutex apiMutex_;
    // Tiny side mutex for composite loop state read by the lock-free snapshot.
    // Never nested with apiMutex_ (taken either inside it or alone).
    mutable std::mutex stateMutex_;
    int sampleRate_ = 48000;
    std::atomic<TransportState> state_ {TransportState::Stopped};
    PlaybackLoop loop_;
    std::atomic<bool> followCursor_ {true};
    std::atomic<double> playheadRows_ {0.0};
    Synthesizer synth_;
    MixBusState mixBus_;
    std::vector<RowLocation> rowMap_;
    // Published read-only lookup for the lock-free snapshot() path (swapped
    // atomically after rebuildRowMap under apiMutex_). Bundles the row map with
    // secondsPerRow so snapshot never dereferences song_ off-lock.
    struct PublishedLookup {
        double secondsPerRow = 0.0;
        std::vector<RowLocation> rows;
    };
    std::shared_ptr<const PublishedLookup> lookupSnapshot_;
    std::vector<PreparedEvent> preparedEvents_;
    std::vector<SynthPatch> preparedPatchOverrides_;
    // Reused per-segment bookkeeping to avoid allocations on the realtime audio path.
    std::vector<int> trackFrameCountersScratch_;
    std::vector<int> trackFrameStampsScratch_;
    std::vector<int> trackSegmentCountersScratch_;
    // Last started note per track for note-off steps; persists across render segments.
    std::vector<int> lastNoteForTrackScratch_;
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
