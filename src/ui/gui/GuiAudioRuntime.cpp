#include "ui/gui/GuiAudioRuntime.h"

#include <algorithm>
#include <cctype>
#include <csignal>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <sstream>
#if defined(__linux__)
#include <pthread.h>
#endif

namespace arachno {

namespace {

void ignoreSigpipeForAudioPipes() {
    static bool configured = false;
    if (!configured) {
        std::signal(SIGPIPE, SIG_IGN);
        configured = true;
    }
}

} // namespace

const char* audioPerformanceModeLabel(AudioPerformanceMode mode) {
    switch (mode) {
        case AudioPerformanceMode::Auto:
            return "AUTO";
        case AudioPerformanceMode::Live:
            return "LIVE";
        case AudioPerformanceMode::Balanced:
            return "BAL";
        case AudioPerformanceMode::Heavy:
            return "HEAVY";
        case AudioPerformanceMode::Custom:
            return "CUSTOM";
    }
    return "AUTO";
}

GuiAudioRuntime::~GuiAudioRuntime() {
    close();
}

void GuiAudioRuntime::tuneForLoad(int sampleRate, int audibleTracks) {
    int loadClass = 0;
    int frameMin = 384;
    int frameMax = 3072;
    if (audioPerformanceMode_ == AudioPerformanceMode::Auto) {
        if (audibleTracks > 28) {
            loadClass = 2;
        } else if (audibleTracks > 12) {
            loadClass = 1;
        }
        if (loadClass <= 0) {
            frameMin = 512;
            frameMax = 4096;
        } else if (loadClass == 1) {
            frameMin = 768;
            frameMax = 6144;
        } else {
            frameMin = 1024;
            frameMax = 8192;
        }
    } else if (audioPerformanceMode_ == AudioPerformanceMode::Live) {
        loadClass = 0;
        frameMin = 96;
        frameMax = 1024;
    } else if (audioPerformanceMode_ == AudioPerformanceMode::Balanced) {
        loadClass = 1;
        frameMin = 512;
        frameMax = 3072;
    } else if (audioPerformanceMode_ == AudioPerformanceMode::Heavy) {
        loadClass = 2;
        frameMin = 768;
        frameMax = 4608;
    } else {
        const int level = std::clamp(audioCustomLevel_, -24, 4096);
        if (level < 0) {
            frameMin = std::max(32, 96 + (level * 2));
            frameMax = std::clamp(frameMin * 6, 256, 1280);
            loadClass = 0;
        } else if (level <= 128) {
            frameMin = 96 + (level * 2);
            frameMax = std::clamp(frameMin * 6, 512, 4096);
            loadClass = level <= 48 ? 0 : 1;
        } else if (level <= 1024) {
            frameMin = 384 + ((level - 128) * 3);
            frameMax = std::clamp(frameMin * 6, 1024, 12288);
            loadClass = level <= 320 ? 1 : 2;
        } else {
            frameMin = 3008 + ((level - 1024) * 4);
            frameMax = std::clamp(frameMin * 6, 2048, 32768);
            loadClass = 2;
        }
    }
    if (loadClass == audioLoadClass_
        && frameMin == audioFrameMin_
        && frameMax == audioFrameMax_
        && std::chrono::steady_clock::now() - lastAudioTuning_ < std::chrono::milliseconds(180)) {
        return;
    }
    audioLoadClass_ = loadClass;
    audioFrameMin_ = frameMin;
    audioFrameMax_ = frameMax;
    lastAudioTuning_ = std::chrono::steady_clock::now();
#if ARACHNO_HAS_ALSA
    if (audioOutputUsesAlsa_ && audioPcm_ != nullptr) {
        tuneAlsaQueueForLoad(sampleRate, audioLoadClass_);
        audioAlsaCv_.notify_one();
    }
#else
    (void)sampleRate;
#endif
}

void GuiAudioRuntime::setPerformanceMode(AudioPerformanceMode mode, int sampleRate, int audibleTracks) {
    if (audioPerformanceMode_ == mode) {
        if (mode == AudioPerformanceMode::Custom) {
            tuneForLoad(sampleRate, audibleTracks);
        }
        return;
    }
    audioPerformanceMode_ = mode;
    audioLoadClass_ = -1;
    lastAudioTuning_ = std::chrono::steady_clock::time_point {};
    tuneForLoad(sampleRate, audibleTracks);
}

void GuiAudioRuntime::adjustCustomLevel(int delta, int sampleRate, int audibleTracks) {
    if (delta == 0) {
        return;
    }
    const int previous = audioCustomLevel_;
    audioCustomLevel_ = std::clamp(audioCustomLevel_ + delta, -24, 4096);
    if (audioCustomLevel_ == previous && audioPerformanceMode_ == AudioPerformanceMode::Custom) {
        return;
    }
    audioPerformanceMode_ = AudioPerformanceMode::Custom;
    audioLoadClass_ = -1;
    lastAudioTuning_ = std::chrono::steady_clock::time_point {};
    tuneForLoad(sampleRate, audibleTracks);
}

void GuiAudioRuntime::setCustomLevel(int level, int sampleRate, int audibleTracks) {
    audioCustomLevel_ = std::clamp(level, -24, 4096);
    audioPerformanceMode_ = AudioPerformanceMode::Custom;
    audioLoadClass_ = -1;
    lastAudioTuning_ = std::chrono::steady_clock::time_point {};
    tuneForLoad(sampleRate, audibleTracks);
}

void GuiAudioRuntime::addOutputPressure(int amount) {
    if (amount <= 0) {
        return;
    }
    int current = outputPressureScore_.load(std::memory_order_relaxed);
    while (true) {
        const int updated = std::min(100, current + amount);
        if (outputPressureScore_.compare_exchange_weak(
                current,
                updated,
                std::memory_order_relaxed,
                std::memory_order_relaxed)) {
            break;
        }
    }
}

void GuiAudioRuntime::close() {
    stopPipeWriterThread();
#if ARACHNO_HAS_ALSA
    {
        std::lock_guard<std::mutex> lock(audioAlsaMutex_);
        audioAlsaThreadStop_ = true;
    }
    audioAlsaCv_.notify_all();
    if (audioAlsaWriterThread_.joinable()) {
        audioAlsaWriterThread_.join();
    }
    audioAlsaThreadRunning_ = false;
    audioAlsaThreadStop_ = false;
    audioAlsaQueue_.clear();
    audioAlsaQueueCapacity_ = 0;
    audioAlsaQueueRead_ = 0;
    audioAlsaQueueSize_ = 0;
    audioAlsaHealthy_ = true;
    if (audioPcm_ != nullptr) {
        snd_pcm_drop(audioPcm_);
        snd_pcm_close(audioPcm_);
        audioPcm_ = nullptr;
    }
#endif
    if (audioPipe_ != nullptr) {
        ::pclose(audioPipe_);
        audioPipe_ = nullptr;
    }
    audioPipeUsingFloat_ = false;
    audioPipeHealthy_ = true;
    audioOutputUsesAlsa_ = false;
    outputPressureScore_.store(0, std::memory_order_relaxed);
    outputCongestionEvents_.store(0, std::memory_order_relaxed);
    outputStarvationEvents_.store(0, std::memory_order_relaxed);
    outputXrunRecoveries_.store(0, std::memory_order_relaxed);
    outputWriteFailures_.store(0, std::memory_order_relaxed);
    std::lock_guard<std::mutex> pressureLock(outputPressureMutex_);
    outputPressureLastDecay_ = std::chrono::steady_clock::now();
}

bool GuiAudioRuntime::open(int sampleRate) {
    close();
    if (sampleRate <= 0) {
        return false;
    }
    std::string forcedOutput;
    if (const char* env = std::getenv("ARACHNO_AUDIO_OUTPUT"); env != nullptr) {
        forcedOutput = env;
        std::transform(forcedOutput.begin(), forcedOutput.end(), forcedOutput.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
    }
    const bool forceAplay = forcedOutput == "aplay";
    const bool forceAlsa = forcedOutput == "alsa";

#if ARACHNO_HAS_ALSA
    auto tryOpenAlsa = [&](bool useFloat) -> bool {
        snd_pcm_t* pcm = nullptr;
        const int openResult = snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
        if (openResult < 0 || pcm == nullptr) {
            return false;
        }
        snd_pcm_nonblock(pcm, 0);
        const snd_pcm_format_t format = useFloat ? SND_PCM_FORMAT_FLOAT_LE : SND_PCM_FORMAT_S16_LE;
        constexpr unsigned int latencyUs = 100000;
        const int paramResult = snd_pcm_set_params(
            pcm,
            format,
            SND_PCM_ACCESS_RW_INTERLEAVED,
            2,
            static_cast<unsigned int>(sampleRate),
            1,
            latencyUs);
        if (paramResult < 0) {
            snd_pcm_close(pcm);
            return false;
        }
        if (snd_pcm_prepare(pcm) < 0) {
            snd_pcm_close(pcm);
            return false;
        }
        audioPcm_ = pcm;
        audioPcmUsingFloat_ = useFloat;
        audioOutputUsesAlsa_ = true;
        if (!startAlsaWriterThread(sampleRate)) {
            snd_pcm_close(pcm);
            audioPcm_ = nullptr;
            audioOutputUsesAlsa_ = false;
            return false;
        }
        return true;
    };
    auto tryAlsaPreferred = [&]() {
        // Prefer float to avoid per-sample S16 conversion in the writer thread.
        return tryOpenAlsa(true) || tryOpenAlsa(false);
    };
#endif

    auto configureLowLatencyPipe = [&]() {
        if (audioPipe_ != nullptr) {
            ignoreSigpipeForAudioPipes();
            setvbuf(audioPipe_, nullptr, _IONBF, 0);
            audioPipeBuffer_.clear();
        }
    };

    constexpr int liveBufferMicros = 120000;
    constexpr int livePeriodMicros = 12000;
    auto tryAplayPreferred = [&]() {
        {
            std::ostringstream cmd;
            cmd
                << "aplay -q -t raw -f FLOAT_LE -c 2 -r " << sampleRate
                << " -B " << liveBufferMicros
                << " -F " << livePeriodMicros;
            audioPipe_ = ::popen(cmd.str().c_str(), "w");
            if (audioPipe_ != nullptr) {
                configureLowLatencyPipe();
                audioPipeUsingFloat_ = true;
                if (!startPipeWriterThread(sampleRate)) {
                    ::pclose(audioPipe_);
                    audioPipe_ = nullptr;
                } else {
                return true;
                }
            }
        }
        {
            std::ostringstream cmd;
            cmd
                << "aplay -q -t raw -f S16_LE -c 2 -r " << sampleRate
                << " -B " << liveBufferMicros
                << " -F " << livePeriodMicros;
            audioPipe_ = ::popen(cmd.str().c_str(), "w");
            if (audioPipe_ != nullptr) {
                configureLowLatencyPipe();
                audioPipeUsingFloat_ = false;
                if (!startPipeWriterThread(sampleRate)) {
                    ::pclose(audioPipe_);
                    audioPipe_ = nullptr;
                } else {
                return true;
                }
            }
        }
        {
            std::ostringstream cmd;
            cmd << "aplay -q -t raw -f FLOAT_LE -c 2 -r " << sampleRate;
            audioPipe_ = ::popen(cmd.str().c_str(), "w");
            if (audioPipe_ != nullptr) {
                configureLowLatencyPipe();
                audioPipeUsingFloat_ = true;
                if (!startPipeWriterThread(sampleRate)) {
                    ::pclose(audioPipe_);
                    audioPipe_ = nullptr;
                } else {
                return true;
                }
            }
        }
        {
            std::ostringstream cmd;
            cmd << "aplay -q -t raw -f S16_LE -c 2 -r " << sampleRate;
            audioPipe_ = ::popen(cmd.str().c_str(), "w");
            if (audioPipe_ != nullptr) {
                configureLowLatencyPipe();
                audioPipeUsingFloat_ = false;
                if (!startPipeWriterThread(sampleRate)) {
                    ::pclose(audioPipe_);
                    audioPipe_ = nullptr;
                } else {
                return true;
                }
            }
        }
        return false;
    };

    if (forceAlsa) {
#if ARACHNO_HAS_ALSA
        return tryAlsaPreferred();
#else
        return false;
#endif
    }
    if (forceAplay) {
        return tryAplayPreferred();
    }
#if ARACHNO_HAS_ALSA
    if (tryAlsaPreferred()) {
        return true;
    }
#endif
    if (tryAplayPreferred()) {
        return true;
    }
    return false;
}

bool GuiAudioRuntime::write(const float* left, const float* right, int frames) {
    if (frames <= 0) {
        return false;
    }
#if ARACHNO_HAS_ALSA
    if (audioOutputUsesAlsa_ && audioPcm_ != nullptr) {
        if (enqueueAlsaFrames(left, right, frames)) {
            return true;
        }
        close();
        return false;
    }
#endif
    if (audioPipe_ == nullptr) {
        return false;
    }
    if (!enqueuePipeFrames(left, right, frames)) {
        close();
        return false;
    }
    return true;
}

bool GuiAudioRuntime::hasOutput() const {
    return (audioOutputUsesAlsa_
#if ARACHNO_HAS_ALSA
        && audioPcm_ != nullptr
#else
        && false
#endif
        ) || (audioPipe_ != nullptr);
}

bool GuiAudioRuntime::usesAlsa() const {
    return audioOutputUsesAlsa_;
}

AudioPerformanceMode GuiAudioRuntime::performanceMode() const {
    return audioPerformanceMode_;
}

int GuiAudioRuntime::customLevel() const {
    return audioCustomLevel_;
}

int GuiAudioRuntime::frameMin() const {
    return audioFrameMin_;
}

int GuiAudioRuntime::frameMax() const {
    return audioFrameMax_;
}

int GuiAudioRuntime::loadClass() const {
    return audioLoadClass_;
}

int GuiAudioRuntime::outputPressureLevel() {
    const auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(outputPressureMutex_);
        if (outputPressureLastDecay_ == std::chrono::steady_clock::time_point {}) {
            outputPressureLastDecay_ = now;
        } else {
            const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - outputPressureLastDecay_);
            if (elapsedMs.count() >= 24) {
                const int decaySteps = static_cast<int>(elapsedMs.count() / 24);
                int current = outputPressureScore_.load(std::memory_order_relaxed);
                while (true) {
                    const int updated = std::max(0, current - decaySteps);
                    if (outputPressureScore_.compare_exchange_weak(
                            current,
                            updated,
                            std::memory_order_relaxed,
                            std::memory_order_relaxed)) {
                        break;
                    }
                }
                outputPressureLastDecay_ += std::chrono::milliseconds(decaySteps * 24);
            }
        }
    }
    return outputPressureScore_.load(std::memory_order_relaxed);
}

