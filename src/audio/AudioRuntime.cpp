#include "AudioRuntime.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace arachno {

namespace {
std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool supportsSampleRate(const AudioDeviceInfo& device, int sampleRate) {
    return device.supportedSampleRates.empty()
        || std::find(device.supportedSampleRates.begin(), device.supportedSampleRates.end(), sampleRate)
            != device.supportedSampleRates.end();
}

const AudioDeviceInfo* findDevice(
    const AudioRuntimeSettings& settings,
    const std::vector<AudioDeviceInfo>& devices,
    std::vector<std::string>& warnings) {
    if (!settings.deviceId.empty()) {
        const auto it = std::find_if(devices.begin(), devices.end(), [&settings](const AudioDeviceInfo& device) {
            return device.id == settings.deviceId;
        });
        if (it == devices.end()) {
            return nullptr;
        }
        if (settings.backend != AudioBackendType::Auto && it->backend != settings.backend) {
            warnings.push_back("selected device backend does not match requested backend");
        }
        return &(*it);
    }

    const AudioBackendType preferred[] = {
        AudioBackendType::PipeWire,
        AudioBackendType::Jack,
        AudioBackendType::Alsa,
        AudioBackendType::Dummy
    };

    if (settings.backend == AudioBackendType::Auto) {
        for (AudioBackendType backend : preferred) {
            const auto it = std::find_if(devices.begin(), devices.end(), [backend](const AudioDeviceInfo& device) {
                return device.available && device.backend == backend;
            });
            if (it != devices.end()) {
                return &(*it);
            }
        }
    } else {
        const auto it = std::find_if(devices.begin(), devices.end(), [&settings](const AudioDeviceInfo& device) {
            return device.available && device.backend == settings.backend;
        });
        if (it != devices.end()) {
            return &(*it);
        }
    }
    return nullptr;
}
} // namespace

const char* audioBackendName(AudioBackendType backend) {
    switch (backend) {
        case AudioBackendType::Auto:
            return "auto";
        case AudioBackendType::Jack:
            return "jack";
        case AudioBackendType::PipeWire:
            return "pipewire";
        case AudioBackendType::Alsa:
            return "alsa";
        case AudioBackendType::Dummy:
            return "dummy";
    }
    return "unknown";
}

AudioBackendType audioBackendFromName(const std::string& name) {
    const std::string normalized = lowercase(name);
    if (normalized == "auto") {
        return AudioBackendType::Auto;
    }
    if (normalized == "jack") {
        return AudioBackendType::Jack;
    }
    if (normalized == "pipewire" || normalized == "pw") {
        return AudioBackendType::PipeWire;
    }
    if (normalized == "alsa") {
        return AudioBackendType::Alsa;
    }
    return AudioBackendType::Dummy;
}

std::vector<AudioDeviceInfo> defaultLinuxAudioDeviceCatalog() {
    return {
        {
            AudioBackendType::PipeWire,
            "pipewire/default",
            "PipeWire default output",
            2,
            0,
            48000,
            64,
            2048,
            {44100, 48000, 88200, 96000},
            false,
            "requires a PipeWire adapter"
        },
        {
            AudioBackendType::Jack,
            "jack/default",
            "JACK default client",
            2,
            0,
            48000,
            32,
            2048,
            {44100, 48000, 88200, 96000},
            false,
            "requires a JACK adapter"
        },
        {
            AudioBackendType::Alsa,
            "alsa/default",
            "ALSA default PCM",
            2,
            0,
            48000,
            128,
            4096,
            {44100, 48000, 96000},
            false,
            "requires an ALSA adapter"
        },
        {
            AudioBackendType::Dummy,
            "dummy/offline",
            "Offline preview device",
            2,
            0,
            48000,
            64,
            4096,
            {44100, 48000, 96000},
            true,
            "headless render callback for tests and non-device previews"
        }
    };
}

AudioRuntimeSettings defaultAudioRuntimeSettings() {
    AudioRuntimeSettings settings;
    settings.backend = AudioBackendType::Auto;
    settings.sampleRate = 48000;
    settings.bufferFrames = 256;
    settings.periods = 2;
    settings.realtimePriority = true;
    settings.connectSystemOutputs = true;
    return settings;
}

double estimateAudioLatencyMs(int sampleRate, int bufferFrames, int periods) {
    if (sampleRate <= 0 || bufferFrames <= 0 || periods <= 0) {
        return 0.0;
    }
    return (static_cast<double>(bufferFrames) * static_cast<double>(periods) * 1000.0)
        / static_cast<double>(sampleRate);
}

