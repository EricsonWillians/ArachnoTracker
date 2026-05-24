#include <cassert>
#include <filesystem>

#include "Exporter.h"
#include "Note.h"
#include "PatternEditor.h"
#include "ProjectIO.h"
#include "Tracker.h"

void testRenderDemoSong();

void testNotes() {
    assert(arachno::noteNameToMidi("C4") == 60);
    assert(arachno::noteNameToMidi("A4") == 69);
    assert(arachno::midiNoteName(60) == "C4");
}

void testTrackerModel() {
    arachno::Tracker tracker;
    tracker.song().bpm = 120.0;
    tracker.song().rowsPerBeat = 4;
    const int track = tracker.addTrack("Lead");

    arachno::SynthPatch patch;
    const int instrument = tracker.addInstrument(patch);

    arachno::Pattern pattern("Main", 16, 1);
    pattern.step(0, track).note = arachno::Note(60, 0.8f);
    pattern.step(0, track).instrument = instrument;
    const int patternIndex = tracker.addPattern(pattern);
    tracker.appendPatternToOrder(patternIndex);

    assert(tracker.song().totalRows() == 16);
    assert(tracker.song().durationSeconds() == 2.0);
}

void testWavExport() {
    const arachno::Song song = arachno::makeDemoSong();
    arachno::AudioEngine engine(song.sampleRate);
    const arachno::RenderedAudio audio = engine.renderSong(song);

    const std::filesystem::path path = std::filesystem::temp_directory_path() / "arachno-smoke.wav";
    arachno::writeWavFile(audio, path.string());
    assert(std::filesystem::exists(path));
    assert(std::filesystem::file_size(path) > 44);
    std::filesystem::remove(path);
}

void testProjectRoundTrip() {
    const arachno::Song original = arachno::makeDemoSong();
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "arachno-roundtrip.arachno";
    arachno::saveProject(original, path.string());
    const arachno::Song loaded = arachno::loadProject(path.string());

    assert(loaded.title == original.title);
    assert(loaded.tracks.size() == original.tracks.size());
    assert(loaded.instruments.size() == original.instruments.size());
    assert(loaded.patterns.size() == original.patterns.size());
    assert(loaded.order == original.order);
    assert(loaded.patterns.front().step(0, 0).note.has_value());
    assert(loaded.patterns.front().step(0, 0).note->midi == 36);
    std::filesystem::remove(path);
}

void testPatternEditorCommands() {
    arachno::Song song = arachno::makeDemoSong();
    arachno::PatternEditorSession editor(song);

    editor.applyCommand("move 2 1");
    editor.applyCommand("inst 1");
    editor.applyCommand("gate 1.5");
    editor.applyCommand("note C5 0.7");
    assert(song.patterns.front().step(2, 1).note.has_value());
    assert(song.patterns.front().step(2, 1).note->midi == 72);
    assert(song.patterns.front().step(2, 1).instrument == 1);
    assert(song.patterns.front().step(2, 1).gate == 1.5);

    editor.applyCommand("transpose -12");
    assert(song.patterns.front().step(2, 1).note->midi == 60);
    editor.applyCommand("clear");
    assert(!song.patterns.front().step(2, 1).note.has_value());
}

int main() {
    testNotes();
    testTrackerModel();
    testRenderDemoSong();
    testWavExport();
    testProjectRoundTrip();
    testPatternEditorCommands();
    return 0;
}
