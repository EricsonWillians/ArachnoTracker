#pragma once

#include <string>

#include "AudioEngine.h"

namespace arachno {

enum class ExportFormat {
    Wav,
    Mp3,
    Ogg
};

ExportFormat exportFormatFromPath(const std::string& path);
void exportAudio(const RenderedAudio& audio, const std::string& path, ExportFormat format);
void writeWavFile(const RenderedAudio& audio, const std::string& path);

} // namespace arachno
