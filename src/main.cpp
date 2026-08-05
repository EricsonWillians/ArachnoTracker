#include <exception>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "AudioEngine.h"
#include "EditorActions.h"
#include "EditorCommandPalette.h"
#include "EditorShortcuts.h"
#include "Exporter.h"
#include "GUI.h"
#include "GmPresetBank.h"
#include "MidiImporter.h"
#include "MidiExporter.h"
#include "PatchIO.h"
#include "PatternEditor.h"
#include "PatternView.h"
#include "ProjectDiagnostics.h"
#include "ProjectIO.h"
#include "Tracker.h"

namespace {

void printUsage() {
    std::cout
        << "ArachnoTracker\n"
        << "Usage:\n"
        << "  ArachnoTracker                      # starts the GUI shell\n"
        << "  ArachnoTracker --demo <output.wav|output.mp3|output.ogg> [template]\n"
        << "  ArachnoTracker --write-demo <project.arachno> [template]\n"
        << "  ArachnoTracker --list-demo-templates\n"
        << "  ArachnoTracker --render <project.arachno> <output.wav|output.mp3|output.ogg>\n"
        << "  ArachnoTracker --render-stems <project.arachno> <output-dir> [wav|mp3|ogg]\n"
        << "  ArachnoTracker --export-midi <project.arachno> <output.mid>\n"
        << "  ArachnoTracker --import-midi <input.mid> <output.arachno> [rows-per-beat] [pattern-rows]\n"
        << "  ArachnoTracker --project-info <project.arachno>\n"
        << "  ArachnoTracker --validate <project.arachno>\n"
        << "  ArachnoTracker --arrangement <project.arachno>\n"
        << "  ArachnoTracker --instruments <project.arachno>\n"
        << "  ArachnoTracker --stats <project.arachno>\n"
        << "  ArachnoTracker --actions\n"
        << "  ArachnoTracker --shortcuts\n"
        << "  ArachnoTracker --palette [query]\n"
        << "  ArachnoTracker --gui [project.arachno]\n"
        << "  ArachnoTracker --gui-window [project.arachno]\n"
        << "  ArachnoTracker --gui-shell [project.arachno]\n"
        << "  ArachnoTracker --export-patch <project.arachno> <instrument> <patch.arachnopatch>\n"
        << "  ArachnoTracker --import-patch <input.arachno> <output.arachno> <patch.arachnopatch> [name]\n"
        << "  ArachnoTracker --replace-patch <input.arachno> <output.arachno> <instrument> <patch.arachnopatch> [name]\n"
        << "  ArachnoTracker --write-gm-presets <output-dir>   # export the 128-preset GM bank as .arachnopatch files\n"
        << "  ArachnoTracker --show <project.arachno> [pattern] [start-row] [rows]\n"
        << "  ArachnoTracker --edit <input.arachno> <output.arachno> <command>...\n"
        << "  ArachnoTracker --edit-file <input.arachno> <output.arachno> <commands.txt>\n"
        << "  ArachnoTracker --interactive <input.arachno> <output.arachno>\n"
        << "  ArachnoTracker --info\n\n"
        << "Editor commands: pattern N, move ROW TRACK, up/down/left/right [N], note C4 [VEL], inst N, gate ROWS, legato [on|off|toggle],\n"
        << "                 select ROW TRACK ROWS TRACKS, copy, cut, paste [ROW] [TRACK], clear-selection,\n"
        << "                 undo, redo,\n"
        << "                 transpose N [track], octave N, fill-scale TRACK START COUNT STRIDE ROOT SCALE INST [VEL] [GATE],\n"
        << "                 euclid TRACK START STEPS PULSES ROOT INST [VEL] [GATE], probability VALUE|clear,\n"
        << "                 retrig COUNT [SPACING] [DECAY], velocity VALUE, vel-nudge DELTA,\n"
        << "                 transpose-selection N, repeat-selection REPEATS [ROW-SPACING] [TRACK-SPACING],\n"
        << "                 param NAME VALUE, fx NAME VALUE, fxp NAME PARAM VALUE,\n"
        << "                 new-pattern NAME ROWS [TRACKS], clone-pattern [NAME], delete-pattern [PATTERN],\n"
        << "                 append-order [PATTERN], insert-order INDEX [PATTERN], remove-order INDEX, set-order ...,\n"
        << "                 tempo BPM, rows-per-beat N, new-track NAME, duplicate-track SRC [NAME],\n"
        << "                 delete-track TRACK, track-name/volume/pan/mute/solo TRACK VALUE, clear-track TRACK,\n"
        << "                 resize-pattern ROWS,\n"
        << "                 title TEXT, author TEXT, description TEXT, notes TEXT,\n"
        << "                 new-instrument NAME, clone-instrument SRC [NAME], instrument-name INST NAME,\n"
        << "                 instrument-wave INST A|B|C|D WAVE, instrument-param INST NAME VALUE,\n"
        << "                 param-clear [NAME|*], fx-clear [NAME|*], view, clear, write, quit\n"
        << "Native WAV export is built in. MP3 and OGG export use ffmpeg or avconv when available.\n";
}

void printSongInfo(const arachno::Song& song) {
    std::cout
        << "Title: " << song.title << "\n"
        << "Author: " << (song.author.empty() ? "<unset>" : song.author) << "\n"
        << "Description: " << (song.description.empty() ? "<unset>" : song.description) << "\n"
        << "Notes: " << (song.notes.empty() ? "<unset>" : song.notes) << "\n"
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

int addPatchInstrument(arachno::Song& song, arachno::SynthPatch patch, const std::string& nameOverride) {
    if (!nameOverride.empty()) {
        patch.name = nameOverride;
    }

    arachno::Instrument instrument;
    instrument.id = static_cast<int>(song.instruments.size());
    instrument.patch = patch;
    song.instruments.push_back(instrument);
    return instrument.id;
}

void replacePatchInstrument(
    arachno::Song& song,
    int instrumentIndex,
    arachno::SynthPatch patch,
    const std::string& nameOverride) {
    if (instrumentIndex < 0 || instrumentIndex >= static_cast<int>(song.instruments.size())) {
        throw std::out_of_range("instrument index is out of range");
    }
    if (!nameOverride.empty()) {
        patch.name = nameOverride;
    }
    song.instruments[static_cast<std::size_t>(instrumentIndex)].patch = patch;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 1) {
            arachno::ApplicationSession session;
            return arachno::runGui(session, std::cin, std::cout);
        }

        if (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
            printUsage();
            return 0;
        }

        if (std::string(argv[1]) == "--info") {
            const arachno::Song song = arachno::makeDemoSong();
            printSongInfo(song);
            return 0;
        }

        if (std::string(argv[1]) == "--actions") {
            std::cout << arachno::renderEditorActionTable();
            return 0;
        }

        if (std::string(argv[1]) == "--shortcuts") {
            const std::vector<arachno::ShortcutBinding> bindings = arachno::defaultEditorShortcuts();
            const std::vector<arachno::ShortcutConflict> conflicts = arachno::validateEditorShortcuts(bindings);
            std::cout << arachno::renderEditorShortcutTable(bindings);
            if (!conflicts.empty()) {
                std::cout << "\nConflicts\n";
                for (const arachno::ShortcutConflict& conflict : conflicts) {
                    std::cout << conflict.shortcut << ":";
                    for (const std::string& actionId : conflict.actionIds) {
                        std::cout << " " << actionId;
                    }
                    std::cout << "\n";
                }
            }
            return conflicts.empty() ? 0 : 1;
        }

        if (std::string(argv[1]) == "--palette") {
            arachno::Song song = arachno::makeDemoSong();
            arachno::PatternEditorSession editor(song);
            const std::string query = argc >= 3 ? argv[2] : "";
            std::cout << arachno::renderEditorCommandPalette(
                arachno::buildEditorCommandPalette(editor, arachno::defaultEditorShortcuts(), query));
            return 0;
        }

        if (std::string(argv[1]) == "--gui"
            || std::string(argv[1]) == "--gui-window"
            || std::string(argv[1]) == "--gui-shell") {
            arachno::ApplicationSession session;
            if (argc >= 3) {
                const arachno::AppOperationResult loaded = session.loadProjectFile(argv[2]);
                if (!loaded.ok) {
                    std::cerr << "Failed to load project: " << loaded.error << "\n";
                    return 1;
                }
            }
            arachno::GuiFrontend frontend = arachno::GuiFrontend::Auto;
            if (std::string(argv[1]) == "--gui-window") {
                frontend = arachno::GuiFrontend::X11Window;
            } else if (std::string(argv[1]) == "--gui-shell") {
                frontend = arachno::GuiFrontend::Shell;
            }
            return arachno::runGui(session, std::cin, std::cout, frontend);
        }

        if (std::string(argv[1]) == "--list-demo-templates") {
            const std::vector<std::string> templates = arachno::demoTemplateNames();
            std::cout << "Demo templates:\n";
            for (const std::string& name : templates) {
                std::cout << "  " << name << "\n";
            }
            return 0;
        }

        if (std::string(argv[1]) == "--demo") {
            if (argc < 3) {
                std::cerr << "--demo requires an output path\n";
                return 2;
            }

            const std::string outputPath = argv[2];
            const std::string templateName = argc >= 4 ? argv[3] : "darkwave_foundation";
            arachno::Song song = arachno::makeTemplateSong(templateName);
            renderSongToPath(song, outputPath);
            return 0;
        }

        if (std::string(argv[1]) == "--write-demo") {
            if (argc < 3) {
                std::cerr << "--write-demo requires a project path\n";
                return 2;
            }

            const std::string templateName = argc >= 4 ? argv[3] : "darkwave_foundation";
            arachno::saveProject(arachno::makeTemplateSong(templateName), argv[2]);
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

        if (std::string(argv[1]) == "--import-midi") {
            if (argc < 4) {
                std::cerr << "--import-midi requires an input MIDI path and an output project path\n";
                return 2;
            }

            arachno::MidiImportOptions options;
            if (argc >= 5) {
                options.rowsPerBeat = std::stoi(argv[4]);
            }
            if (argc >= 6) {
                options.patternRows = std::stoi(argv[5]);
            }
            const arachno::MidiImportReport report = arachno::importMidiFile(argv[2], options);
            arachno::saveProject(report.song, argv[3]);
            std::cout
                << "Imported " << argv[2]
                << " -> " << argv[3]
                << " (" << report.importedNoteCount << " notes, "
                << report.importedTrackCount << " tracks, "
                << report.song.instruments.size() << " instruments)\n";
            for (const arachno::MidiImportTrackMapping& mapping : report.trackMappings) {
                std::cout
                    << "  track " << mapping.trackIndex
                    << " \"" << mapping.trackName << "\""
                    << " -> instrument " << mapping.instrumentIndex
                    << " \"" << mapping.instrumentName << "\""
                    << " [src track " << mapping.midiSourceTrack
                    << " ch " << (mapping.midiChannel + 1)
                    << " prog " << (mapping.dominantProgram + 1)
                    << "]\n";
            }
            for (const arachno::MidiImportWarning& warning : report.warnings) {
                std::cout << "warning: " << warning.message << "\n";
            }
            return 0;
        }

        if (std::string(argv[1]) == "--write-gm-presets") {
            if (argc < 3) {
                std::cerr << "--write-gm-presets requires an output directory\n";
                return 2;
            }
            const std::filesystem::path outDir(argv[2]);
            std::filesystem::create_directories(outDir);
            for (int program = 0; program < 128; ++program) {
                arachno::SynthPatch patch = arachno::gmPresetForProgram(program, 60.0);
                const std::string fileName = (program < 10 ? "00" : (program < 100 ? "0" : ""))
                    + std::to_string(program) + "_"
                    + [&] {
                        std::string slug = arachno::gmProgramName(program);
                        for (char& c : slug) {
                            c = (c == ' ' || c == '(' || c == ')' || c == '+') ? '_' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                        }
                        return slug;
                    }()
                    + ".arachnopatch";
                arachno::savePatch(patch, (outDir / fileName).string());
            }
            std::cout << "Wrote 128 GM presets to " << outDir.string() << "\n";
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

        if (std::string(argv[1]) == "--validate") {
            if (argc < 3) {
                std::cerr << "--validate requires a project path\n";
                return 2;
            }

            const arachno::Song song = arachno::loadProject(argv[2]);
            const std::vector<arachno::ProjectDiagnostic> diagnostics = arachno::validateProject(song);
            std::cout << arachno::formatDiagnostics(diagnostics);
            return arachno::hasErrors(diagnostics) ? 1 : 0;
        }

        if (std::string(argv[1]) == "--arrangement") {
            if (argc < 3) {
                std::cerr << "--arrangement requires a project path\n";
                return 2;
            }

            std::cout << arachno::renderArrangementTable(arachno::loadProject(argv[2]));
            return 0;
        }

        if (std::string(argv[1]) == "--instruments") {
            if (argc < 3) {
                std::cerr << "--instruments requires a project path\n";
                return 2;
            }

            std::cout << arachno::renderInstrumentTable(arachno::loadProject(argv[2]));
            return 0;
        }

        if (std::string(argv[1]) == "--stats") {
            if (argc < 3) {
                std::cerr << "--stats requires a project path\n";
                return 2;
            }

            std::cout << arachno::renderProjectStats(arachno::loadProject(argv[2]));
            return 0;
        }

        if (std::string(argv[1]) == "--export-patch") {
            if (argc < 5) {
                std::cerr << "--export-patch requires a project path, instrument, and patch path\n";
                return 2;
            }

            const arachno::Song song = arachno::loadProject(argv[2]);
            const int instrument = std::stoi(argv[3]);
            if (instrument < 0 || instrument >= static_cast<int>(song.instruments.size())) {
                throw std::out_of_range("instrument index is out of range");
            }
            arachno::savePatch(song.instruments[static_cast<std::size_t>(instrument)].patch, argv[4]);
            std::cout << "Exported " << argv[4] << "\n";
            return 0;
        }

        if (std::string(argv[1]) == "--import-patch") {
            if (argc < 5) {
                std::cerr << "--import-patch requires input project, output project, and patch path\n";
                return 2;
            }

            arachno::Song song = arachno::loadProject(argv[2]);
            const int instrument = addPatchInstrument(
                song,
                arachno::loadPatch(argv[4]),
                argc >= 6 ? argv[5] : "");
            arachno::saveProject(song, argv[3]);
            std::cout << "Imported " << argv[4] << " as instrument " << instrument << "\n";
            return 0;
        }

        if (std::string(argv[1]) == "--replace-patch") {
            if (argc < 6) {
                std::cerr << "--replace-patch requires input project, output project, instrument, and patch path\n";
                return 2;
            }

            arachno::Song song = arachno::loadProject(argv[2]);
            replacePatchInstrument(
                song,
                std::stoi(argv[4]),
                arachno::loadPatch(argv[5]),
                argc >= 7 ? argv[6] : "");
            arachno::saveProject(song, argv[3]);
            std::cout << "Replaced instrument " << argv[4] << " from " << argv[5] << "\n";
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
                if (line == "instruments" || line == "i") {
                    std::cout << arachno::renderInstrumentTable(song);
                    continue;
                }
                const arachno::EditorCommandResult result = editor.tryApplyCommand(line);
                if (result.ok) {
                    std::cout << result.message << "\n";
                } else {
                    std::cout << "error: " << result.error << "\n";
                }
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