AudioRuntimeValidation validateAudioRuntimeSettings(
    const AudioRuntimeSettings& settings,
    const std::vector<AudioDeviceInfo>& devices) {
    AudioRuntimeValidation result;
    result.normalized = settings;

    if (settings.sampleRate <= 0) {
        result.error = "audio sample rate must be positive";
        return result;
    }
    if (settings.bufferFrames <= 0) {
        result.error = "audio buffer size must be positive";
        return result;
    }
    if (settings.periods <= 0) {
        result.error = "audio period count must be positive";
        return result;
    }

    const AudioDeviceInfo* device = findDevice(settings, devices, result.warnings);
    if (device == nullptr) {
        result.error = settings.deviceId.empty()
            ? "no available audio device for requested backend"
            : "requested audio device is not available";
        return result;
    }
    if (!device->available) {
        result.error = "requested audio device is not available";
        return result;
    }

    result.device = *device;
    result.normalized.backend = device->backend;
    result.normalized.deviceId = device->id;
    if (result.normalized.sampleRate <= 0) {
        result.normalized.sampleRate = device->defaultSampleRate;
    }
    if (!supportsSampleRate(*device, result.normalized.sampleRate)) {
        result.warnings.push_back("sample rate is not advertised by selected device");
    }
    if (result.normalized.bufferFrames < device->minimumBufferFrames) {
        result.normalized.bufferFrames = device->minimumBufferFrames;
        result.warnings.push_back("buffer size raised to device minimum");
    }
    if (result.normalized.bufferFrames > device->maximumBufferFrames) {
        result.normalized.bufferFrames = device->maximumBufferFrames;
        result.warnings.push_back("buffer size lowered to device maximum");
    }
    result.estimatedLatencyMs = estimateAudioLatencyMs(
        result.normalized.sampleRate,
        result.normalized.bufferFrames,
        result.normalized.periods);
    result.ok = true;
    return result;
}

AudioRuntimeSession::AudioRuntimeSession(std::vector<AudioDeviceInfo> devices)
    : devices_(std::move(devices)), settings_(defaultAudioRuntimeSettings()) {}

AudioRuntimeValidation AudioRuntimeSession::configure(const AudioRuntimeSettings& settings) {
    const AudioRuntimeValidation validation = validateAudioRuntimeSettings(settings, devices_);
    if (!validation.ok) {
        health_.configured = false;
        health_.active = false;
        health_.lastError = validation.error;
        return validation;
    }

    settings_ = validation.normalized;
    health_.configured = true;
    health_.active = false;
    health_.backend = settings_.backend;
    health_.deviceId = settings_.deviceId;
    health_.sampleRate = settings_.sampleRate;
    health_.bufferFrames = settings_.bufferFrames;
    health_.periods = settings_.periods;
    health_.estimatedLatencyMs = validation.estimatedLatencyMs;
    health_.lastError.clear();
    resetCounters();
    return validation;
}

bool AudioRuntimeSession::start() {
    if (!health_.configured) {
        health_.lastError = "audio runtime is not configured";
        return false;
    }
    health_.active = true;
    return true;
}

void AudioRuntimeSession::stop() {
    health_.active = false;
}

void AudioRuntimeSession::resetCounters() {
    health_.processedBlocks = 0;
    health_.processedFrames = 0;
    health_.underrunCount = 0;
}

AudioRuntimeProcessResult AudioRuntimeSession::renderBlock(
    RealtimePlaybackSession& playback,
    float* left,
    float* right,
    int frameCount) {
    AudioRuntimeProcessResult result;
    if (!health_.configured) {
        result.error = "audio runtime is not configured";
        health_.lastError = result.error;
        return result;
    }
    if (!health_.active) {
        result.error = "audio runtime is not active";
        health_.lastError = result.error;
        return result;
    }
    if (left == nullptr || right == nullptr || frameCount <= 0) {
        result.error = "audio runtime render block is invalid";
        health_.lastError = result.error;
        return result;
    }

    playback.render(left, right, frameCount);
    ++health_.processedBlocks;
    health_.processedFrames += frameCount;
    result.ok = true;
    result.frames = frameCount;
    if (frameCount != health_.bufferFrames) {
        result.underrun = frameCount < health_.bufferFrames;
        if (result.underrun) {
            reportUnderrun("short audio callback");
        }
    }
    return result;
}

void AudioRuntimeSession::reportUnderrun(const std::string& detail) {
    ++health_.underrunCount;
    health_.lastError = detail.empty() ? "audio underrun" : detail;
}

} // namespace arachno
