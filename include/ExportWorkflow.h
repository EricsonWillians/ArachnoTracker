#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Exporter.h"
#include "Tracker.h"

namespace arachno {

enum class ExportTarget {
    Mixdown,
    Stems,
    Midi
};

struct ExportRequest {
    ExportTarget target = ExportTarget::Mixdown;
    std::string outputPath;
    ExportFormat audioFormat = ExportFormat::Wav;
    int midiTicksPerQuarter = 480;
};

struct ExportedFile {
    std::string path;
    int sampleRate = 0;
    int frameCount = 0;
};

struct ExportResult {
    bool ok = false;
    ExportTarget target = ExportTarget::Mixdown;
    std::string message;
    std::string error;
    std::vector<ExportedFile> files;
};

struct ExportProgress {
    ExportTarget target = ExportTarget::Mixdown;
    int current = 0;
    int total = 0;
    std::string label;
    std::string path;
};

using ExportProgressCallback = std::function<void(const ExportProgress&)>;

struct ExportPreflight {
    bool ok = false;
    ExportTarget target = ExportTarget::Mixdown;
    std::string outputPath;
    int totalWork = 0;
    std::vector<std::string> expectedFiles;
    std::vector<std::string> warnings;
    std::string error;
};

ExportRequest mixdownExportRequest(const std::string& outputPath);
ExportRequest stemExportRequest(const std::string& outputDirectory, ExportFormat format = ExportFormat::Wav);
ExportRequest midiExportRequest(const std::string& outputPath, int ticksPerQuarter = 480);

const char* exportTargetName(ExportTarget target);
const char* exportFormatName(ExportFormat format);
std::string exportFormatExtension(ExportFormat format);
ExportPreflight preflightExport(const Song& song, const ExportRequest& request);
ExportResult runExportWorkflow(const Song& song, const ExportRequest& request);
ExportResult runExportWorkflow(
    const Song& song,
    const ExportRequest& request,
    const ExportProgressCallback& progress);

} // namespace arachno
