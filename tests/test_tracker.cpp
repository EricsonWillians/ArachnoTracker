#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>

#include "Exporter.h"
#include "MidiExporter.h"
#include "Note.h"
#include "PatternEditor.h"
#include "PatternView.h"
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

void testTrackStemRendering() {
    arachno::Song song = arachno::makeDemoSong();
    arachno::AudioEngine engine(song.sampleRate);

    const arachno::RenderedAudio bass = engine.renderTrackStem(song, 0);
    const arachno::RenderedAudio lead = engine.renderTrackStem(song, 1);
    assert(bass.frameCount() == lead.frameCount());

    bool bassHasSignal = false;
    bool differs = false;
    for (std::size_t i = 0; i < bass.interleavedStereo.size(); ++i) {
        bassHasSignal = bassHasSignal || bass.interleavedStereo[i] > 0.001f || bass.interleavedStereo[i] < -0.001f;
        differs = differs || std::abs(bass.interleavedStereo[i] - lead.interleavedStereo[i]) > 0.001f;
    }
    assert(bassHasSignal);
    assert(differs);

    song.tracks[0].muted = true;
    const arachno::RenderedAudio mixWithoutBass = engine.renderSong(song);
    const arachno::RenderedAudio mutedBassStem = engine.renderTrackStem(song, 0);
    assert(mixWithoutBass.frameCount() == mutedBassStem.frameCount());

    bool mutedStemHasSignal = false;
    for (float sample : mutedBassStem.interleavedStereo) {
        mutedStemHasSignal = mutedStemHasSignal || sample > 0.001f || sample < -0.001f;
    }
    assert(mutedStemHasSignal);
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

void testStepAutomation() {
    arachno::Song song = arachno::makeDemoSong();
    arachno::PatternEditorSession editor(song);

    editor.applyCommand("move 0 1");
    editor.applyCommand("param cutoff 0.95");
    editor.applyCommand("param vibrato 6");
    assert(song.patterns.front().step(0, 1).automation.at("cutoff") == 0.95);
    assert(song.patterns.front().step(0, 1).automation.at("vibrato") == 6.0);

    const std::string table = arachno::renderPatternTable(song, 0, 0, 1);
    assert(table.find("*") != std::string::npos);

    const std::filesystem::path path = std::filesystem::temp_directory_path() / "arachno-automation.arachno";
    arachno::saveProject(song, path.string());
    const arachno::Song loaded = arachno::loadProject(path.string());
    assert(loaded.patterns.front().step(0, 1).automation.at("cutoff") == 0.95);
    std::filesystem::remove(path);

    editor.applyCommand("param-clear cutoff");
    assert(song.patterns.front().step(0, 1).automation.count("cutoff") == 0);
    editor.applyCommand("param-clear *");
    assert(song.patterns.front().step(0, 1).automation.empty());
}

void testCompositionalTransforms() {
    arachno::Song song = arachno::makeDemoSong();
    arachno::PatternEditorSession editor(song);

    editor.applyCommand("fill-scale 1 0 4 2 C4 minor 1 0.75 0.5");
    assert(song.patterns.front().step(0, 1).note->midi == 60);
    assert(song.patterns.front().step(2, 1).note->midi == 62);
    assert(song.patterns.front().step(4, 1).note->midi == 63);
    assert(song.patterns.front().step(6, 1).note->midi == 65);

    editor.applyCommand("euclid 0 0 8 3 C2 0 0.9 0.4");
    int hits = 0;
    for (int row = 0; row < 8; ++row) {
        const arachno::PatternStep& step = song.patterns.front().step(row, 0);
        if (step.note.has_value() && step.note->midi == arachno::noteNameToMidi("C2")) {
            ++hits;
        }
    }
    assert(hits == 3);
}

void testPatternView() {
    const arachno::Song song = arachno::makeDemoSong();
    const std::string table = arachno::renderPatternTable(song, 0, 0, 4);
    assert(table.find("Pattern 0") != std::string::npos);
    assert(table.find("Bass") != std::string::npos);
    assert(table.find("C2") != std::string::npos);
}

void testMidiExport() {
    const arachno::Song song = arachno::makeDemoSong();
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "arachno-smoke.mid";
    arachno::exportMidiFile(song, path.string());

    std::ifstream in(path, std::ios::binary);
    char header[4] {};
    in.read(header, 4);
    assert(std::string(header, 4) == "MThd");
    assert(std::filesystem::file_size(path) > 32);
    std::filesystem::remove(path);
}

int main() {
    testNotes();
    testTrackerModel();
    testRenderDemoSong();
    testWavExport();
    testTrackStemRendering();
    testProjectRoundTrip();
    testPatternEditorCommands();
    testStepAutomation();
    testCompositionalTransforms();
    testPatternView();
    testMidiExport();
    return 0;
}