std::pair<std::size_t, std::size_t> GuiAudioRuntime::alsaQueueUsage() {
#if ARACHNO_HAS_ALSA
    std::lock_guard<std::mutex> lock(audioAlsaMutex_);
    return {audioAlsaQueueSize_, audioAlsaQueueCapacity_};
#else
    return {0, 0};
#endif
}

std::pair<std::size_t, std::size_t> GuiAudioRuntime::pipeQueueUsage() {
    std::lock_guard<std::mutex> lock(audioPipeMutex_);
    return {audioPipeQueueSize_, audioPipeQueueCapacity_};
}

void GuiAudioRuntime::stopPipeWriterThread() {
    {
        std::lock_guard<std::mutex> lock(audioPipeMutex_);
        audioPipeThreadStop_ = true;
    }
    audioPipeCv_.notify_all();
    if (audioPipeWriterThread_.joinable()) {
        audioPipeWriterThread_.join();
    }
    std::lock_guard<std::mutex> lock(audioPipeMutex_);
    audioPipeThreadRunning_ = false;
    audioPipeThreadStop_ = false;
    audioPipeQueue_.clear();
    audioPipeQueueCapacity_ = 0;
    audioPipeQueueRead_ = 0;
    audioPipeQueueSize_ = 0;
}

bool GuiAudioRuntime::startPipeWriterThread(int sampleRate) {
    if (audioPipe_ == nullptr || sampleRate <= 0) {
        return false;
    }
    stopPipeWriterThread();
    {
        std::lock_guard<std::mutex> lock(audioPipeMutex_);
        const int queueFrames = std::max(sampleRate / 2, 12288);
        audioPipeQueueCapacity_ = static_cast<std::size_t>(queueFrames) * 2;
        audioPipeQueue_.assign(audioPipeQueueCapacity_, 0.0f);
        audioPipeQueueRead_ = 0;
        audioPipeQueueSize_ = 0;
        audioPipeChunkSamples_ = static_cast<std::size_t>(std::clamp(sampleRate / 128, 256, 2048)) * 2;
        audioPipeThreadStop_ = false;
        audioPipeHealthy_ = true;
        audioPipeThreadRunning_ = true;
    }

    FILE* pipe = audioPipe_;
    const bool pipeUsesFloat = audioPipeUsingFloat_;
    audioPipeWriterThread_ = std::thread([this, pipe, pipeUsesFloat]() {
#if defined(__linux__)
        // Best-effort realtime-ish scheduling for steadier pipe delivery.
        sched_param sched {};
        sched.sched_priority = 7;
        (void)pthread_setschedparam(pthread_self(), SCHED_RR, &sched);
#endif
        std::vector<float> chunkFloat;
        std::vector<short> chunkS16;
        constexpr std::size_t maxChunkSamples = 4096;
        chunkFloat.reserve(maxChunkSamples);
        chunkS16.reserve(maxChunkSamples);
        bool primed = false;
        int starvationSpins = 0;
        float concealLeft = 0.0f;
        float concealRight = 0.0f;

        while (true) {
            bool synthesizedFallback = false;
            {
                std::unique_lock<std::mutex> lock(audioPipeMutex_);
                const std::size_t startThreshold = std::max<std::size_t>(2, audioPipeChunkSamples_);
                const std::size_t sustainThreshold = std::max<std::size_t>(2, audioPipeChunkSamples_ / 3);
                const bool wokeWithAudio = audioPipeCv_.wait_for(lock, std::chrono::milliseconds(12), [&]() {
                    return audioPipeThreadStop_ || audioPipeQueueSize_ >= (primed ? sustainThreshold : startThreshold);
                });
                if (audioPipeThreadStop_ && audioPipeQueueSize_ < 2) {
                    break;
                }
                const std::size_t request = std::max<std::size_t>(2, audioPipeChunkSamples_);
                if (!wokeWithAudio && primed) {
                    ++starvationSpins;
                    if (starvationSpins < 8) {
                        continue;
                    }
                    const std::size_t concealSamples = std::min(
                        maxChunkSamples,
                        std::max<std::size_t>(2, std::min(request, sustainThreshold)));
                    chunkFloat.resize(concealSamples);
                    float localLeft = concealLeft;
                    float localRight = concealRight;
                    for (std::size_t sample = 0; sample < concealSamples; sample += 2) {
                        localLeft *= 0.93f;
                        localRight *= 0.93f;
                        chunkFloat[sample] = localLeft;
                        if (sample + 1 < concealSamples) {
                            chunkFloat[sample + 1] = localRight;
                        }
                    }
                    concealLeft = localLeft;
                    concealRight = localRight;
                    synthesizedFallback = true;
                } else {
                    starvationSpins = 0;
                    const std::size_t availableEvenSamples = audioPipeQueueSize_ - (audioPipeQueueSize_ % 2);
                    if (availableEvenSamples < 2) {
                        primed = false;
                        continue;
                    }
                    primed = true;
                    const std::size_t toCopy = std::min(
                        maxChunkSamples,
                        std::max<std::size_t>(2, std::min(request, availableEvenSamples)));
                    chunkFloat.resize(toCopy);
                    const std::size_t contiguous = std::min(toCopy, audioPipeQueueCapacity_ - audioPipeQueueRead_);
                    std::memcpy(
                        chunkFloat.data(),
                        audioPipeQueue_.data() + audioPipeQueueRead_,
                        contiguous * sizeof(float));
                    if (toCopy > contiguous) {
                        std::memcpy(
                            chunkFloat.data() + contiguous,
                            audioPipeQueue_.data(),
                            (toCopy - contiguous) * sizeof(float));
                    }
                    audioPipeQueueRead_ = (audioPipeQueueRead_ + toCopy) % audioPipeQueueCapacity_;
                    audioPipeQueueSize_ -= toCopy;
                    if (audioPipeQueueSize_ < (startThreshold / 3)) {
                        primed = false;
                    }
                    if (toCopy >= 2) {
                        concealLeft = chunkFloat[toCopy - 2];
                        concealRight = chunkFloat[toCopy - 1];
                    }
                    lock.unlock();
                    audioPipeCv_.notify_one();
                }
            }

            if (chunkFloat.empty()) {
                continue;
            }
            if (pipeUsesFloat) {
                const std::size_t written = std::fwrite(
                    chunkFloat.data(),
                    sizeof(float),
                    chunkFloat.size(),
                    pipe);
                if (written != chunkFloat.size()) {
                    std::lock_guard<std::mutex> lock(audioPipeMutex_);
                    audioPipeHealthy_ = false;
                    outputWriteFailures_.fetch_add(1, std::memory_order_relaxed);
                    addOutputPressure(24);
                    break;
                }
            } else {
                chunkS16.resize(chunkFloat.size());
                for (std::size_t index = 0; index < chunkFloat.size(); ++index) {
                    const float clamped = std::clamp(chunkFloat[index], -1.0f, 1.0f);
                    chunkS16[index] = static_cast<short>(std::lround(clamped * 32767.0f));
                }
                const std::size_t written = std::fwrite(
                    chunkS16.data(),
                    sizeof(short),
                    chunkS16.size(),
                    pipe);
                if (written != chunkS16.size()) {
                    std::lock_guard<std::mutex> lock(audioPipeMutex_);
                    audioPipeHealthy_ = false;
                    outputWriteFailures_.fetch_add(1, std::memory_order_relaxed);
                    addOutputPressure(24);
                    break;
                }
            }
            if (synthesizedFallback) {
                std::lock_guard<std::mutex> lock(audioPipeMutex_);
                outputStarvationEvents_.fetch_add(1, std::memory_order_relaxed);
                addOutputPressure(16);
                if (audioPipeQueueSize_ == 0) {
                    primed = false;
                }
            }
        }
        std::lock_guard<std::mutex> lock(audioPipeMutex_);
        audioPipeThreadRunning_ = false;
    });
    return true;
}

