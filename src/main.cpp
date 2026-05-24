#include <exception>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "AudioEngine.h"
#include "Exporter.h"
#include "MidiExporter.h"
#include "PatternEditor.h"
#include "PatternView.h"
#include "ProjectIO.h"
#include "Tracker.h"

namespace {

void printUsage() {
    std::cout
        << "ArachnoTracker\n"
        << "Usage:\n"
        << "  ArachnoTracker --demo <output.wav|output.mp3|output.ogg>\n"
        << "  ArachnoTracker --write-demo <project.arachno>\n"
        << "  ArachnoTracker --render <project.arachno> <output.wav|output.mp3|output.ogg>\n"
        << "  ArachnoTracker --render-stems <project.arachno> <output-dir> [wav|mp3|ogg]\n"
        << "  ArachnoTracker --export-midi <project.arachno> <output.mid>\n"
        << "  ArachnoTracker --project-info <project.arachno>\n"
        << "  ArachnoTracker --show <project.arachno> [pattern] [start-row] [rows]\n"
        << "  ArachnoTracker --edit <input.arachno> <output.arachno> <command>...\n"
        << "  ArachnoTracker --edit-file <input.arachno> <output.arachno> <commands.txt>\n"
        << "  ArachnoTracker --interactive <input.arachno> <output.arachno>\n"
        << "  ArachnoTracker --info\n\n"
        << "Editor commands: pattern N, move ROW TRACK, up/down/left/right [N], note C4 [VEL], inst N, gate ROWS,\n"
        << "                 transpose N [track], fill-scale TRACK START COUNT STRIDE ROOT SCALE INST [VEL] [GATE],\n"
        << "                 euclid TRACK START STEPS PULSES ROOT INST [VEL] [GATE], param NAME VALUE,\n"
        << "                 param-clear [NAME|*], view, clear, write, quit\n"
        << "Native WAV export is built in. MP3 and OGG export use ffmpeg or avconv when available.\n";
}

void printSongInfo(const arachno::Song& song) {
    std::cout
        << "Title: " << song.title << "\n"
        << "Tracks: " << song.tracks.size() << "\n"
        << "Instruments: " << song.instruments.size() << "\n"
        << "Patterns: " << song.patterns.size() << "\n"
        << "Order entries: " << song.order.size() << "\n"
        << "Duration: " << song.durationSeconds() << " seconds\n";
}

void renderSongToPath(const arachno::Song& song, const std::string& outputPath) {
    arachno::AudioEngine engine(song.sampleRate);
    arachno::RenderedAudio audio = engine.renderSong(song);
    arachno::exportAudio(audio, outputPath, arachno::exportFormatFromPath(outputPath));

    std::cout
        << "Exported " << outputPath << " at " << audio.sampleRate
        << " Hz, " << audio.frameCount() << " stereo frames\n";
}

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

    if (sanitized.empty()) {
        return "track";
    }
    return sanitized;
}

std::string normalizedAudioExtension(const std::string& extension) {
    if (extension == "mp3") {
        return ".mp3";
    }
    if (extension == "ogg" || extension == "oga") {
        return ".ogg";
    }
    return ".wav";
}

void renderStemsToDirectory(const arachno::Song& song, const std::string& outputDirectory, const std::string& extension) {
    const std::filesystem::path directory(outputDirectory);
    std::filesystem::create_directories(directory);

    const std::string suffix = normalizedAudioExtension(extension);
    arachno::AudioEngine engine(song.sampleRate);

    for (std::size_t track = 0; track < song.tracks.size(); ++track) {
        const std::string fileName = std::to_string(track + 1)
            + "_"
            + sanitizeStemName(song.tracks[track].name)
            + suffix;
        const std::filesystem::path path = directory / fileName;
        const arachno::RenderedAudio audio = engine.renderTrackStem(song, static_cast<int>(track));
        arachno::exportAudio(audio, path.string(), arachno::exportFormatFromPath(path.string()));
        std::cout << "Exported " << path.string() << "\n";
    }
}

