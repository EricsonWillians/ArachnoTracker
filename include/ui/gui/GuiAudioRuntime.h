#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifndef ARACHNO_HAS_ALSA
#define ARACHNO_HAS_ALSA 0
#endif

#if ARACHNO_HAS_ALSA
#include <alsa/asoundlib.h>
#endif

namespace arachno {

enum class AudioPerformanceMode {
    Auto,
    Live,
    Balanced,
    Heavy,
    Custom
};

const char* audioPerformanceModeLabel(AudioPerformanceMode mode);

class GuiAudioRuntime {
public:
    GuiAudioRuntime() = default;
    ~GuiAudioRuntime();

    GuiAudioRuntime(const GuiAudioRuntime&) = delete;
    GuiAudioRuntime& operator=(const GuiAudioRuntime&) = delete;

    void close();
    bool open(int sampleRate);
    bool write(const float* left, const float* right, int frames);
    void tuneForLoad(int sampleRate, int audibleTracks);
    void setPerformanceMode(AudioPerformanceMode mode, int sampleRate, int audibleTracks);
    void adjustCustomLevel(int delta, int sampleRate, int audibleTracks);
    void setCustomLevel(int level, int sampleRate, int audibleTracks);

    bool hasOutput() const;
    bool usesAlsa() const;
    AudioPerformanceMode performanceMode() const;
    int customLevel() const;
    int frameMin() const;
    int frameMax() const;
    int loadClass() const;
    std::pair<std::size_t, std::size_t> alsaQueueUsage();

private:
#if ARACHNO_HAS_ALSA
    void resetAlsaQueue(int sampleRate);
    void tuneAlsaQueueForLoad(int sampleRate, int loadClass);
    bool enqueueAlsaFrames(const float* left, const float* right, int frames);
    bool startAlsaWriterThread(int sampleRate);
#endif

    FILE* audioPipe_ = nullptr;
    bool audioOutputUsesAlsa_ = false;
    bool audioPipeUsingFloat_ = false;
    std::vector<float> audioInterleavedFloat_;
    std::vector<short> audioInterleavedS16_;
    std::vector<char> audioPipeBuffer_;
    int audioFrameMin_ = 536;
    int audioFrameMax_ = 3216;
    int audioLoadClass_ = 2;
    AudioPerformanceMode audioPerformanceMode_ = AudioPerformanceMode::Heavy;
    int audioCustomLevel_ = 0;
    std::chrono::steady_clock::time_point lastAudioTuning_ {};

#if ARACHNO_HAS_ALSA
    snd_pcm_t* audioPcm_ = nullptr;
    bool audioPcmUsingFloat_ = false;
    std::thread audioAlsaWriterThread_;
    std::mutex audioAlsaMutex_;
    std::condition_variable audioAlsaCv_;
    std::vector<float> audioAlsaQueue_;
    std::size_t audioAlsaQueueCapacity_ = 0;
    std::size_t audioAlsaQueueRead_ = 0;
    std::size_t audioAlsaQueueSize_ = 0;
    bool audioAlsaThreadRunning_ = false;
    bool audioAlsaThreadStop_ = false;
    bool audioAlsaHealthy_ = true;
    std::size_t audioAlsaStartThresholdSamples_ = 0;
    std::size_t audioAlsaChunkSamples_ = 0;
#endif
};

} // namespace arachno