bool GuiAudioRuntime::enqueuePipeFrames(const float* left, const float* right, int frames) {
    if (audioPipe_ == nullptr || frames <= 0) {
        return false;
    }
    const std::size_t required = static_cast<std::size_t>(frames) * 2;
    if (required == 0) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(audioPipeMutex_);
        if (!audioPipeThreadRunning_ || audioPipeThreadStop_ || !audioPipeHealthy_ || required > audioPipeQueueCapacity_) {
            return false;
        }
    }
    if (audioInterleavedFloat_.size() < required) {
        audioInterleavedFloat_.resize(required);
    }
    for (int index = 0; index < frames; ++index) {
        const std::size_t sample = static_cast<std::size_t>(index) * 2;
        audioInterleavedFloat_[sample] = left[static_cast<std::size_t>(index)];
        audioInterleavedFloat_[sample + 1] = right[static_cast<std::size_t>(index)];
    }

    std::unique_lock<std::mutex> lock(audioPipeMutex_);
    if (!audioPipeThreadRunning_ || audioPipeThreadStop_ || !audioPipeHealthy_ || required > audioPipeQueueCapacity_) {
        return false;
    }
    while (audioPipeQueueSize_ + required > audioPipeQueueCapacity_) {
        outputCongestionEvents_.fetch_add(1, std::memory_order_relaxed);
        addOutputPressure(6);
        (void)audioPipeCv_.wait_for(lock, std::chrono::milliseconds(1), [&]() {
            return audioPipeThreadStop_
                || !audioPipeHealthy_
                || (audioPipeQueueSize_ + required <= audioPipeQueueCapacity_);
        });
        if (audioPipeThreadStop_ || !audioPipeHealthy_) {
            return false;
        }
    }
    std::size_t writeIndex = (audioPipeQueueRead_ + audioPipeQueueSize_) % audioPipeQueueCapacity_;
    const std::size_t contiguous = std::min(required, audioPipeQueueCapacity_ - writeIndex);
    std::memcpy(
        audioPipeQueue_.data() + writeIndex,
        audioInterleavedFloat_.data(),
        contiguous * sizeof(float));
    if (required > contiguous) {
        std::memcpy(
            audioPipeQueue_.data(),
            audioInterleavedFloat_.data() + contiguous,
            (required - contiguous) * sizeof(float));
    }
    audioPipeQueueSize_ += required;
    lock.unlock();
    audioPipeCv_.notify_one();
    return true;
}

