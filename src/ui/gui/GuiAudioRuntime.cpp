#include "ui/gui/GuiAudioRuntime.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <sstream>

namespace arachno {

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
    int frameMin = 128;
    int frameMax = 1536;
    if (audioPerformanceMode_ == AudioPerformanceMode::Auto) {
        if (audibleTracks > 28) {
            loadClass = 2;
        } else if (audibleTracks > 12) {
            loadClass = 1;
        }
        if (loadClass <= 0) {
            frameMin = 128;
            frameMax = 1536;
        } else if (loadClass == 1) {
            frameMin = 320;
            frameMax = 2304;
        } else {
            frameMin = 536;
            frameMax = 3216;
        }
    } else if (audioPerformanceMode_ == AudioPerformanceMode::Live) {
        loadClass = 0;
        frameMin = 96;
        frameMax = 1024;
    } else if (audioPerformanceMode_ == AudioPerformanceMode::Balanced) {
        loadClass = 1;
        frameMin = 320;
        frameMax = 2304;
    } else if (audioPerformanceMode_ == AudioPerformanceMode::Heavy) {
        loadClass = 2;
        frameMin = 536;
        frameMax = 3216;
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

void GuiAudioRuntime::close() {
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
    audioOutputUsesAlsa_ = false;
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
        constexpr unsigned int latencyUs = 30000;
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
        return tryOpenAlsa(false) || tryOpenAlsa(true);
    };
#endif

    auto configureLowLatencyPipe = [&]() {
        if (audioPipe_ != nullptr) {
            setvbuf(audioPipe_, nullptr, _IONBF, 0);
            audioPipeBuffer_.clear();
        }
    };

    constexpr int liveBufferMicros = 30000;
    constexpr int livePeriodMicros = 5000;
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
                return true;
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
                return true;
            }
        }
        {
            std::ostringstream cmd;
            cmd << "aplay -q -t raw -f FLOAT_LE -c 2 -r " << sampleRate;
            audioPipe_ = ::popen(cmd.str().c_str(), "w");
            if (audioPipe_ != nullptr) {
                configureLowLatencyPipe();
                audioPipeUsingFloat_ = true;
                return true;
            }
        }
        {
            std::ostringstream cmd;
            cmd << "aplay -q -t raw -f S16_LE -c 2 -r " << sampleRate;
            audioPipe_ = ::popen(cmd.str().c_str(), "w");
            if (audioPipe_ != nullptr) {
                configureLowLatencyPipe();
                audioPipeUsingFloat_ = false;
                return true;
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
    if (tryAplayPreferred()) {
        return true;
    }
#if ARACHNO_HAS_ALSA
    if (tryAlsaPreferred()) {
        return true;
    }
#endif
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
    if (audioPipeUsingFloat_) {
        const std::size_t sampleCount = static_cast<std::size_t>(frames) * 2;
        if (audioInterleavedFloat_.size() < sampleCount) {
            audioInterleavedFloat_.resize(sampleCount);
        }
        for (int index = 0; index < frames; ++index) {
            audioInterleavedFloat_[static_cast<std::size_t>(index) * 2] = left[static_cast<std::size_t>(index)];
            audioInterleavedFloat_[(static_cast<std::size_t>(index) * 2) + 1] = right[static_cast<std::size_t>(index)];
        }
        const std::size_t written = std::fwrite(
            audioInterleavedFloat_.data(),
            sizeof(float),
            sampleCount,
            audioPipe_);
        if (written != sampleCount) {
            close();
            return false;
        }
    } else {
        const std::size_t sampleCount = static_cast<std::size_t>(frames) * 2;
        if (audioInterleavedS16_.size() < sampleCount) {
            audioInterleavedS16_.resize(sampleCount);
        }
        for (int index = 0; index < frames; ++index) {
            const float l = std::clamp(left[static_cast<std::size_t>(index)], -1.0f, 1.0f);
            const float r = std::clamp(right[static_cast<std::size_t>(index)], -1.0f, 1.0f);
            audioInterleavedS16_[static_cast<std::size_t>(index) * 2] = static_cast<short>(std::lround(l * 32767.0f));
            audioInterleavedS16_[(static_cast<std::size_t>(index) * 2) + 1] = static_cast<short>(std::lround(r * 32767.0f));
        }
        const std::size_t written = std::fwrite(
            audioInterleavedS16_.data(),
            sizeof(short),
            sampleCount,
            audioPipe_);
        if (written != sampleCount) {
            close();
            return false;
        }
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

std::pair<std::size_t, std::size_t> GuiAudioRuntime::alsaQueueUsage() {
#if ARACHNO_HAS_ALSA
    std::lock_guard<std::mutex> lock(audioAlsaMutex_);
    return {audioAlsaQueueSize_, audioAlsaQueueCapacity_};
#else
    return {0, 0};
#endif
}

#if ARACHNO_HAS_ALSA
void GuiAudioRuntime::resetAlsaQueue(int sampleRate) {
    audioAlsaQueueRead_ = 0;
    audioAlsaQueueSize_ = 0;
    const int queueFrames = std::max(2048, sampleRate / 3);
    audioAlsaQueueCapacity_ = static_cast<std::size_t>(queueFrames) * 2;
    audioAlsaQueue_.assign(audioAlsaQueueCapacity_, 0.0f);
    audioAlsaStartThresholdSamples_ = static_cast<std::size_t>(std::clamp(sampleRate / 150, 96, 768)) * 2;
    audioAlsaChunkSamples_ = static_cast<std::size_t>(std::clamp(sampleRate / 300, 64, 512)) * 2;
}

void GuiAudioRuntime::tuneAlsaQueueForLoad(int sampleRate, int loadClass) {
    if (!audioAlsaThreadRunning_ || audioAlsaQueueCapacity_ == 0) {
        return;
    }
    const int startFrames = loadClass <= 0 ? 384 : (loadClass == 1 ? 896 : 1664);
    const int chunkFrames = loadClass <= 0 ? 256 : (loadClass == 1 ? 512 : 1024);
    const std::size_t desiredStartSamples = static_cast<std::size_t>(std::clamp(startFrames, 64, 2048)) * 2;
    const std::size_t desiredChunkSamples = static_cast<std::size_t>(std::clamp(chunkFrames, 32, 1024)) * 2;
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
    if (audioAlsaQueueSize_ + required > audioAlsaQueueCapacity_) {
        const std::size_t overflow = (audioAlsaQueueSize_ + required) - audioAlsaQueueCapacity_;
        audioAlsaQueueRead_ = (audioAlsaQueueRead_ + overflow) % audioAlsaQueueCapacity_;
        audioAlsaQueueSize_ -= overflow;
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
        std::vector<float> chunkFloat;
        std::vector<short> chunkS16;
        constexpr std::size_t maxChunkSamples = 4096;
        chunkFloat.reserve(maxChunkSamples);
        chunkS16.reserve(maxChunkSamples);
        bool primed = false;

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
                    (void)snd_pcm_wait(audioPcm_, 5);
                    continue;
                }
                if (written == -EPIPE || written == -ESTRPIPE) {
                    if (snd_pcm_prepare(audioPcm_) < 0) {
                        return false;
                    }
                    continue;
                }
                return false;
            }
            return true;
        };

        while (true) {
            {
                std::unique_lock<std::mutex> lock(audioAlsaMutex_);
                const std::size_t startThreshold = std::max<std::size_t>(2, audioAlsaStartThresholdSamples_);
                audioAlsaCv_.wait(lock, [&]() {
                    return audioAlsaThreadStop_ || audioAlsaQueueSize_ >= (primed ? 2 : startThreshold);
                });
                if (audioAlsaThreadStop_ && audioAlsaQueueSize_ < 2) {
                    break;
                }
                primed = true;
                const std::size_t requestedChunk = std::max<std::size_t>(2, audioAlsaChunkSamples_);
                const std::size_t toCopy = std::min(
                    maxChunkSamples,
                    std::max<std::size_t>(2, std::min(requestedChunk, audioAlsaQueueSize_ - (audioAlsaQueueSize_ % 2))));
                chunkFloat.resize(toCopy);
                for (std::size_t sample = 0; sample < toCopy; ++sample) {
                    chunkFloat[sample] = audioAlsaQueue_[audioAlsaQueueRead_];
                    audioAlsaQueueRead_ = (audioAlsaQueueRead_ + 1) % audioAlsaQueueCapacity_;
                }
                audioAlsaQueueSize_ -= toCopy;
                if (audioAlsaQueueSize_ < (startThreshold / 3)) {
                    primed = false;
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
                    break;
                }
            }
        }
    });
    return true;
}
#endif

} // namespace arachno
