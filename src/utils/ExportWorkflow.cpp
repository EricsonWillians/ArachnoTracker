#include "ExportWorkflow.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>

#include "AudioEngine.h"
#include "MidiExporter.h"

namespace arachno {

namespace {
std::string sanitizeStemName(const std::string& name) {
    std::string sanitized;
    sanitized.reserve(name.size());
    for (char ch : name) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            sanitized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        } else if (ch == '-' || ch == '_' || ch == ' ') {
            sanitized.push_back('_');
        }
    }
    return sanitized.empty() ? "track" : sanitized;
}

void requireOutputPath(const ExportRequest& request) {
    if (request.outputPath.empty()) {
        throw std::invalid_argument("export output path is empty");
    }
}

void reportProgress(
    const ExportProgressCallback& progress,
    ExportTarget target,
    int current,
    int total,
    const std::string& label,
    const std::string& path = "") {
    if (progress) {
        progress({target, current, total, label, path});
    }
}

std::string stemOutputPath(const ExportRequest& request, const Track& track, std::size_t index) {
    const std::filesystem::path directory(request.outputPath);
    const std::string filename = std::to_string(index + 1)
        + "_"
        + sanitizeStemName(track.name)
        + exportFormatExtension(request.audioFormat);
    return (directory / filename).string();
}

ExportResult runMixdownExport(
    const Song& song,
    const ExportRequest& request,
    const ExportProgressCallback& progress) {
    requireOutputPath(request);
    reportProgress(progress, request.target, 0, 1, "Rendering mixdown", request.outputPath);
    AudioEngine engine(song.sampleRate);
    const RenderedAudio audio = engine.renderSong(song);
    exportAudio(audio, request.outputPath, request.audioFormat);
    reportProgress(progress, request.target, 1, 1, "Exported mixdown", request.outputPath);

    ExportResult result;
    result.ok = true;
    result.target = request.target;
    result.message = "Exported mixdown";
    result.files.push_back({request.outputPath, audio.sampleRate, audio.frameCount()});
    return result;
}

ExportResult runStemExport(
    const Song& song,
    const ExportRequest& request,
    const ExportProgressCallback& progress) {
    requireOutputPath(request);
    const std::filesystem::path directory(request.outputPath);
    std::filesystem::create_directories(directory);

    AudioEngine engine(song.sampleRate);
    ExportResult result;
    result.ok = true;
    result.target = request.target;
    result.message = "Exported stems";
    reportProgress(progress, request.target, 0, static_cast<int>(song.tracks.size()), "Preparing stems", request.outputPath);

    for (std::size_t track = 0; track < song.tracks.size(); ++track) {
        const std::filesystem::path path = stemOutputPath(request, song.tracks[track], track);
        const RenderedAudio audio = engine.renderTrackStem(song, static_cast<int>(track));
        exportAudio(audio, path.string(), request.audioFormat);
        result.files.push_back({path.string(), audio.sampleRate, audio.frameCount()});
        reportProgress(
            progress,
            request.target,
            static_cast<int>(track + 1),
            static_cast<int>(song.tracks.size()),
            "Exported stem " + song.tracks[track].name,
            path.string());
    }
    return result;
}

ExportResult runMidiExport(
    const Song& song,
    const ExportRequest& request,
    const ExportProgressCallback& progress) {
    requireOutputPath(request);
    reportProgress(progress, request.target, 0, 1, "Writing MIDI", request.outputPath);
    exportMidiFile(song, request.outputPath, request.midiTicksPerQuarter);
    reportProgress(progress, request.target, 1, 1, "Exported MIDI", request.outputPath);

    ExportResult result;
    result.ok = true;
    result.target = request.target;
    result.message = "Exported MIDI";
    result.files.push_back({request.outputPath, 0, 0});
    return result;
}
} // namespace

ExportRequest mixdownExportRequest(const std::string& outputPath) {
    ExportRequest request;
    request.target = ExportTarget::Mixdown;
    request.outputPath = outputPath;
    request.audioFormat = exportFormatFromPath(outputPath);
    return request;
}