#if ARACHNO_HAS_ALSA
void GuiAudioRuntime::resetAlsaQueue(int sampleRate) {
    audioAlsaQueueRead_ = 0;
    audioAlsaQueueSize_ = 0;
    const int queueFrames = std::max(12288, sampleRate / 2);
    audioAlsaQueueCapacity_ = static_cast<std::size_t>(queueFrames) * 2;
    audioAlsaQueue_.assign(audioAlsaQueueCapacity_, 0.0f);
    audioAlsaStartThresholdSamples_ = static_cast<std::size_t>(std::clamp(sampleRate / 8, 1024, 8192)) * 2;
    audioAlsaChunkSamples_ = static_cast<std::size_t>(std::clamp(sampleRate / 128, 256, 2048)) * 2;
}

void GuiAudioRuntime::tuneAlsaQueueForLoad(int sampleRate, int loadClass) {
    if (!audioAlsaThreadRunning_ || audioAlsaQueueCapacity_ == 0) {
        return;
    }
    const int startFrames = loadClass <= 0 ? 2048 : (loadClass == 1 ? 3072 : 4096);
    const int chunkFrames = loadClass <= 0 ? 384 : (loadClass == 1 ? 512 : 768);
    const std::size_t desiredStartSamples = static_cast<std::size_t>(std::clamp(startFrames, 512, 8192)) * 2;
    const std::size_t desiredChunkSamples = static_cast<std::size_t>(std::clamp(chunkFrames, 256, 3072)) * 2;
    std::lock_guard<std::mutex> lock(audioAlsaMutex_);
    audioAlsaStartThresholdSamples_ = std::min(desiredStartSamples, audioAlsaQueueCapacity_);
    audioAlsaChunkSamples_ = std::min(desiredChunkSamples, audioAlsaQueueCapacity_);
    if (audioAlsaChunkSamples_ < 2) {
        audioAlsaChunkSamples_ = 2;
    }
    if (audioAlsaStartThresholdSamples_ < 2) {
        audioAlsaStartThresholdSamples_ = 2;
    }
    (void)sampleRate;
}

