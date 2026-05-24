#include "Exporter.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace arachno {

namespace {
void writeU16(std::ofstream& out, std::uint16_t value) {
    out.put(static_cast<char>(value & 0xff));
    out.put(static_cast<char>((value >> 8) & 0xff));
}

void writeU32(std::ofstream& out, std::uint32_t value) {
    out.put(static_cast<char>(value & 0xff));
    out.put(static_cast<char>((value >> 8) & 0xff));
    out.put(static_cast<char>((value >> 16) & 0xff));
    out.put(static_cast<char>((value >> 24) & 0xff));
}

std::string lowerExtension(const std::string& path) {
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return ext;
}

std::string shellQuote(const std::string& value) {
    std::string quoted = "'";
    for (char ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted += ch;
        }
    }
    quoted += "'";
    return quoted;
}

bool runEncoder(const std::string& command) {
    return std::system(command.c_str()) == 0;
}

void exportWithExternalEncoder(const RenderedAudio& audio, const std::string& path, ExportFormat format) {
    const std::filesystem::path outputPath(path);
    const std::filesystem::path tempPath = outputPath.parent_path()
        / (outputPath.stem().string() + ".arachno-export-temp.wav");

    writeWavFile(audio, tempPath.string());

    const std::string wav = shellQuote(tempPath.string());
    const std::string out = shellQuote(path);
    std::string command;

    bool success = false;
    if (format == ExportFormat::Mp3) {
        command = "ffmpeg -y -loglevel error -i " + wav + " -codec:a libmp3lame -q:a 2 " + out;
        success = runEncoder(command);
        if (!success) {
            command = "avconv -y -loglevel error -i " + wav + " -codec:a libmp3lame -q:a 2 " + out;
            success = runEncoder(command);
        }
    } else {
        command = "ffmpeg -y -loglevel error -i " + wav + " -codec:a libvorbis -q:a 6 " + out;
        success = runEncoder(command);
        if (!success) {
            command = "avconv -y -loglevel error -i " + wav + " -codec:a libvorbis -q:a 6 " + out;
            success = runEncoder(command);
        }
    }

    std::error_code ignored;
    std::filesystem::remove(tempPath, ignored);

    if (!success) {
        throw std::runtime_error("export requires ffmpeg or avconv for compressed formats");
    }
}
} // namespace

ExportFormat exportFormatFromPath(const std::string& path) {
    const std::string ext = lowerExtension(path);
    if (ext == ".mp3") {
        return ExportFormat::Mp3;
    }
    if (ext == ".ogg" || ext == ".oga") {
        return ExportFormat::Ogg;
    }
    return ExportFormat::Wav;
}

void exportAudio(const RenderedAudio& audio, const std::string& path, ExportFormat format) {
    if (format == ExportFormat::Wav) {
        writeWavFile(audio, path);
        return;
    }

    exportWithExternalEncoder(audio, path, format);
}

void writeWavFile(const RenderedAudio& audio, const std::string& path) {
    if (audio.sampleRate <= 0) {
        throw std::invalid_argument("sample rate must be positive");
    }
    if (audio.interleavedStereo.size() % 2 != 0) {
        throw std::invalid_argument("audio buffer must be interleaved stereo");
    }

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("failed to open output file: " + path);
    }

    constexpr std::uint16_t channels = 2;
    constexpr std::uint16_t bitsPerSample = 16;
    constexpr std::uint16_t blockAlign = channels * bitsPerSample / 8;
    const std::uint32_t byteRate = static_cast<std::uint32_t>(audio.sampleRate * blockAlign);
    const std::uint32_t dataSize = static_cast<std::uint32_t>(audio.interleavedStereo.size() * sizeof(std::int16_t));
    const std::uint32_t riffSize = 36 + dataSize;

    out.write("RIFF", 4);
    writeU32(out, riffSize);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    writeU32(out, 16);
    writeU16(out, 1);
    writeU16(out, channels);
    writeU32(out, static_cast<std::uint32_t>(audio.sampleRate));
    writeU32(out, byteRate);
    writeU16(out, blockAlign);
    writeU16(out, bitsPerSample);
    out.write("data", 4);
    writeU32(out, dataSize);

    for (float sample : audio.interleavedStereo) {
        const float clipped = std::clamp(sample, -1.0f, 1.0f);
        const auto pcm = static_cast<std::int16_t>(clipped * 32767.0f);
        writeU16(out, static_cast<std::uint16_t>(pcm));
    }
}

} // namespace arachno