void applyCommandFile(arachno::PatternEditorSession& editor, const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to open command file: " + path);
    }

    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        const std::string trimmed = line.substr(0, line.find_first_of("\r\n"));
        const std::size_t first = trimmed.find_first_not_of(" \t");
        if (first == std::string::npos || trimmed[first] == '#') {
            continue;
        }

        try {
            std::cout << editor.applyCommand(trimmed) << "\n";
        } catch (const std::exception& error) {
            throw std::runtime_error(
                "command file " + path + ":" + std::to_string(lineNumber) + ": " + error.what());
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 1 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
            printUsage();
            return 0;
        }

        if (std::string(argv[1]) == "--info") {
            const arachno::Song song = arachno::makeDemoSong();
            printSongInfo(song);
            return 0;
        }

        if (std::string(argv[1]) == "--demo") {
            if (argc < 3) {
                std::cerr << "--demo requires an output path\n";
                return 2;
            }

            const std::string outputPath = argv[2];
            arachno::Song song = arachno::makeDemoSong();
            renderSongToPath(song, outputPath);
            return 0;
        }

        if (std::string(argv[1]) == "--write-demo") {
            if (argc < 3) {
                std::cerr << "--write-demo requires a project path\n";
                return 2;
            }

            arachno::saveProject(arachno::makeDemoSong(), argv[2]);
            std::cout << "Wrote " << argv[2] << "\n";
            return 0;
        }

        if (std::string(argv[1]) == "--render") {
            if (argc < 4) {
                std::cerr << "--render requires a project path and an output path\n";
                return 2;
            }

            const arachno::Song song = arachno::loadProject(argv[2]);
            renderSongToPath(song, argv[3]);
            return 0;
        }

        if (std::string(argv[1]) == "--render-stems") {
            if (argc < 4) {
                std::cerr << "--render-stems requires a project path and an output directory\n";
                return 2;
            }

            const arachno::Song song = arachno::loadProject(argv[2]);
            renderStemsToDirectory(song, argv[3], argc >= 5 ? argv[4] : "wav");
            return 0;
        }

        if (std::string(argv[1]) == "--export-midi") {
            if (argc < 4) {
                std::cerr << "--export-midi requires a project path and an output path\n";
                return 2;
            }

            const arachno::Song song = arachno::loadProject(argv[2]);
            arachno::exportMidiFile(song, argv[3]);
            std::cout << "Exported " << argv[3] << "\n";
            return 0;
        }

        if (std::string(argv[1]) == "--project-info") {
            if (argc < 3) {
                std::cerr << "--project-info requires a project path\n";
                return 2;
            }

            printSongInfo(arachno::loadProject(argv[2]));
            return 0;
        }

        if (std::string(argv[1]) == "--show") {
            if (argc < 3) {
                std::cerr << "--show requires a project path\n";
                return 2;
            }

            const arachno::Song song = arachno::loadProject(argv[2]);
            const int pattern = argc >= 4 ? std::stoi(argv[3]) : 0;
            const int startRow = argc >= 5 ? std::stoi(argv[4]) : 0;
            const int rows = argc >= 6 ? std::stoi(argv[5]) : -1;
            std::cout << arachno::renderPatternTable(song, pattern, startRow, rows);
            return 0;
        }

        if (std::string(argv[1]) == "--edit") {
            if (argc < 5) {
                std::cerr << "--edit requires input path, output path, and at least one command\n";
                return 2;
            }

            arachno::Song song = arachno::loadProject(argv[2]);
            arachno::PatternEditorSession editor(song);
            for (int i = 4; i < argc; ++i) {
                std::cout << editor.applyCommand(argv[i]) << "\n";
            }
            arachno::saveProject(song, argv[3]);
            std::cout << "Wrote " << argv[3] << "\n";
            return 0;
        }

        if (std::string(argv[1]) == "--edit-file") {
            if (argc < 5) {
                std::cerr << "--edit-file requires input path, output path, and a command file\n";
                return 2;
            }

            arachno::Song song = arachno::loadProject(argv[2]);
            arachno::PatternEditorSession editor(song);
            applyCommandFile(editor, argv[4]);
            arachno::saveProject(song, argv[3]);
            std::cout << "Wrote " << argv[3] << "\n";
            return 0;
        }

        if (std::string(argv[1]) == "--interactive") {
            if (argc < 4) {
                std::cerr << "--interactive requires input path and output path\n";
                return 2;
            }

            arachno::Song song = arachno::loadProject(argv[2]);
            arachno::PatternEditorSession editor(song);
            std::cout << "Interactive editor. Type commands, write, or quit.\n";
            std::string line;
            while (true) {
                const arachno::EditorCursor& cursor = editor.cursor();
                std::cout
                    << "arachno p" << cursor.pattern
                    << " r" << cursor.row
                    << " t" << cursor.track
                    << "> ";
                if (!std::getline(std::cin, line)) {
                    break;
                }
                if (line == "quit" || line == "q") {
                    break;
                }
                if (line == "write" || line == "w") {
                    arachno::saveProject(song, argv[3]);
                    std::cout << "Wrote " << argv[3] << "\n";
                    continue;
                }
                if (line == "view" || line == "v") {
                    std::cout << arachno::renderPatternTable(song, cursor.pattern, cursor.row, 16);
                    continue;
                }
                std::cout << editor.applyCommand(line) << "\n";
            }
            return 0;
        }

        printUsage();
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "ArachnoTracker error: " << error.what() << "\n";
        return 1;
    }
}