bool GuiAudioRuntime::enqueueAlsaFrames(const float* left, const float* right, int frames) {
    if (!audioOutputUsesAlsa_ || !audioAlsaThreadRunning_ || !audioAlsaHealthy_ || frames <= 0) {
        return false;
    }
    const std::size_t required = static_cast<std::size_t>(frames) * 2;
    if (required == 0 || required > audioAlsaQueueCapacity_) {
        return false;
    }
    if (audioInterleavedFloat_.size() < required) {
        audioInterleavedFloat_.resize(required);
    }
    for (int index = 0; index < frames; ++index) {
        const std::size_t sample = static_cast<std::size_t>(index) * 2;
        audioInterleavedFloat_[sample] = left[static_cast<std::size_t>(index)];
        audioInterleavedFloat_[sample + 1] = right[static_cast<std::size_t>(index)];
    }

    std::unique_lock<std::mutex> lock(audioAlsaMutex_);
    if (audioAlsaThreadStop_ || !audioAlsaHealthy_) {
        return false;
    }
    while (audioAlsaQueueSize_ + required > audioAlsaQueueCapacity_) {
        // Backpressure instead of trimming: dropping queued samples introduces audible artifacts.
        outputCongestionEvents_.fetch_add(1, std::memory_order_relaxed);
        addOutputPressure(6);
        (void)audioAlsaCv_.wait_for(lock, std::chrono::milliseconds(1), [&]() {
            return audioAlsaThreadStop_
                || !audioAlsaHealthy_
                || (audioAlsaQueueSize_ + required <= audioAlsaQueueCapacity_);
        });
        if (audioAlsaThreadStop_ || !audioAlsaHealthy_) {
            return false;
        }
    }
    std::size_t writeIndex = (audioAlsaQueueRead_ + audioAlsaQueueSize_) % audioAlsaQueueCapacity_;
    const std::size_t contiguous = std::min(required, audioAlsaQueueCapacity_ - writeIndex);
    std::memcpy(
        audioAlsaQueue_.data() + writeIndex,
        audioInterleavedFloat_.data(),
        contiguous * sizeof(float));
    if (required > contiguous) {
        std::memcpy(
            audioAlsaQueue_.data(),
            audioInterleavedFloat_.data() + contiguous,
            (required - contiguous) * sizeof(float));
    }
    audioAlsaQueueSize_ += required;
    lock.unlock();
    audioAlsaCv_.notify_one();
    return true;
}