ExportRequest stemExportRequest(const std::string& outputDirectory, ExportFormat format) {
    ExportRequest request;
    request.target = ExportTarget::Stems;
    request.outputPath = outputDirectory;
    request.audioFormat = format;
    return request;
}

ExportRequest midiExportRequest(const std::string& outputPath, int ticksPerQuarter) {
    ExportRequest request;
    request.target = ExportTarget::Midi;
    request.outputPath = outputPath;
    request.midiTicksPerQuarter = ticksPerQuarter;
    return request;
}

const char* exportTargetName(ExportTarget target) {
    switch (target) {
        case ExportTarget::Mixdown:
            return "mixdown";
        case ExportTarget::Stems:
            return "stems";
        case ExportTarget::Midi:
            return "midi";
    }
    return "unknown";
}

const char* exportFormatName(ExportFormat format) {
    switch (format) {
        case ExportFormat::Wav:
            return "wav";
        case ExportFormat::Mp3:
            return "mp3";
        case ExportFormat::Ogg:
            return "ogg";
    }
    return "unknown";
}

std::string exportFormatExtension(ExportFormat format) {
    switch (format) {
        case ExportFormat::Wav:
            return ".wav";
        case ExportFormat::Mp3:
            return ".mp3";
        case ExportFormat::Ogg:
            return ".ogg";
    }
    return ".wav";
}

ExportPreflight preflightExport(const Song& song, const ExportRequest& request) {
    ExportPreflight preflight;
    preflight.target = request.target;
    preflight.outputPath = request.outputPath;

    if (request.outputPath.empty()) {
        preflight.error = "export output path is empty";
        return preflight;
    }

    switch (request.target) {
        case ExportTarget::Mixdown:
            preflight.totalWork = 1;
            preflight.expectedFiles.push_back(request.outputPath);
            break;
        case ExportTarget::Stems:
            if (song.tracks.empty()) {
                preflight.error = "song has no tracks to export";
                return preflight;
            }
            preflight.totalWork = static_cast<int>(song.tracks.size());
            for (std::size_t track = 0; track < song.tracks.size(); ++track) {
                preflight.expectedFiles.push_back(stemOutputPath(request, song.tracks[track], track));
            }
            break;
        case ExportTarget::Midi:
            if (request.midiTicksPerQuarter <= 0) {
                preflight.error = "MIDI ticks per quarter must be positive";
                return preflight;
            }
            preflight.totalWork = 1;
            preflight.expectedFiles.push_back(request.outputPath);
            break;
    }

    if (request.target == ExportTarget::Mixdown
        && request.audioFormat != exportFormatFromPath(request.outputPath)) {
        preflight.warnings.push_back("requested audio format does not match output extension");
    }
    preflight.ok = true;
    return preflight;
}

ExportResult runExportWorkflow(const Song& song, const ExportRequest& request) {
    return runExportWorkflow(song, request, nullptr);
}

ExportResult runExportWorkflow(
    const Song& song,
    const ExportRequest& request,
    const ExportProgressCallback& progress) {
    const ExportPreflight preflight = preflightExport(song, request);
    if (!preflight.ok) {
        ExportResult result;
        result.ok = false;
        result.target = request.target;
        result.error = preflight.error;
        return result;
    }

    try {
        switch (request.target) {
            case ExportTarget::Mixdown:
                return runMixdownExport(song, request, progress);
            case ExportTarget::Stems:
                return runStemExport(song, request, progress);
            case ExportTarget::Midi:
                return runMidiExport(song, request, progress);
        }
    } catch (const std::exception& error) {
        ExportResult result;
        result.ok = false;
        result.target = request.target;
        result.error = error.what();
        return result;
    }

    ExportResult result;
    result.ok = false;
    result.target = request.target;
    result.error = "unknown export target";
    return result;
}

} // namespace arachno
