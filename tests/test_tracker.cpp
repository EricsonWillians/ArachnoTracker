#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <vector>

#include "Exporter.h"
#include "MidiExporter.h"
#include "Note.h"
#include "PatchIO.h"
#include "PatternEditor.h"
#include "PatternView.h"
#include "ProjectDiagnostics.h"
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

void testArrangementCommands() {
    arachno::Song song = arachno::makeDemoSong();
    arachno::PatternEditorSession editor(song);

    editor.applyCommand("tempo 96");
    editor.applyCommand("rows-per-beat 8");
    assert(song.bpm == 96.0);
    assert(song.rowsPerBeat == 8);

    editor.applyCommand("new-pattern Bridge 32");
    assert(editor.cursor().pattern == 1);
    assert(song.patterns.size() == 2);
    assert(song.patterns[1].name() == "Bridge");
    assert(song.patterns[1].rowCount() == 32);
    assert(song.patterns[1].trackCount() == static_cast<int>(song.tracks.size()));

    editor.applyCommand("pattern-name Drop");
    assert(song.patterns[1].name() == "Drop");

    editor.applyCommand("note C4 0.8");
    assert(song.patterns[1].step(0, 0).note.has_value());

    editor.applyCommand("clone-pattern DropCopy");
    assert(editor.cursor().pattern == 2);
    assert(song.patterns[2].name() == "DropCopy");
    assert(song.patterns[2].step(0, 0).note.has_value());

    editor.applyCommand("append-order");
    assert(song.order.back() == 2);
    editor.applyCommand("set-order 0 1 2");
    assert((song.order == std::vector<int> {0, 1, 2}));

    editor.applyCommand("track-volume 1 1.25");
    editor.applyCommand("track-pan 1 -0.5");
    editor.applyCommand("track-mute 1 true");
    editor.applyCommand("track-solo 2 on");
    assert(song.tracks[1].volume == 1.25);
    assert(song.tracks[1].pan == -0.5);
    assert(song.tracks[1].muted);
    assert(song.tracks[2].solo);
}

void testTrackLifecycleCommands() {
    arachno::Song song = arachno::makeDemoSong();
    arachno::PatternEditorSession editor(song);

    editor.applyCommand("new-track Counter");
    assert(song.tracks.size() == 4);
    assert(song.tracks[3].name == "Counter");
    assert(song.patterns.front().trackCount() == 4);

    editor.applyCommand("move 0 3");
    editor.applyCommand("note C5 0.8");
    assert(song.patterns.front().step(0, 3).note.has_value());

    editor.applyCommand("duplicate-track 3 CounterCopy");
    assert(song.tracks.size() == 5);
    assert(song.tracks[4].name == "CounterCopy");
    assert(song.patterns.front().trackCount() == 5);
    assert(song.patterns.front().step(0, 4).note.has_value());
    assert(song.patterns.front().step(0, 4).note->midi == 72);

    editor.applyCommand("track-name 4 Answer");
    assert(song.tracks[4].name == "Answer");

    editor.applyCommand("clear-track 4");
    assert(!song.patterns.front().step(0, 4).note.has_value());

    editor.applyCommand("resize-pattern 96");
    assert(song.patterns.front().rowCount() == 96);
    assert(song.patterns.front().trackCount() == 5);
}

void testInstrumentCommands() {
    arachno::Song song = arachno::makeDemoSong();
    arachno::PatternEditorSession editor(song);

    editor.applyCommand("new-instrument Glass");
    assert(song.instruments.size() == 4);
    assert(song.instruments[3].id == 3);
    assert(song.instruments[3].patch.name == "Glass");

    editor.applyCommand("instrument-wave 3 A sine");
    editor.applyCommand("instrument-wave 3 B triangle");
    editor.applyCommand("instrument-param 3 cutoff 0.91");
    editor.applyCommand("instrument-param 3 vibrato 5");
    assert(song.instruments[3].patch.oscillatorA == arachno::Waveform::Sine);
    assert(song.instruments[3].patch.oscillatorB == arachno::Waveform::Triangle);
    assert(song.instruments[3].patch.cutoff == 0.91);
    assert(song.instruments[3].patch.vibratoCents == 5.0);

    editor.applyCommand("clone-instrument 3 GlassCopy");
    assert(song.instruments.size() == 5);
    assert(song.instruments[4].patch.name == "GlassCopy");
    assert(song.instruments[4].patch.cutoff == 0.91);

    editor.applyCommand("instrument-name 4 Air");
    assert(song.instruments[4].patch.name == "Air");

    const std::string table = arachno::renderInstrumentTable(song);
    assert(table.find("Glass") != std::string::npos);
    assert(table.find("Air") != std::string::npos);
}

void testPatchRoundTrip() {
    arachno::SynthPatch patch;
    patch.name = "Glass";
    patch.oscillatorA = arachno::Waveform::Sine;
    patch.oscillatorB = arachno::Waveform::Triangle;
    patch.cutoff = 0.91;
    patch.vibratoCents = 5.0;
    patch.ampEnvelope.attack = 0.03;

    const std::filesystem::path path = std::filesystem::temp_directory_path() / "arachno-glass.arachnopatch";
    arachno::savePatch(patch, path.string());
    const arachno::SynthPatch loaded = arachno::loadPatch(path.string());

    assert(loaded.name == "Glass");
    assert(loaded.oscillatorA == arachno::Waveform::Sine);
    assert(loaded.oscillatorB == arachno::Waveform::Triangle);
    assert(loaded.cutoff == 0.91);
    assert(loaded.vibratoCents == 5.0);
    assert(loaded.ampEnvelope.attack == 0.03);
    std::filesystem::remove(path);
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

void testArrangementView() {
    arachno::Song song = arachno::makeDemoSong();
    arachno::PatternEditorSession editor(song);
    editor.applyCommand("new-pattern Bridge 32");
    editor.applyCommand("append-order");

    const std::string table = arachno::renderArrangementTable(song);
    assert(table.find("Arrangement") != std::string::npos);
    assert(table.find("Bridge") != std::string::npos);
    assert(table.find("total rows") != std::string::npos);
}

void testProjectDiagnostics() {
    arachno::Song song = arachno::makeDemoSong();
    const std::vector<arachno::ProjectDiagnostic> ok = arachno::validateProject(song);
    assert(!arachno::hasErrors(ok));

    song.patterns.front().step(0, 0).instrument = 99;
    const std::vector<arachno::ProjectDiagnostic> broken = arachno::validateProject(song);
    assert(arachno::hasErrors(broken));
    const std::string formatted = arachno::formatDiagnostics(broken);
    assert(formatted.find("missing instrument") != std::string::npos);
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
    testArrangementCommands();
    testTrackLifecycleCommands();
    testInstrumentCommands();
    testPatchRoundTrip();
    testCompositionalTransforms();
    testPatternView();
    testArrangementView();
    testProjectDiagnostics();
    testMidiExport();
    return 0;
}
