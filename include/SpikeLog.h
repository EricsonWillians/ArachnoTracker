#pragma once

// SpikeLog: always-compiled, env-gated latency spike instrumentation.
//
// Enable with ARACHNO_SPIKE_LOG=1 (stderr) or ARACHNO_SPIKE_LOG=/path/to.log.
// When disabled, checkSpike() costs one relaxed atomic load + a clock read.
// Purpose: catch RANDOM realtime stutters in-situ — producer render spikes,
// output-writer gaps/xruns, and GUI-loop phase spikes are logged with a
// monotonic millisecond timestamp so they can be correlated with audible
// glitches. Diagnostic-only; no behavior change when disabled.

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>

namespace arachno {
namespace spike_log_detail {

inline std::atomic<bool>& enabledFlag() {
    static std::atomic<bool> flag {[] {
        const char* env = std::getenv("ARACHNO_SPIKE_LOG");
        return env != nullptr && env[0] != '\0' && !(env[0] == '0' && env[1] == '\0');
    }()};
    return flag;
}

inline std::FILE*& sink() {
    static std::FILE* file = [] {
        const char* env = std::getenv("ARACHNO_SPIKE_LOG");
        if (env == nullptr || env[0] == '\0' || (env[0] == '0' && env[1] == '\0')) {
            return static_cast<std::FILE*>(nullptr);
        }
        if (env[0] == '1' && env[1] == '\0') {
            return stderr;
        }
        std::FILE* opened = std::fopen(env, "a");
        return opened != nullptr ? opened : stderr;
    }();
    return file;
}

inline std::mutex& sinkMutex() {
    static std::mutex mutex;
    return mutex;
}

inline long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

} // namespace spike_log_detail

inline bool spikeLogEnabled() {
    return spike_log_detail::enabledFlag().load(std::memory_order_relaxed);
}

// Logs one spike line: "<t_ms> <tag> <value_ms>ms <detail".
inline void logSpike(const char* tag, double valueMs, const char* detail = "") {
    if (!spikeLogEnabled()) {
        return;
    }
    const std::lock_guard<std::mutex> lock(spike_log_detail::sinkMutex());
    std::fprintf(spike_log_detail::sink(), "%lld %-18s %8.2fms %s\n", spike_log_detail::nowMs(), tag, valueMs, detail);
    std::fflush(spike_log_detail::sink());
}

// Cheap scoped phase timer: construct with a tag + threshold; on destruction,
// logs if the phase exceeded the threshold. When disabled, this is one atomic
// load plus two clock reads.
class SpikeProbe {
public:
    SpikeProbe(const char* tag, double thresholdMs, const char* detail = "")
        : tag_(tag), thresholdMs_(thresholdMs), detail_(detail), active_(spikeLogEnabled()) {
        if (active_) {
            start_ = std::chrono::steady_clock::now();
        }
    }
    ~SpikeProbe() {
        if (!active_) {
            return;
        }
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_).count();
        if (ms >= thresholdMs_) {
            logSpike(tag_, ms, detail_);
        }
    }
    SpikeProbe(const SpikeProbe&) = delete;
    SpikeProbe& operator=(const SpikeProbe&) = delete;

private:
    const char* tag_;
    double thresholdMs_;
    const char* detail_;
    bool active_;
    std::chrono::steady_clock::time_point start_;
};

} // namespace arachno
