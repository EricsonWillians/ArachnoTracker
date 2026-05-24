#include <exception>
#include <iostream>
#include <sstream>
#include <string>

#include "AudioEngine.h"
#include "Exporter.h"
#include "PatternEditor.h"
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
        << "  ArachnoTracker --project-info <project.arachno>\n"
        << "  ArachnoTracker --edit <input.arachno> <output.arachno> <command>...\n"
        << "  ArachnoTracker --interactive <input.arachno> <output.arachno>\n"
        << "  ArachnoTracker --info\n\n"
        << "Editor commands: pattern N, move ROW TRACK, up/down/left/right [N], note C4 [VEL], inst N, gate ROWS, transpose N [track], clear, write, quit\n"
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

        if (std::string(argv[1]) == "--project-info") {
            if (argc < 3) {
                std::cerr << "--project-info requires a project path\n";
                return 2;
            }

            printSongInfo(arachno::loadProject(argv[2]));
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