bool GuiAudioRuntime::startAlsaWriterThread(int sampleRate) {
    if (audioPcm_ == nullptr) {
        return false;
    }
    resetAlsaQueue(sampleRate);
    audioAlsaThreadStop_ = false;
    audioAlsaHealthy_ = true;
    audioAlsaThreadRunning_ = true;
    audioAlsaWriterThread_ = std::thread([this]() {
#if defined(__linux__)
        // Best-effort realtime-ish scheduling for steadier PCM delivery.
        sched_param sched {};
        sched.sched_priority = 8;
        (void)pthread_setschedparam(pthread_self(), SCHED_RR, &sched);
#endif
        std::vector<float> chunkFloat;
        std::vector<short> chunkS16;
        constexpr std::size_t maxChunkSamples = 4096;
        chunkFloat.reserve(maxChunkSamples);
        chunkS16.reserve(maxChunkSamples);
        bool primed = false;
        int starvationSpins = 0;
        float concealLeft = 0.0f;
        float concealRight = 0.0f;

        auto writeFramesToPcm = [&](const void* data, int frames) -> bool {
            int offset = 0;
            while (offset < frames) {
                const snd_pcm_sframes_t written = snd_pcm_writei(
                    audioPcm_,
                    audioPcmUsingFloat_
                        ? static_cast<const void*>(static_cast<const float*>(data) + static_cast<std::size_t>(offset) * 2)
                        : static_cast<const void*>(static_cast<const short*>(data) + static_cast<std::size_t>(offset) * 2),
                    static_cast<snd_pcm_uframes_t>(frames - offset));
                if (written > 0) {
                    offset += static_cast<int>(written);
                    continue;
                }
                if (written == -EAGAIN) {
                    (void)snd_pcm_wait(audioPcm_, 2);
                    continue;
                }
                if (written == -EPIPE || written == -ESTRPIPE) {
                    if (snd_pcm_prepare(audioPcm_) < 0) {
                        return false;
                    }
                    primed = false;
                    outputXrunRecoveries_.fetch_add(1, std::memory_order_relaxed);
                    addOutputPressure(22);
                    continue;
                }
                return false;
            }
            return true;
        };

        while (true) {
            bool synthesizedFallback = false;
            {
                std::unique_lock<std::mutex> lock(audioAlsaMutex_);
                const std::size_t startThreshold = std::max<std::size_t>(2, audioAlsaStartThresholdSamples_);
                const std::size_t sustainThreshold = std::max<std::size_t>(2, audioAlsaChunkSamples_ / 3);
                const bool wokeWithAudio = audioAlsaCv_.wait_for(lock, std::chrono::milliseconds(12), [&]() {
                    return audioAlsaThreadStop_ || audioAlsaQueueSize_ >= (primed ? sustainThreshold : startThreshold);
                });
                if (audioAlsaThreadStop_ && audioAlsaQueueSize_ < 2) {
                    break;
                }
                const std::size_t requestedChunk = std::max<std::size_t>(2, audioAlsaChunkSamples_);
                if (!wokeWithAudio && primed) {
                    // Conceal only after repeated starvation waits; otherwise keep waiting for real data.
                    ++starvationSpins;
                    if (starvationSpins < 8) {
                        continue;
                    }
                    const std::size_t concealSamples = std::min(
                        maxChunkSamples,
                        std::max<std::size_t>(2, std::min(requestedChunk, sustainThreshold)));
                    chunkFloat.resize(concealSamples);
                    float localLeft = concealLeft;
                    float localRight = concealRight;
                    for (std::size_t sample = 0; sample < concealSamples; sample += 2) {
                        localLeft *= 0.93f;
                        localRight *= 0.93f;
                        chunkFloat[sample] = localLeft;
                        if (sample + 1 < concealSamples) {
                            chunkFloat[sample + 1] = localRight;
                        }
                    }
                    concealLeft = localLeft;
                    concealRight = localRight;
                    synthesizedFallback = true;
                } else {
                    starvationSpins = 0;
                    const std::size_t availableEvenSamples = audioAlsaQueueSize_ - (audioAlsaQueueSize_ % 2);
                    if (availableEvenSamples < 2) {
                        // Not enough queued PCM yet; wait for real audio instead of underflowing queue accounting.
                        primed = false;
                        continue;
                    }
                    primed = true;
                    const std::size_t toCopy = std::min(
                        maxChunkSamples,
                        std::max<std::size_t>(2, std::min(requestedChunk, availableEvenSamples)));
                    chunkFloat.resize(toCopy);
                    const std::size_t contiguous = std::min(toCopy, audioAlsaQueueCapacity_ - audioAlsaQueueRead_);
                    std::memcpy(
                        chunkFloat.data(),
                        audioAlsaQueue_.data() + audioAlsaQueueRead_,
                        contiguous * sizeof(float));
                    if (toCopy > contiguous) {
                        std::memcpy(
                            chunkFloat.data() + contiguous,
                            audioAlsaQueue_.data(),
                            (toCopy - contiguous) * sizeof(float));
                    }
                    audioAlsaQueueRead_ = (audioAlsaQueueRead_ + toCopy) % audioAlsaQueueCapacity_;
                    audioAlsaQueueSize_ -= toCopy;
                    if (audioAlsaQueueSize_ < (startThreshold / 3)) {
                        primed = false;
                    }
                    if (toCopy >= 2) {
                        concealLeft = chunkFloat[toCopy - 2];
                        concealRight = chunkFloat[toCopy - 1];
                    }
                    audioAlsaCv_.notify_one();
                }
            }

            const int frames = static_cast<int>(chunkFloat.size() / 2);
            if (frames <= 0) {
                continue;
            }
            if (audioPcmUsingFloat_) {
                if (!writeFramesToPcm(chunkFloat.data(), frames)) {
                    std::lock_guard<std::mutex> lock(audioAlsaMutex_);
                    audioAlsaHealthy_ = false;
                    outputWriteFailures_.fetch_add(1, std::memory_order_relaxed);
                    addOutputPressure(24);
                    break;
                }
            } else {
                chunkS16.resize(chunkFloat.size());
                for (std::size_t index = 0; index < chunkFloat.size(); ++index) {
                    const float clamped = std::clamp(chunkFloat[index], -1.0f, 1.0f);
                    chunkS16[index] = static_cast<short>(std::lround(clamped * 32767.0f));
                }
                if (!writeFramesToPcm(chunkS16.data(), frames)) {
                    std::lock_guard<std::mutex> lock(audioAlsaMutex_);
                    audioAlsaHealthy_ = false;
                    outputWriteFailures_.fetch_add(1, std::memory_order_relaxed);
                    addOutputPressure(24);
                    break;
                }
            }
            if (synthesizedFallback) {
                std::lock_guard<std::mutex> lock(audioAlsaMutex_);
                outputStarvationEvents_.fetch_add(1, std::memory_order_relaxed);
                addOutputPressure(16);
                if (audioAlsaQueueSize_ == 0) {
                    primed = false;
                }
            }
        }
    });
    return true;
}
#endif

} // namespace arachno
