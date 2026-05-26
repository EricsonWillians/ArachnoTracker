#pragma once

#include <string>
#include <vector>

#include "RealtimePlayback.h"

namespace arachno {

enum class AudioBackendType {
    Auto,
    Jack,
    PipeWire,
    Alsa,
    Dummy
};

struct AudioDeviceInfo {
    AudioBackendType backend = AudioBackendType::Dummy;
    std::string id;
    std::string name;
    int outputChannels = 2;
    int inputChannels = 0;
    int defaultSampleRate = 48000;
    int minimumBufferFrames = 64;
    int maximumBufferFrames = 2048;
    std::vector<int> supportedSampleRates;
    bool available = true;
    std::string status;
};

struct AudioRuntimeSettings {
    AudioBackendType backend = AudioBackendType::Auto;
    std::string deviceId;
    int sampleRate = 48000;
    int bufferFrames = 256;
    int periods = 2;
    bool realtimePriority = true;
    bool connectSystemOutputs = true;
};

struct AudioRuntimeValidation {
    bool ok = false;
    AudioRuntimeSettings normalized;
    AudioDeviceInfo device;
    double estimatedLatencyMs = 0.0;
    std::vector<std::string> warnings;
    std::string error;
};

struct AudioRuntimeHealth {
    bool configured = false;
    bool active = false;
    AudioBackendType backend = AudioBackendType::Dummy;
    std::string deviceId;
    int sampleRate = 48000;
    int bufferFrames = 256;
    int periods = 2;
    double estimatedLatencyMs = 0.0;
    int processedBlocks = 0;
    long long processedFrames = 0;
    int underrunCount = 0;
    std::string lastError;
};

struct AudioRuntimeProcessResult {
    bool ok = false;
    int frames = 0;
    bool underrun = false;
    std::string error;
};

const char* audioBackendName(AudioBackendType backend);
AudioBackendType audioBackendFromName(const std::string& name);
std::vector<AudioDeviceInfo> defaultLinuxAudioDeviceCatalog();
AudioRuntimeSettings defaultAudioRuntimeSettings();
AudioRuntimeValidation validateAudioRuntimeSettings(
    const AudioRuntimeSettings& settings,
    const std::vector<AudioDeviceInfo>& devices = defaultLinuxAudioDeviceCatalog());
double estimateAudioLatencyMs(int sampleRate, int bufferFrames, int periods);

class AudioRuntimeSession {
public:
    explicit AudioRuntimeSession(std::vector<AudioDeviceInfo> devices = defaultLinuxAudioDeviceCatalog());

    AudioRuntimeValidation configure(const AudioRuntimeSettings& settings);
    bool start();
    void stop();
    void resetCounters();
    AudioRuntimeProcessResult renderBlock(RealtimePlaybackSession& playback, float* left, float* right, int frameCount);
    void reportUnderrun(const std::string& detail = "");

    const AudioRuntimeHealth& health() const { return health_; }
    const std::vector<AudioDeviceInfo>& devices() const { return devices_; }

private:
    std::vector<AudioDeviceInfo> devices_;
    AudioRuntimeSettings settings_;
    AudioRuntimeHealth health_;
};

} // namespace arachno
