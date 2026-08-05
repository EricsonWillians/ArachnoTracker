#include <cassert>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "AppActions.h"
#include "AppAsync.h"
#include "AppEvent.h"
#include "ApplicationSession.h"
#include "AutoSave.h"
#include "AppTask.h"
#include "AudioRuntime.h"
#include "EditorActions.h"
#include "EditorCommandPalette.h"
#include "EditorShortcuts.h"
#include "EditorViewModel.h"
#include "Exporter.h"
#include "ExportWorkflow.h"
#include "FileCompatibility.h"
#include "GUI.h"
#include "GmPresetBank.h"
#include "MidiImporter.h"
#include "MidiExporter.h"
#include "Note.h"
#include "PatchIO.h"
#include "PatternEditor.h"
#include "PatternView.h"
#include "ProjectDiagnostics.h"
#include "ProjectIO.h"
#include "ProjectLifecycle.h"
#include "RealtimePlayback.h"
#include "ScriptIntegration.h"
#include "StepEffects.h"
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

void testRealtimePlaybackContract() {
    arachno::Song song = arachno::makeDemoSong();
    arachno::RealtimePlaybackSession playback(song.sampleRate);
    playback.setSong(&song);

    arachno::PlaybackSnapshot initial = playback.snapshot();
    assert(initial.hasSong);
    assert(initial.state == arachno::TransportState::Stopped);
    assert(initial.position.absoluteRow == 0.0);
    assert(initial.sampleRate == song.sampleRate);

    playback.seekRows(4.0);
    playback.setLoopRows(4.0, 8.0);
    playback.play();
    assert(playback.snapshot().state == arachno::TransportState::Playing);
    assert(playback.snapshot().loop.enabled);

    std::vector<float> left(4096, 0.0f);
    std::vector<float> right(4096, 0.0f);
    playback.render(left.data(), right.data(), static_cast<int>(left.size()));

    bool heardPlayback = false;
    for (std::size_t index = 0; index < left.size(); ++index) {
        heardPlayback = heardPlayback || std::abs(left[index]) > 0.0001f || std::abs(right[index]) > 0.0001f;
    }
    assert(heardPlayback);
    assert(playback.snapshot().position.absoluteRow >= 4.0);
    assert(playback.snapshot().position.absoluteRow < 8.0);

    playback.pause();
    assert(playback.snapshot().state == arachno::TransportState::Paused);

    const bool previewedInstrument = playback.previewInstrument(0, arachno::noteNameToMidi("C2"), 0.9f, 0.2);
    assert(previewedInstrument);
    const arachno::AuditionResult auditionedInstrument = playback.auditionInstrument(0, arachno::noteNameToMidi("C2"), 0.9f, 0.2);
    assert(auditionedInstrument.ok);
    assert(auditionedInstrument.request.label == song.instruments[0].patch.name);
    const arachno::AuditionResult invalidAudition = playback.auditionInstrument(99);
    assert(!invalidAudition.ok);
    assert(!invalidAudition.error.empty());

    const arachno::AuditionResult drumAudition = playback.auditionDrumPatch(song.instruments[1].patch);
    assert(drumAudition.ok);
    const arachno::RenderedAudio clip = playback.renderAuditionClip(drumAudition.request, 0.25);
    assert(clip.sampleRate == song.sampleRate);
    assert(clip.frameCount() > 0);
    bool clipHasSignal = false;
    for (float sample : clip.interleavedStereo) {
        clipHasSignal = clipHasSignal || std::abs(sample) > 0.0001f;
    }
    assert(clipHasSignal);

    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    playback.render(left.data(), right.data(), static_cast<int>(left.size()));
    bool heardPreview = false;
    for (std::size_t index = 0; index < left.size(); ++index) {
        heardPreview = heardPreview || std::abs(left[index]) > 0.0001f || std::abs(right[index]) > 0.0001f;
    }
    assert(heardPreview);

    assert(playback.previewStep(0, 0, 0));
    assert(!playback.previewInstrument(99));

    playback.stop();
    assert(playback.snapshot().state == arachno::TransportState::Stopped);
    assert(playback.snapshot().position.absoluteRow == 4.0);
}

void testSynthParameterSurface() {
    arachno::SynthPatch patch;
    const std::vector<std::pair<std::string, double>> parameters {
        {"osc_a_enabled", 1.0},
        {"osc_b_enabled", 0.0},
        {"osc_c_enabled", 1.0},
        {"osc_d_enabled", 0.0},
        {"oscillator_mix", 0.37},
        {"oscillator_c_mix", 0.41},
        {"oscillator_d_mix", 0.29},
        {"osc_a_level", 0.82},
        {"osc_b_level", 0.68},
        {"osc_c_level", 0.52},
        {"osc_d_level", 0.43},
        {"detune_cents", 9.0},
        {"detune_c_cents", -7.0},
        {"detune_d_cents", 6.0},
        {"osc_a_detune_cents", 4.0},
        {"osc_b_detune_cents", -3.0},
        {"osc_c_detune_cents", 8.0},
        {"osc_d_detune_cents", -9.0},
        {"pulse_width", 0.34},
        {"osc_a_pulse_width", 0.32},
        {"osc_b_pulse_width", 0.41},
        {"osc_c_pulse_width", 0.52},
        {"osc_d_pulse_width", 0.63},
        {"pwm_depth", 0.27},
        {"osc_a_pwm_depth", 0.22},
        {"osc_b_pwm_depth", 0.19},
        {"osc_c_pwm_depth", 0.31},
        {"osc_d_pwm_depth", 0.44},
        {"fm_enabled", 1.0},
        {"fm_amount", 0.39},
        {"fm_ratio", 3.2},
        {"fm_feedback", 0.41},
        {"fm_algorithm", 2.0},
        {"chorus_enabled", 1.0},
        {"chorus_mix", 0.28},
        {"chorus_rate", 0.66},
        {"chorus_depth", 0.42},
        {"chorus_feedback", 0.19},
        {"chorus_delay", 0.44},
        {"chorus_width", 0.71},
        {"chorus_ensemble", 0.36},
        {"unison_voices", 4.0},
        {"unison_detune_cents", 14.0},
        {"stereo_spread", 0.71},
        {"sub_enabled", 1.0},
        {"sub_oscillator", 0.38},
        {"noise_enabled", 1.0},
        {"noise", 0.12},
        {"noise_tone", 0.69},
        {"cutoff", 0.84},
        {"resonance", 0.36},
        {"filter_mode", 2.0},
        {"filter_drive", 0.24},
        {"filter_keytrack", 0.53},
        {"filter_envelope", 0.46},
        {"lfo_filter_depth", 0.35},
        {"lfo_pan_depth", 0.28},
        {"pitch_envelope_semitones", 11.0},
        {"pitch_envelope_decay", 0.14},
        {"lfo_rate", 4.8},
        {"vibrato_cents", 24.0},
        {"tremolo_depth", 0.22},
        {"ring_enabled", 1.0},
        {"ring_mod", 0.31},
        {"hard_sync_enabled", 1.0},
        {"hard_sync", 0.52},
        {"drive", 0.47},
        {"osc_a_drive", 0.26},
        {"osc_b_drive", 0.34},
        {"osc_c_drive", 0.18},
        {"osc_d_drive", 0.41},
        {"wavefold", 0.29},
        {"bit_crush_enabled", 1.0},
        {"bit_crush", 0.37},
        {"sample_rate_reduction", 0.21},
        {"comb_mix", 0.33},
        {"comb_time", 0.09},
        {"comb_feedback", 0.42},
        {"delay_mix", 0.16},
        {"delay_time", 0.29},
        {"delay_feedback", 0.35},
        {"delay_tone", 0.62},
        {"delay_stereo", 0.42},
        {"delay_mod_depth", 0.48},
        {"delay_drive", 0.27},
        {"delay_ducking", 0.23},
        {"reverb_mix", 0.14},
        {"reverb_size", 0.68},
        {"reverb_damping", 0.47},
        {"reverb_pre_delay", 0.12},
        {"reverb_diffusion", 0.61},
        {"reverb_width", 0.58},
        {"reverb_shimmer", 0.22},
        {"reverb_mod_depth", 0.24},
        {"high_pass", 0.18},
        {"click", 0.14},
        {"transient_shape", 0.35},
        {"transient_noise", 0.28},
        {"transient_pitch_semitones", 9.0},
        {"transient_pitch_decay", 0.03},
        {"transient_burst_count", 4.0},
        {"transient_burst_spacing", 0.004},
        {"transient_burst_decay", 0.64},
        {"transient_tone", 0.77},
        {"transient_decay", 0.04},
        {"analog_color", 0.58},
        {"vintage_drift", 0.63},
        {"wow_flutter", 0.21},
        {"tone_tilt", -0.24},
        {"tape_color", 0.32},
        {"air_boost", 0.27},
        {"low_punch", 0.35},
        {"gain", 0.67},
        {"pan", -0.37},
        {"amp_attack", 0.012},
        {"amp_decay", 0.18},
        {"amp_sustain", 0.61},
        {"amp_release", 0.27},
        {"sustain_hold", 0.45},
        {"filter_attack", 0.01},
        {"filter_decay", 0.16},
        {"filter_sustain", 0.58},
        {"filter_release", 0.24},
        {"portamento", 0.06},
        {"portamento_legato", 1.0},
        {"fm_decay", 0.35},
        {"velocity_to_fm", 0.5},
        {"mono_mode", 1.0},
    };
    for (const auto& [name, value] : parameters) {
        assert(arachno::setSynthPatchParameter(patch, name, value));
    }
    assert(!arachno::setSynthPatchParameter(patch, "totally_unknown_param", 0.5));
}

void testSynthStereoAndHeadroom() {
    auto peakAndBalance = [](const arachno::RenderedAudio& clip) {
        double peak = 0.0;
        double sumLeft = 0.0;
        double sumRight = 0.0;
        double sideMetric = 0.0;
        for (std::size_t i = 0; i + 1 < clip.interleavedStereo.size(); i += 2) {
            const double left = std::abs(static_cast<double>(clip.interleavedStereo[i]));
            const double right = std::abs(static_cast<double>(clip.interleavedStereo[i + 1]));
            peak = std::max(peak, std::max(left, right));
            sumLeft += left;
            sumRight += right;
            sideMetric += std::abs(left - right);
        }
        return std::array<double, 4> {
            peak,
            sumLeft,
            sumRight,
            sideMetric / std::max<std::size_t>(1, clip.frameCount())};
    };

    arachno::RealtimePlaybackSession playback(48000);
    arachno::SynthPatch base;
    base.oscillatorAEnabled = true;
    base.oscillatorBEnabled = false;
    base.oscillatorCEnabled = false;
    base.oscillatorDEnabled = false;
    base.oscillatorA = arachno::Waveform::Saw;
    base.fmEnabled = false;
    base.ringEnabled = false;
    base.hardSyncEnabled = false;
    base.chorusEnabled = false;
    base.bitCrushEnabled = false;
    base.noiseEnabled = false;
    base.subEnabled = false;
    base.unisonVoices = 1;
    base.stereoSpread = 0.0;
    base.gain = 0.7;
    base.pan = 0.0;

    arachno::AuditionRequest req;
    req.note = arachno::Note(arachno::noteNameToMidi("C3"), 1.0f);
    req.patch = base;
    req.gateSeconds = 0.3;

    arachno::SynthPatch hardLeft = base;
    hardLeft.pan = -1.0;
    req.patch = hardLeft;
    const arachno::RenderedAudio leftClip = playback.renderAuditionClip(req, 0.4);
    const std::array<double, 4> leftMetrics = peakAndBalance(leftClip);
    assert(leftMetrics[1] > leftMetrics[2] * 1.25);

    arachno::SynthPatch hardRight = base;
    hardRight.pan = 1.0;
    req.patch = hardRight;
    const arachno::RenderedAudio rightClip = playback.renderAuditionClip(req, 0.4);
    const std::array<double, 4> rightMetrics = peakAndBalance(rightClip);
    assert(rightMetrics[2] > rightMetrics[1] * 1.25);

    arachno::SynthPatch noSpread = base;
    noSpread.unisonVoices = 5;
    noSpread.unisonDetuneCents = 16.0;
    noSpread.stereoSpread = 0.0;
    req.patch = noSpread;
    const arachno::RenderedAudio monoClip = playback.renderAuditionClip(req, 0.4);
    const std::array<double, 4> monoMetrics = peakAndBalance(monoClip);

    arachno::SynthPatch wideSpread = noSpread;
    wideSpread.stereoSpread = 1.0;
    req.patch = wideSpread;
    const arachno::RenderedAudio wideClip = playback.renderAuditionClip(req, 0.4);
    const std::array<double, 4> wideMetrics = peakAndBalance(wideClip);
    assert(wideMetrics[3] > monoMetrics[3] * 1.35);

    arachno::SynthPatch hot = base;
    hot.oscillatorBEnabled = true;
    hot.oscillatorCEnabled = true;
    hot.oscillatorDEnabled = true;
    hot.oscillatorB = arachno::Waveform::SuperSaw;
    hot.oscillatorC = arachno::Waveform::Square;
    hot.oscillatorD = arachno::Waveform::Saw;
    hot.oscBLevel = 1.0;
    hot.oscCLevel = 1.0;
    hot.oscDLevel = 1.0;
    hot.drive = 0.9;
    hot.gain = 1.0;
    hot.unisonVoices = 6;
    hot.unisonDetuneCents = 18.0;
    req.patch = hot;
    const arachno::RenderedAudio hotClip = playback.renderAuditionClip(req, 0.4);
    const std::array<double, 4> hotMetrics = peakAndBalance(hotClip);
    assert(hotMetrics[0] <= 0.9995);
}

void testSynthEnvelopeGateRelease() {
    arachno::RealtimePlaybackSession playback(48000);
    arachno::SynthPatch patch;
    patch.oscillatorAEnabled = true;
    patch.oscillatorBEnabled = false;
    patch.oscillatorCEnabled = false;
    patch.oscillatorDEnabled = false;
    patch.oscillatorA = arachno::Waveform::Sine;
    patch.fmEnabled = false;
    patch.ringEnabled = false;
    patch.hardSyncEnabled = false;
    patch.chorusEnabled = false;
    patch.bitCrushEnabled = false;
    patch.noiseEnabled = false;
    patch.subEnabled = false;
    patch.gain = 0.85;
    patch.ampEnvelope.attack = 0.40;
    patch.ampEnvelope.decay = 0.08;
    patch.ampEnvelope.sustain = 0.80;
    patch.ampEnvelope.release = 0.06;

    arachno::AuditionRequest request;
    request.note = arachno::Note(arachno::noteNameToMidi("C4"), 1.0f);
    request.patch = patch;
    request.gateSeconds = 0.02;
    const arachno::RenderedAudio clip = playback.renderAuditionClip(request, 0.45);
    assert(clip.sampleRate == 48000);

    const int earlyStart = static_cast<int>(clip.sampleRate * 0.02);
    const int earlyEnd = static_cast<int>(clip.sampleRate * 0.10);
    const int lateStart = static_cast<int>(clip.sampleRate * 0.24);
    const int lateEnd = static_cast<int>(clip.sampleRate * 0.34);

    double earlyPeak = 0.0;
    double latePeak = 0.0;
    for (int frame = earlyStart; frame < earlyEnd && frame < static_cast<int>(clip.frameCount()); ++frame) {
        const std::size_t idx = static_cast<std::size_t>(frame) * 2;
        earlyPeak = std::max(earlyPeak, std::max(
            std::abs(static_cast<double>(clip.interleavedStereo[idx])),
            std::abs(static_cast<double>(clip.interleavedStereo[idx + 1]))));
    }
    for (int frame = lateStart; frame < lateEnd && frame < static_cast<int>(clip.frameCount()); ++frame) {
        const std::size_t idx = static_cast<std::size_t>(frame) * 2;
        latePeak = std::max(latePeak, std::max(
            std::abs(static_cast<double>(clip.interleavedStereo[idx])),
            std::abs(static_cast<double>(clip.interleavedStereo[idx + 1]))));
    }
    assert(earlyPeak > 0.001);
    assert(latePeak < earlyPeak * 0.18);
}

void testSynthNoteOffSustain() {
    arachno::SynthPatch patch;
    patch.oscillatorAEnabled = true;
    patch.oscillatorBEnabled = false;
    patch.oscillatorCEnabled = false;
    patch.oscillatorDEnabled = false;
    patch.oscillatorA = arachno::Waveform::Sine;
    patch.fmEnabled = false;
    patch.ringEnabled = false;
    patch.hardSyncEnabled = false;
    patch.chorusEnabled = false;
    patch.bitCrushEnabled = false;
    patch.noiseEnabled = false;
    patch.subEnabled = false;
    patch.gain = 0.85;
    patch.ampEnvelope.attack = 0.01;
    patch.ampEnvelope.decay = 0.05;
    patch.ampEnvelope.sustain = 0.80;
    patch.ampEnvelope.release = 0.08;

    arachno::Synthesizer synth(48000.0);
    constexpr int blockFrames = 4800; // 0.1s blocks
    std::vector<float> left(blockFrames, 0.0f);
    std::vector<float> right(blockFrames, 0.0f);
    auto renderBlock = [&]() {
        // Synthesizer::render accumulates into the output buffers; callers must clear them.
        std::fill(left.begin(), left.end(), 0.0f);
        std::fill(right.begin(), right.end(), 0.0f);
        synth.render(left.data(), right.data(), blockFrames);
    };
    auto blockPeak = [&]() {
        double peak = 0.0;
        for (int i = 0; i < blockFrames; ++i) {
            peak = std::max(peak, std::max(
                std::abs(static_cast<double>(left[static_cast<std::size_t>(i)])),
                std::abs(static_cast<double>(right[static_cast<std::size_t>(i)]))));
        }
        return peak;
    };

    // A sustained note must keep sounding at its sustain level long past any normal gate.
    synth.noteOn(arachno::Note(60, 1.0f), patch, 0.0, 0.05, -1, true);
    double heldPeak = 0.0;
    for (int block = 0; block < 10; ++block) { // 1.0s total
        renderBlock();
        if (block >= 6) { // 0.6s..1.0s: far beyond the nominal 0.05s gate
            heldPeak = std::max(heldPeak, blockPeak());
        }
    }
    assert(heldPeak > 0.05);

    // noteOff ends the sustain and the release phase decays to silence.
    synth.noteOff(60);
    double releasePeak = 0.0;
    for (int block = 0; block < 10; ++block) { // 1.0s of release
        renderBlock();
        if (block >= 8) {
            releasePeak = std::max(releasePeak, blockPeak());
        }
    }
    assert(releasePeak < heldPeak * 0.02);

    // allNotesOff also ends sustained voices.
    synth.noteOn(arachno::Note(64, 1.0f), patch, 0.0, 0.05, -1, true);
    renderBlock();
    synth.allNotesOff(-1);
    double allOffPeak = 0.0;
    for (int block = 0; block < 10; ++block) {
        renderBlock();
        if (block >= 8) {
            allOffPeak = std::max(allOffPeak, blockPeak());
        }
    }
    assert(allOffPeak < heldPeak * 0.02);
}

void testSequencerNoteOff() {
    arachno::Song song = arachno::makeBlankSong();
    arachno::SynthPatch& patch = song.instruments[0].patch;
    patch.oscillatorA = arachno::Waveform::Sine;
    patch.oscillatorBEnabled = false;
    patch.oscillatorCEnabled = false;
    patch.oscillatorDEnabled = false;
    patch.noiseEnabled = false;
    patch.gain = 0.8;
    patch.ampEnvelope.attack = 0.005;
    patch.ampEnvelope.decay = 0.05;
    patch.ampEnvelope.sustain = 0.9;
    patch.ampEnvelope.release = 0.05;

    arachno::PatternStep& noteStep = song.patterns[0].step(0, 0);
    noteStep.note = arachno::Note(arachno::noteNameToMidi("C4"), 1.0f);
    noteStep.instrument = 0;
    noteStep.gate = 30.0; // would ring ~3.5s without a note-off step
    arachno::PatternStep& offStep = song.patterns[0].step(8, 0);
    offStep.noteOff = true;

    arachno::AudioEngine engine(song.sampleRate);
    auto peakBetween = [](const arachno::RenderedAudio& audio, double startSec, double endSec) {
        double peak = 0.0;
        const int start = static_cast<int>(startSec * audio.sampleRate);
        const int end = std::min(static_cast<int>(endSec * audio.sampleRate), static_cast<int>(audio.frameCount()));
        for (int frame = start; frame < end; ++frame) {
            const std::size_t idx = static_cast<std::size_t>(frame) * 2;
            peak = std::max(peak, std::max(
                std::abs(static_cast<double>(audio.interleavedStereo[idx])),
                std::abs(static_cast<double>(audio.interleavedStereo[idx + 1]))));
        }
        return peak;
    };

    const arachno::RenderedAudio audio = engine.renderSong(song);
    const double heldPeak = peakBetween(audio, 0.2, 0.6);
    // Note-off fires at row 8 (~0.94s at 128bpm/4rpb) with a 0.05s release.
    const double tailPeak = peakBetween(audio, 1.5, 3.0);
    assert(heldPeak > 0.05);
    assert(tailPeak < heldPeak * 0.05);

    // Sanity: without the note-off step the same note is still sounding in the tail window.
    offStep.noteOff = false;
    const arachno::RenderedAudio sustainedAudio = engine.renderSong(song);
    const double sustainedLatePeak = peakBetween(sustainedAudio, 1.5, 3.0);
    assert(sustainedLatePeak > heldPeak * 0.3);
}

void testNoteOffStepRoundTrip() {
    arachno::Song song = arachno::makeBlankSong();
    arachno::PatternStep& noteStep = song.patterns[0].step(0, 0);
    noteStep.note = arachno::Note(60, 0.9f);
    noteStep.instrument = 0;
    arachno::PatternStep& offStep = song.patterns[0].step(4, 0);
    offStep.noteOff = true;

    const std::filesystem::path path = std::filesystem::temp_directory_path() / "arachno-noteoff-roundtrip.arachno";
    arachno::saveProject(song, path.string());
    const arachno::Song loaded = arachno::loadProject(path.string());
    assert(loaded.patterns[0].step(4, 0).noteOff);
    assert(!loaded.patterns[0].step(4, 0).note.has_value());
    assert(!loaded.patterns[0].step(0, 0).noteOff);
    assert(loaded.patterns[0].step(0, 0).note.has_value());

    // Older files without the trailing note-off token must still load with noteOff = false.
    std::ifstream in(path);
    std::stringstream buffer;
    buffer << in.rdbuf();
    in.close();
    std::string text = buffer.str();
    const std::string marker = "step 4 0 ";
    const std::size_t lineStart = text.find(marker);
    assert(lineStart != std::string::npos);
    const std::size_t lineEnd = text.find('\n', lineStart);
    assert(lineEnd != std::string::npos);
    std::string legacyLine = text.substr(lineStart, lineEnd - lineStart);
    const std::size_t lastSpace = legacyLine.find_last_of(' ');
    assert(lastSpace != std::string::npos);
    legacyLine = legacyLine.substr(0, lastSpace); // strip the trailing note-off token
    text.replace(lineStart, lineEnd - lineStart, legacyLine);
    const std::filesystem::path legacyPath = std::filesystem::temp_directory_path() / "arachno-noteoff-legacy.arachno";
    {
        std::ofstream out(legacyPath);
        out << text;
    }
    const arachno::Song legacyLoaded = arachno::loadProject(legacyPath.string());
    assert(!legacyLoaded.patterns[0].step(4, 0).noteOff);
    assert(legacyLoaded.patterns[0].step(0, 0).note.has_value());

    // Editor command surface: noteoff writes a release step, entering a note clears it.
    arachno::PatternEditorSession editor(song);
    editor.applyCommand("move 4 0");
    editor.applyCommand("noteoff");
    assert(song.patterns[0].step(4, 0).noteOff);
    assert(!song.patterns[0].step(4, 0).note.has_value());
    editor.applyCommand("note C4 0.9");
    assert(!song.patterns[0].step(4, 0).noteOff);
    assert(song.patterns[0].step(4, 0).note.has_value());

    std::filesystem::remove(path);
    std::filesystem::remove(legacyPath);
}

void testLegatoInput() {
    arachno::Song song = arachno::makeBlankSong();
    arachno::PatternEditorSession editor(song);
    const int rowCount = song.patterns[0].rowCount();

    // Default: legato disarmed keeps the standard per-step gate.
    assert(!editor.legatoInputEnabled());
    editor.applyCommand("move 0 0");
    editor.applyCommand("note C4 0.9");
    assert(song.patterns[0].step(0, 0).note.has_value());
    assert(song.patterns[0].step(0, 0).gate == 0.88);

    // Legato armed: a note sustains to the pattern end when nothing follows.
    editor.applyCommand("legato on");
    assert(editor.legatoInputEnabled());
    editor.applyCommand("move 8 0");
    editor.applyCommand("note G4 0.9");
    assert(song.patterns[0].step(8, 0).gate == static_cast<double>(rowCount - 8));

    // A note entered before an existing note sustains exactly up to it.
    editor.applyCommand("move 4 0");
    editor.applyCommand("note E4 0.9");
    assert(song.patterns[0].step(4, 0).gate == 4.0);

    // A note-off step (===) also terminates the sustain.
    editor.applyCommand("move 12 0");
    editor.applyCommand("noteoff");
    editor.applyCommand("move 10 0");
    editor.applyCommand("note C5 0.9");
    assert(song.patterns[0].step(10, 0).gate == 2.0);

    // Steps on other tracks do not cut the sustain short.
    editor.applyCommand("move 6 1");
    editor.applyCommand("note C3 0.9");
    assert(song.patterns[0].step(6, 1).gate == static_cast<double>(rowCount - 6));
    assert(song.patterns[0].step(4, 0).gate == 4.0);

    // Bare "legato" toggles back off; entries fall back to the default gate.
    editor.applyCommand("legato");
    assert(!editor.legatoInputEnabled());
    editor.applyCommand("move 16 0");
    editor.applyCommand("note C4 0.9");
    assert(song.patterns[0].step(16, 0).gate == 0.88);

    // Toggling legato must not dirty the project undo history.
    arachno::PatternEditorSession undoProbe(song);
    assert(!undoProbe.canUndo());
    undoProbe.applyCommand("legato on");
    undoProbe.applyCommand("legato off");
    assert(!undoProbe.canUndo());
}

void testAudioProducerThreadContract() {
    // The shared audio-state mutex (RealtimePlaybackSession::apiMutex, exposed
    // as ApplicationSession::audioStateMutex) must serialize producer-thread
    // render blocks against transport, audition, and song mutations without
    // deadlock or state corruption — the contract the GUI audio producer
    // thread relies on.
    arachno::Song song = arachno::makeBlankSong();
    arachno::RealtimePlaybackSession playback(song.sampleRate);
    playback.setSong(&song);
    std::atomic<bool> stop {false};
    std::atomic<long> framesRendered {0};
    std::thread producer([&]() {
        std::vector<float> left(256, 0.0f);
        std::vector<float> right(256, 0.0f);
        while (!stop.load(std::memory_order_relaxed)) {
            playback.render(left.data(), right.data(), 256);
            framesRendered.fetch_add(256, std::memory_order_relaxed);
        }
    });
    arachno::PatternEditorSession editor(song);
    for (int i = 0; i < 200; ++i) {
        {
            // Mirrors ApplicationSession::applyEditorCommand's guard.
            const std::lock_guard<std::recursive_mutex> lock(playback.apiMutex());
            editor.applyCommand("note C4 0.9");
            editor.applyCommand("down");
        }
        playback.play();
        playback.previewInstrument(0, 60 + (i % 12), 0.8f, 0.1);
        if ((i % 5) == 0) {
            playback.pause();
        }
        if ((i % 7) == 0) {
            playback.play();
        }
        if ((i % 11) == 0) {
            playback.stop();
        }
    }
    playback.play();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    // Lock-free snapshot contract: audition with a STOPPED transport must be
    // visible as previewActive immediately (the GUI producer gates audition
    // rendering on it; a telemetry-stale false silences the patch screen).
    playback.stop();
    playback.previewInstrument(0, 60, 0.8f, 0.1);
    assert(playback.snapshot().previewActive);
    stop.store(true, std::memory_order_relaxed);
    producer.join();
    assert(framesRendered.load() > 0);
    playback.stop();
    const arachno::PlaybackSnapshot snap = playback.snapshot();
    assert(snap.state == arachno::TransportState::Stopped);
}

void testAudioRuntimeContract() {
    const std::vector<arachno::AudioDeviceInfo> defaults = arachno::defaultLinuxAudioDeviceCatalog();
    assert(!defaults.empty());
    assert(std::string(arachno::audioBackendName(arachno::AudioBackendType::PipeWire)) == "pipewire");
    assert(arachno::audioBackendFromName("pw") == arachno::AudioBackendType::PipeWire);
    assert(arachno::estimateAudioLatencyMs(48000, 256, 2) > 10.0);

    arachno::AudioRuntimeSettings settings = arachno::defaultAudioRuntimeSettings();
    settings.bufferFrames = 16;
    const arachno::AudioRuntimeValidation validation = arachno::validateAudioRuntimeSettings(settings);
    assert(validation.ok);
    assert(validation.normalized.backend == arachno::AudioBackendType::Dummy);
    assert(validation.normalized.deviceId == "dummy/offline");
    assert(validation.normalized.bufferFrames >= validation.device.minimumBufferFrames);
    assert(!validation.warnings.empty());

    settings.backend = arachno::AudioBackendType::Jack;
    settings.deviceId.clear();
    const arachno::AudioRuntimeValidation unavailableJack = arachno::validateAudioRuntimeSettings(settings);
    assert(!unavailableJack.ok);
    assert(unavailableJack.error.find("no available") != std::string::npos);

    std::vector<arachno::AudioDeviceInfo> devices = defaults;
    for (arachno::AudioDeviceInfo& device : devices) {
        if (device.backend == arachno::AudioBackendType::PipeWire) {
            device.available = true;
            device.status = "available";
        }
    }

    arachno::AudioRuntimeSession runtime(devices);
    arachno::AudioRuntimeSettings runtimeSettings;
    runtimeSettings.backend = arachno::AudioBackendType::PipeWire;
    runtimeSettings.bufferFrames = 128;
    runtimeSettings.periods = 3;
    const arachno::AudioRuntimeValidation runtimeValidation = runtime.configure(runtimeSettings);
    assert(runtimeValidation.ok);
    assert(runtime.health().configured);
    assert(runtime.health().backend == arachno::AudioBackendType::PipeWire);
    assert(runtime.health().estimatedLatencyMs == arachno::estimateAudioLatencyMs(48000, 128, 3));
    assert(runtime.start());
    assert(runtime.health().active);

    arachno::Song song = arachno::makeDemoSong();
    arachno::RealtimePlaybackSession playback(song.sampleRate);
    playback.setSong(&song);
    playback.play();

    std::vector<float> left(static_cast<std::size_t>(runtime.health().bufferFrames), 0.0f);
    std::vector<float> right(static_cast<std::size_t>(runtime.health().bufferFrames), 0.0f);
    const arachno::AudioRuntimeProcessResult processed =
        runtime.renderBlock(playback, left.data(), right.data(), runtime.health().bufferFrames);
    assert(processed.ok);
    assert(processed.frames == runtime.health().bufferFrames);
    assert(runtime.health().processedBlocks == 1);
    assert(runtime.health().processedFrames == runtime.health().bufferFrames);

    const arachno::AudioRuntimeProcessResult shortBlock =
        runtime.renderBlock(playback, left.data(), right.data(), runtime.health().bufferFrames / 2);
    assert(shortBlock.ok);
    assert(shortBlock.underrun);
    assert(runtime.health().underrunCount == 1);
    runtime.stop();
    assert(!runtime.health().active);
}

void testApplicationSessionState() {
    arachno::ApplicationSession app(arachno::makeDemoSong());
    const std::vector<arachno::AppEvent> startupEvents = app.eventsSince(0);
    assert(!startupEvents.empty());
    assert(startupEvents.back().type == arachno::AppEventType::SessionReady);
    const std::uint64_t startupSequence = app.lastEventSequence();
    assert(std::string(arachno::appEventTypeName(arachno::AppEventType::TaskStarted)) == "task-started");

    arachno::AppSessionSnapshot initial = app.snapshot(0, 8);
    assert(!initial.dirty);
    assert(!initial.hasProjectPath);
    assert(initial.playback.hasSong);
    assert(initial.audio.configured);
    assert(initial.audio.backend == arachno::AudioBackendType::Dummy);
    assert(initial.audio.sampleRate == 48000);
    assert(!initial.audio.active);
    assert(initial.playback.state == arachno::TransportState::Stopped);
    assert(initial.editor.status.title == app.song().title);
    assert(!initial.hasDiagnosticErrors);
    assert(initial.diagnosticErrorCount == 0);
    assert(initial.lastMessage.severity == arachno::AppMessageSeverity::Info);

    const arachno::EditorCommandResult edit = app.applyEditorCommand("move 2 1");
    assert(edit.ok);
    assert(!edit.projectChanged);
    assert(!app.dirty());

    const arachno::EditorCommandResult note = app.applyEditorCommand("note C5 0.7");
    assert(note.ok);
    assert(note.projectChanged);
    assert(app.dirty());
    assert(app.snapshot().editor.activeStep.hasNote);
    const std::vector<arachno::AppEvent> editEvents = app.eventsSince(startupSequence);
    assert(std::any_of(
        editEvents.begin(),
        editEvents.end(),
        [](const arachno::AppEvent& event) {
            return event.type == arachno::AppEventType::EditorChanged;
        }));
    assert(std::any_of(
        editEvents.begin(),
        editEvents.end(),
        [](const arachno::AppEvent& event) {
            return event.type == arachno::AppEventType::ProjectChanged && event.dirty;
        }));

    const std::filesystem::path path = std::filesystem::temp_directory_path() / "arachno-app-session.arachno";
    const arachno::AppOperationResult save = app.saveProjectFileAs(path.string());
    assert(save.ok);
    assert(!app.dirty());
    assert(app.projectPath() == path.string());
    assert(app.snapshot().hasProjectPath);

    const arachno::EditorCommandResult bad = app.applyEditorCommand("gate -1");
    assert(!bad.ok);
    assert(!app.dirty());
    assert(app.lastMessage().severity == arachno::AppMessageSeverity::Error);

    app.playback().play();
    assert(app.snapshot().playback.state == arachno::TransportState::Playing);
    const arachno::AuditionResult audition = app.auditionCursorStep();
    assert(audition.ok);
    assert(app.previewCursorStep());
    const std::vector<arachno::AppEvent> drainedEvents = app.drainEvents();
    assert(std::any_of(
        drainedEvents.begin(),
        drainedEvents.end(),
        [](const arachno::AppEvent& event) {
            return event.type == arachno::AppEventType::PlaybackChanged;
        }));
    assert(app.drainEvents().empty());

    const arachno::AppOperationResult audioStart = app.startAudioRuntime();
    assert(audioStart.ok);
    std::vector<float> left(64, 0.0f);
    std::vector<float> right(64, 0.0f);
    const arachno::AudioRuntimeProcessResult render = app.renderAudioRuntimeBlock(left.data(), right.data(), 64);
    assert(render.ok);
    assert(app.audioRuntimeHealth().active);
    assert(app.audioRuntimeHealth().processedBlocks >= 1);
    const arachno::AudioRuntimeProcessResult underrun = app.renderAudioRuntimeBlock(left.data(), right.data(), 32);
    assert(underrun.ok);
    assert(underrun.underrun);
    assert(app.audioRuntimeHealth().underrunCount >= 1);
    const arachno::AppOperationResult audioStop = app.stopAudioRuntime();
    assert(audioStop.ok);
    assert(!app.audioRuntimeHealth().active);

    arachno::ApplicationSession loaded;
    const arachno::AppOperationResult load = loaded.loadProjectFile(path.string());
    assert(load.ok);
    assert(!loaded.dirty());
    assert(loaded.projectPath() == path.string());
    assert(loaded.snapshot().editor.status.title == app.song().title);

    std::filesystem::remove(path);
}

#include "ui/gui/GuiFileBrowserOps.h"
#include "ui/gui/GuiPathDefaults.h"

void testPatchBrowserUxHelpers() {
    using arachno::InlinePromptKind;
    assert(arachno::inlinePromptKindIsPatch(InlinePromptKind::ImportPatchAsNewPath));
    assert(arachno::inlinePromptKindIsPatch(InlinePromptKind::ImportPatchReplacePath));
    assert(arachno::inlinePromptKindIsPatch(InlinePromptKind::ImportPatchReplaceAllPath));
    assert(arachno::inlinePromptKindIsPatch(InlinePromptKind::ExportPatchPath));
    assert(!arachno::inlinePromptKindIsPatch(InlinePromptKind::OpenProjectPath));
    assert(arachno::inlinePromptKindPreviewsPatchFile(InlinePromptKind::ImportPatchAsNewPath));
    assert(arachno::inlinePromptKindPreviewsPatchFile(InlinePromptKind::ImportPatchReplacePath));
    assert(!arachno::inlinePromptKindPreviewsPatchFile(InlinePromptKind::ImportPatchReplaceAllPath));
    assert(!arachno::inlinePromptKindPreviewsPatchFile(InlinePromptKind::ExportPatchPath));

    // Double-click detection: second click on the same entry confirms, streak is consumed.
    const bool first = arachno::fileBrowserRegisterClickForDoubleClick(7);
    const bool second = arachno::fileBrowserRegisterClickForDoubleClick(7);
    const bool third = arachno::fileBrowserRegisterClickForDoubleClick(7);
    assert(!first);
    assert(second);
    assert(!third);
    // A different index breaks the streak; clicking the new index again is a double.
    const bool otherIndex = arachno::fileBrowserRegisterClickForDoubleClick(3);
    assert(!otherIndex);
    const bool otherIndexAgain = arachno::fileBrowserRegisterClickForDoubleClick(3);
    assert(otherIndexAgain);
    const bool afterConsumed = arachno::fileBrowserRegisterClickForDoubleClick(3);
    assert(!afterConsumed);

    const std::filesystem::path settingsPath = arachno::defaultSettingsPath();
    assert(!settingsPath.empty());
    assert(settingsPath.filename() == "settings.txt");
}

void testAppSettingsPersistence() {
    arachno::AppSettings settings;
    settings.exportDefaults.defaultDirectory = "/tmp/arachno-export";
    settings.exportDefaults.audioFormat = "ogg";
    settings.exportDefaults.sampleRate = 44100;
    settings.exportDefaults.renderStems = true;
    settings.layout.gridVisibleRows = 48;
    settings.layout.showBrowser = false;
    settings.layout.followPlayback = false;
    settings.audioRuntime.backend = "dummy";
    settings.audioRuntime.deviceId = "dummy/offline";
    settings.audioRuntime.sampleRate = 44100;
    settings.audioRuntime.bufferFrames = 128;
    settings.audioRuntime.periods = 3;
    settings.audioRuntime.realtimePriority = false;
    settings.audioRuntime.connectSystemOutputs = false;
    settings.shortcutOverrides.push_back({"Ctrl+Alt+Z", "history.undo"});
    settings.syncCheckpoints.push_back({"main-window", 42, 7, "/tmp/first.arachno", "fingerprint-v1"});
    settings.browser.lastPatchDirectory = "/tmp/arachno-patches/bass";
    assert(arachno::findSyncCheckpoint(settings, "main-window") != nullptr);
    assert(arachno::upsertSyncCheckpoint(settings, {"secondary", 91, 12}));
    assert(arachno::findSyncCheckpoint(settings, "secondary") != nullptr);
    assert(arachno::removeSyncCheckpoint(settings, "secondary"));
    assert(arachno::findSyncCheckpoint(settings, "secondary") == nullptr);

    arachno::addRecentProject(settings, "/tmp/first.arachno");
    arachno::addRecentProject(settings, "/tmp/second.arachno");
    arachno::addRecentProject(settings, "/tmp/first.arachno");
    assert(settings.recentProjects.size() == 2);
    assert(settings.recentProjects.front() == "/tmp/first.arachno");

    const std::vector<arachno::ShortcutBinding> shortcuts = arachno::effectiveEditorShortcuts(settings);
    const arachno::EditorAction* undo = arachno::findEditorActionForShortcut(shortcuts, "ctrl+alt+z");
    assert(undo != nullptr);
    assert(undo->id == "history.undo");

    const std::filesystem::path path = std::filesystem::temp_directory_path() / "arachno-settings.conf";
    arachno::saveAppSettings(settings, path.string());
    const arachno::AppSettings loaded = arachno::loadAppSettings(path.string());

    assert(loaded.exportDefaults.defaultDirectory == settings.exportDefaults.defaultDirectory);
    assert(loaded.exportDefaults.audioFormat == "ogg");
    assert(loaded.exportDefaults.sampleRate == 44100);
    assert(loaded.exportDefaults.renderStems);
    assert(loaded.layout.gridVisibleRows == 48);
    assert(!loaded.layout.showBrowser);
    assert(!loaded.layout.followPlayback);
    assert(loaded.audioRuntime.backend == "dummy");
    assert(loaded.audioRuntime.deviceId == "dummy/offline");
    assert(loaded.audioRuntime.sampleRate == 44100);
    assert(loaded.audioRuntime.bufferFrames == 128);
    assert(loaded.audioRuntime.periods == 3);
    assert(!loaded.audioRuntime.realtimePriority);
    assert(!loaded.audioRuntime.connectSystemOutputs);
    assert(loaded.recentProjects == settings.recentProjects);
    assert(loaded.shortcutOverrides.size() == 1);
    assert(loaded.shortcutOverrides.front().shortcut == "Ctrl+Alt+Z");
    assert(loaded.syncCheckpoints.size() == 1);
    assert(loaded.syncCheckpoints.front().name == "main-window");
    assert(loaded.syncCheckpoints.front().eventSequence == 42);
    assert(loaded.syncCheckpoints.front().taskId == 7);
    assert(loaded.syncCheckpoints.front().projectPath == "/tmp/first.arachno");
    assert(loaded.syncCheckpoints.front().projectFingerprint == "fingerprint-v1");
    assert(loaded.browser.lastPatchDirectory == "/tmp/arachno-patches/bass");

    const arachno::AudioRuntimeSettings runtimeSettings =
        arachno::audioRuntimeSettingsFromPreferences(loaded.audioRuntime);
    assert(runtimeSettings.backend == arachno::AudioBackendType::Dummy);
    assert(runtimeSettings.deviceId == "dummy/offline");
    assert(runtimeSettings.sampleRate == 44100);
    assert(runtimeSettings.bufferFrames == 128);
    assert(runtimeSettings.periods == 3);
    assert(!runtimeSettings.realtimePriority);
    assert(!runtimeSettings.connectSystemOutputs);

    arachno::ApplicationSession app(arachno::makeDemoSong(), 48000, loaded);
    assert(!app.snapshot().playback.followCursor);

    const arachno::AppOperationResult saveSettings = app.saveSettingsFile(path.string());
    assert(saveSettings.ok);
    const arachno::AppOperationResult loadSettings = app.loadSettingsFile(path.string());
    assert(loadSettings.ok);
    assert(app.settings().exportDefaults.audioFormat == "ogg");

    std::filesystem::remove(path);
}

void testScriptIntegrationSurface() {
    const arachno::ScriptCommand projectCommand = arachno::buildPythonNewProjectCommand(
        "/tmp/community starter.arachno",
        "Community Starter",
        132.0);
    const std::string renderedCommand = arachno::formatScriptCommand(projectCommand);
    assert(renderedCommand.find("PYTHONPATH=python") != std::string::npos);
    assert(renderedCommand.find("new-ebm") != std::string::npos);
    assert(renderedCommand.find("'Community Starter'") != std::string::npos);

    const arachno::ScriptCommand patchCommand = arachno::buildPythonPatchPluginCommand(
        "plugin.py",
        "bass.arachnopatch",
        "Factory Bass");
    assert(arachno::formatScriptCommand(patchCommand).find("patch-plugin") != std::string::npos);

    arachno::ApplicationSession app(arachno::makeDemoSong());
    const int initialInstruments = static_cast<int>(app.song().instruments.size());
    const std::filesystem::path temp = std::filesystem::temp_directory_path();
    const std::filesystem::path patchPath = temp / "script-bass.arachnopatch";
    const std::filesystem::path projectPath = temp / "script-project.arachno";
    const std::filesystem::path commandsPath = temp / "script-commands.arachno-edit";

    arachno::SynthPatch patch;
    patch.name = "Script Bass";
    patch.drive = 0.5;
    arachno::savePatch(patch, patchPath.string());
    const arachno::ScriptArtifactInfo patchInfo = arachno::inspectScriptArtifact(patchPath.string());
    assert(patchInfo.type == arachno::ScriptArtifactType::Patch);
    assert(patchInfo.exists);
    assert(patchInfo.loadable);

    const arachno::ScriptImportResult patchImport = app.importScriptArtifact(patchPath.string(), "Imported Bass");
    assert(patchImport.ok);
    assert(patchImport.importedInstrument == initialInstruments);
    assert(app.song().instruments.back().patch.name == "Imported Bass");
    assert(app.dirty());

    arachno::saveProject(arachno::makeDemoSong(), projectPath.string());
    const arachno::ScriptImportResult projectImport = app.importScriptArtifact(projectPath.string());
    assert(projectImport.ok);
    assert(!app.dirty());
    assert(app.projectPath() == projectPath.string());

    {
        std::ofstream out(commandsPath);
        out << "# generated edit script\n";
        out << "move 2 1\n";
        out << "note C5 0.7\n";
    }
    assert(arachno::scriptArtifactTypeFromPath(commandsPath.string()) == arachno::ScriptArtifactType::CommandFile);
    const std::vector<std::string> commands = arachno::loadScriptCommandFile(commandsPath.string());
    assert(commands.size() == 2);
    const arachno::ScriptImportResult commandImport = app.importScriptArtifact(commandsPath.string());
    assert(commandImport.ok);
    assert(commandImport.appliedCommandCount == 2);
    assert(app.dirty());
    assert(app.song().patterns.front().step(2, 1).note.has_value());

    std::filesystem::remove(patchPath);
    std::filesystem::remove(projectPath);
    std::filesystem::remove(commandsPath);
}

void testFileCompatibilityInspection() {
    const std::filesystem::path temp = std::filesystem::temp_directory_path();
    const std::filesystem::path legacyProject = temp / "arachno-legacy-minimal.arachno";
    const std::filesystem::path legacyPatch = temp / "arachno-legacy-minimal.arachnopatch";
    const std::filesystem::path futureProject = temp / "arachno-future.arachno";
    const std::filesystem::path commandFile = temp / "arachno-commands.arachno-edit";

    {
        std::ofstream out(legacyProject);
        out << "arachno_project 1\n";
        out << "title \"Legacy\"\n";
        out << "bpm 120\n";
        out << "rows_per_beat 4\n";
        out << "sample_rate 48000\n";
        out << "tracks 1\n";
        out << "track \"Mono\" 0.85 0 0 0\n";
        out << "instruments 1\n";
        out << "instrument 0 \"Init\" saw square"
            << " 0.35 7 0.18 0.02 0.72 0.12 0.18 5.5 0 0 0.08 0.55 0"
            << " 0.005 0.08 0.72 0.18 0.005 0.08 0.72 0.18\n";
        out << "patterns 1\n";
        out << "pattern \"Main\" 4 1\n";
        out << "step 0 0 1 60 0.8 0 0.88 0 0\n";
        out << "end_pattern\n";
        out << "order 1 0\n";
        out << "end_project\n";
    }

    const arachno::FileCompatibilityReport projectReport = arachno::inspectTrackerFile(legacyProject.string());
    assert(projectReport.kind == arachno::TrackerFileKind::Project);
    assert(projectReport.exists);
    assert(projectReport.readable);
    assert(projectReport.compatible);
    assert(projectReport.loadable);
    assert(projectReport.version == 1);

    {
        std::ofstream out(legacyPatch);
        out << "arachno_patch 1\n";
        out << "name \"Legacy Patch\"\n";
        out << "oscillators saw square\n";
        out << "params 0.35 7 0.18 0.02 0.72 0.12 0.18 5.5 0 0 0.08 0.55 0\n";
        out << "amp 0.005 0.08 0.72 0.18\n";
        out << "filter 0.005 0.08 0.72 0.18\n";
        out << "end_patch\n";
    }

    const arachno::FileCompatibilityReport patchReport = arachno::inspectPatchFile(legacyPatch.string());
    assert(patchReport.kind == arachno::TrackerFileKind::Patch);
    assert(patchReport.compatible);
    assert(patchReport.loadable);
    assert(patchReport.version == 1);

    {
        std::ofstream out(futureProject);
        out << "arachno_project 99\n";
    }
    const arachno::FileCompatibilityReport futureReport = arachno::inspectProjectFile(futureProject.string());
    assert(futureReport.exists);
    assert(futureReport.version == 99);
    assert(!futureReport.compatible);
    assert(!futureReport.loadable);
    assert(futureReport.error.find("newer") != std::string::npos);

    {
        std::ofstream out(commandFile);
        out << "# script output\n";
        out << "move 0 0\n";
    }
    const arachno::FileCompatibilityReport commandReport = arachno::inspectTrackerFile(commandFile.string());
    assert(commandReport.kind == arachno::TrackerFileKind::CommandFile);
    assert(commandReport.compatible);
    assert(commandReport.loadable);

    const std::string formatted = arachno::formatFileCompatibilityReport(futureReport);
    assert(formatted.find("File compatibility") != std::string::npos);
    assert(formatted.find("version: 99") != std::string::npos);

    std::filesystem::remove(legacyProject);
    std::filesystem::remove(legacyPatch);
    std::filesystem::remove(futureProject);
    std::filesystem::remove(commandFile);
}

void testExportWorkflow() {
    const arachno::Song song = arachno::makeDemoSong();
    const std::filesystem::path temp = std::filesystem::temp_directory_path();
    const std::filesystem::path mixdown = temp / "arachno-workflow-mix.wav";
    const std::filesystem::path stems = temp / "arachno-workflow-stems";
    const std::filesystem::path midi = temp / "arachno-workflow.mid";

    arachno::ApplicationSession app(song);
    const arachno::ExportPreflight mixPreflight = arachno::preflightExport(
        song,
        arachno::mixdownExportRequest(mixdown.string()));
    assert(mixPreflight.ok);
    assert(mixPreflight.totalWork == 1);
    assert(mixPreflight.expectedFiles.size() == 1);
    assert(mixPreflight.expectedFiles.front() == mixdown.string());

    const arachno::ExportPreflight stemPreflight = arachno::preflightExport(
        song,
        arachno::stemExportRequest(stems.string()));
    assert(stemPreflight.ok);
    assert(stemPreflight.totalWork == static_cast<int>(song.tracks.size()));
    assert(stemPreflight.expectedFiles.size() == song.tracks.size());
    assert(stemPreflight.expectedFiles.front().find("1_bass.wav") != std::string::npos);

    arachno::ExportRequest badMidi = arachno::midiExportRequest(midi.string(), 0);
    const arachno::ExportPreflight badMidiPreflight = arachno::preflightExport(song, badMidi);
    assert(!badMidiPreflight.ok);
    assert(badMidiPreflight.error.find("positive") != std::string::npos);

    arachno::Song noTracks = song;
    noTracks.tracks.clear();
    const arachno::ExportPreflight noStemPreflight = arachno::preflightExport(
        noTracks,
        arachno::stemExportRequest(stems.string()));
    assert(!noStemPreflight.ok);
    assert(noStemPreflight.error.find("no tracks") != std::string::npos);

    const arachno::ExportResult mixResult = app.exportProject(arachno::mixdownExportRequest(mixdown.string()));
    assert(mixResult.ok);
    assert(mixResult.target == arachno::ExportTarget::Mixdown);
    assert(mixResult.files.size() == 1);
    assert(mixResult.files.front().sampleRate == song.sampleRate);
    assert(mixResult.files.front().frameCount > 0);
    assert(std::filesystem::file_size(mixdown) > 44);

    std::vector<arachno::ExportProgress> progressEvents;
    const arachno::ExportResult stemResult = arachno::runExportWorkflow(
        song,
        arachno::stemExportRequest(stems.string()),
        [&progressEvents](const arachno::ExportProgress& progress) {
            progressEvents.push_back(progress);
        });
    assert(stemResult.ok);
    assert(stemResult.target == arachno::ExportTarget::Stems);
    assert(stemResult.files.size() == song.tracks.size());
    assert(progressEvents.size() == song.tracks.size() + 1);
    assert(progressEvents.front().current == 0);
    assert(progressEvents.front().total == static_cast<int>(song.tracks.size()));
    assert(progressEvents.back().current == static_cast<int>(song.tracks.size()));
    assert(progressEvents.back().target == arachno::ExportTarget::Stems);
    assert(!progressEvents.back().path.empty());
    for (const arachno::ExportedFile& file : stemResult.files) {
        assert(std::filesystem::exists(file.path));
        assert(file.sampleRate == song.sampleRate);
        assert(file.frameCount > 0);
    }

    const arachno::ExportResult midiResult = app.exportProject(arachno::midiExportRequest(midi.string()));
    assert(midiResult.ok);
    assert(midiResult.target == arachno::ExportTarget::Midi);
    assert(midiResult.files.size() == 1);
    assert(std::filesystem::file_size(midi) > 32);

    const arachno::ExportResult badResult = app.exportProject(arachno::mixdownExportRequest(""));
    assert(!badResult.ok);
    assert(!badResult.error.empty());
    const arachno::ExportPreflight badPreflight = arachno::preflightExport(song, arachno::mixdownExportRequest(""));
    assert(!badPreflight.ok);
    assert(badPreflight.error == badResult.error);

    assert(arachno::exportTargetName(arachno::ExportTarget::Stems) == std::string("stems"));
    assert(arachno::exportFormatName(arachno::ExportFormat::Ogg) == std::string("ogg"));
    assert(arachno::exportFormatExtension(arachno::ExportFormat::Mp3) == ".mp3");

    std::filesystem::remove(mixdown);
    std::filesystem::remove(midi);
    std::filesystem::remove_all(stems);
}

void testAutoSaveRecovery() {
    arachno::ApplicationSession app(arachno::makeDemoSong());
    app.applyEditorCommand("title Recovery Test");
    app.applyEditorCommand("move 2 1");
    app.applyEditorCommand("note C5 0.7");
    assert(app.dirty());

    const std::filesystem::path recoveryDirectory = std::filesystem::temp_directory_path() / "arachno-recovery";
    const std::string recoveryPath = arachno::recoveryPathForProject(
        "/tmp/My Project.arachno",
        recoveryDirectory.string());
    assert(arachno::defaultRecoveryFileName("/tmp/My Project.arachno") == "My_Project.autosave.arachno");

    const arachno::RecoveryResult saved = app.saveRecoverySnapshot(recoveryPath);
    assert(saved.ok);
    assert(std::filesystem::exists(recoveryPath));

    const arachno::RecoveryInfo info = arachno::inspectRecoveryFile(recoveryPath);
    assert(info.exists);
    assert(info.loadable);
    assert(info.error.empty());

    arachno::ApplicationSession restored;
    const arachno::RecoveryResult restore = restored.restoreRecoverySnapshot(recoveryPath);
    assert(restore.ok);
    assert(restored.dirty());
    assert(restored.song().title == "Recovery Test");
    assert(restored.song().patterns.front().step(2, 1).note.has_value());

    const arachno::RecoveryResult cleared = restored.clearRecoverySnapshot(recoveryPath);
    assert(cleared.ok);
    assert(!std::filesystem::exists(recoveryPath));

    std::filesystem::remove_all(recoveryDirectory);
}

void testProjectLifecyclePlanning() {
    const std::filesystem::path temp = std::filesystem::temp_directory_path();
    const std::filesystem::path openPath = temp / "arachno-lifecycle-open.arachno";
    const std::filesystem::path currentPath = temp / "arachno-lifecycle-current.arachno";
    const std::filesystem::path futurePath = temp / "arachno-lifecycle-future.arachno";
    const std::filesystem::path recoveryDirectory = temp / "arachno-lifecycle-recovery";

    arachno::saveProject(arachno::makeDemoSong(), openPath.string());
    arachno::saveProject(arachno::makeDemoSong(), currentPath.string());
    const std::string recoveryPath = arachno::recoveryPathForProject(openPath.string(), recoveryDirectory.string());
    const arachno::RecoveryResult recovery = arachno::saveRecoveryFile(arachno::makeDemoSong(), recoveryPath);
    assert(recovery.ok);

    const arachno::ProjectOpenPreflight preflight =
        arachno::preflightOpenProject(openPath.string(), recoveryDirectory.string());
    assert(preflight.canOpen);
    assert(preflight.shouldOfferRecovery);
    assert(preflight.recovery.loadable);

    arachno::ApplicationSession clean(arachno::makeDemoSong());
    const arachno::UnsavedChangesPrompt cleanPrompt =
        arachno::buildUnsavedChangesPrompt(clean, arachno::ProjectLifecycleAction::OpenProject);
    assert(!cleanPrompt.required);

    arachno::ApplicationSession unsaved(arachno::makeDemoSong());
    unsaved.applyEditorCommand("title Unsaved Lifecycle");
    assert(unsaved.dirty());

    const arachno::UnsavedChangesPrompt prompt =
        arachno::buildUnsavedChangesPrompt(unsaved, arachno::ProjectLifecycleAction::OpenProject);
    assert(prompt.required);
    assert(prompt.message.find("Untitled project") != std::string::npos);
    assert(prompt.detail.find("open another project") != std::string::npos);

    const arachno::ProjectLifecyclePlan waiting = arachno::planProjectLifecycleTransition(
        unsaved,
        arachno::ProjectLifecycleAction::OpenProject,
        arachno::UnsavedChangesChoice::NotNeeded,
        openPath.string(),
        recoveryDirectory.string());
    assert(!waiting.canProceed);
    assert(waiting.requiresUnsavedDecision);
    assert(waiting.message == "Waiting for unsaved-changes decision");

    const arachno::ProjectLifecyclePlan saveAsNeeded = arachno::planProjectLifecycleTransition(
        unsaved,
        arachno::ProjectLifecycleAction::OpenProject,
        arachno::UnsavedChangesChoice::Save,
        openPath.string(),
        recoveryDirectory.string());
    assert(!saveAsNeeded.canProceed);
    assert(saveAsNeeded.shouldSaveBeforeProceeding);
    assert(saveAsNeeded.requiresSaveAs);

    const arachno::ProjectLifecyclePlan discardClose = arachno::planProjectLifecycleTransition(
        unsaved,
        arachno::ProjectLifecycleAction::CloseProject,
        arachno::UnsavedChangesChoice::Discard);
    assert(discardClose.canProceed);
    assert(discardClose.shouldDiscardChanges);

    const arachno::ProjectLifecyclePlan cancelQuit = arachno::planProjectLifecycleTransition(
        unsaved,
        arachno::ProjectLifecycleAction::QuitApplication,
        arachno::UnsavedChangesChoice::Cancel);
    assert(!cancelQuit.canProceed);
    assert(cancelQuit.message == "Canceled by user");

    arachno::ApplicationSession saved(arachno::makeDemoSong());
    assert(saved.saveProjectFileAs(currentPath.string()).ok);
    saved.applyEditorCommand("title Saved But Dirty");
    const arachno::ProjectLifecyclePlan saveThenOpen = arachno::planProjectLifecycleTransition(
        saved,
        arachno::ProjectLifecycleAction::OpenProject,
        arachno::UnsavedChangesChoice::Save,
        openPath.string(),
        recoveryDirectory.string());
    assert(saveThenOpen.canProceed);
    assert(saveThenOpen.shouldSaveBeforeProceeding);
    assert(!saveThenOpen.requiresSaveAs);
    assert(saveThenOpen.shouldOfferRecovery);

    {
        std::ofstream out(futurePath);
        out << "arachno_project 99\n";
    }
    const arachno::ProjectLifecyclePlan futureOpen = arachno::planProjectLifecycleTransition(
        clean,
        arachno::ProjectLifecycleAction::OpenProject,
        arachno::UnsavedChangesChoice::NotNeeded,
        futurePath.string());
    assert(!futureOpen.canProceed);
    assert(!futureOpen.openPreflight.canOpen);
    assert(futureOpen.error.find("newer") != std::string::npos);

    assert(std::string(arachno::projectLifecycleActionName(arachno::ProjectLifecycleAction::RestoreRecovery))
        == "restore-recovery");
    assert(std::string(arachno::unsavedChangesChoiceName(arachno::UnsavedChangesChoice::Discard)) == "discard");

    std::filesystem::remove(openPath);
    std::filesystem::remove(currentPath);
    std::filesystem::remove(futurePath);
    std::filesystem::remove_all(recoveryDirectory);
}

void testApplicationActionBridge() {
    arachno::ApplicationSession app(arachno::makeDemoSong());

    const std::vector<arachno::AppActionEntry> projectEntries =
        arachno::buildApplicationActionPalette(app, "project");
    assert(std::any_of(
        projectEntries.begin(),
        projectEntries.end(),
        [](const arachno::AppActionEntry& entry) {
            return entry.id == "project.open" && entry.requiresPath;
        }));
    assert(std::any_of(
        projectEntries.begin(),
        projectEntries.end(),
        [](const arachno::AppActionEntry& entry) {
            return entry.id == "project.save" && !entry.enabled;
        }));

    const std::vector<arachno::AppActionEntry> noteEntries =
        arachno::buildApplicationActionPalette(app, "note");
    assert(std::any_of(
        noteEntries.begin(),
        noteEntries.end(),
        [](const arachno::AppActionEntry& entry) {
            return entry.id == "editor.step.note" && entry.requiresCommandText && entry.parameterCount == 3;
        }));

    const arachno::AppActionSchema noteSchema = arachno::buildAppActionSchema("editor.step.note");
    assert(noteSchema.kind == arachno::AppActionKind::Editor);
    assert(noteSchema.parameters.size() == 3);
    assert(noteSchema.parameters[0].name == "note");
    assert(noteSchema.parameters[0].type == arachno::AppActionParameterType::NoteName);
    assert(noteSchema.parameters[1].name == "velocity");
    assert(!noteSchema.parameters[1].required);
    assert(noteSchema.parameters[1].hasMinimum);
    assert(noteSchema.parameters[1].hasMaximum);
    assert(noteSchema.parameters[2].name == "index");
    assert(!noteSchema.parameters[2].required);

    const arachno::AppActionCommandBuild builtNote = arachno::buildCommandForAppAction(
        "editor.step.note",
        {{"note", "C5"}, {"velocity", "0.8"}});
    assert(builtNote.ok);
    assert(builtNote.commandText == "note C5 0.8");

    const arachno::AppActionCommandBuild missingNote = arachno::buildCommandForAppAction(
        "editor.step.note",
        {{"velocity", "0.8"}});
    assert(!missingNote.ok);
    assert(missingNote.missingParameters.size() == 1);
    assert(missingNote.missingParameters.front() == "note");

    const arachno::AppActionValidationResult invalidNote = arachno::validateAppActionParameters(
        "editor.step.note",
        {{"note", "H2"}, {"velocity", "0.8"}});
    assert(!invalidNote.ok);
    assert(invalidNote.fieldErrors.size() == 1);
    assert(invalidNote.fieldErrors.front().name == "note");

    const arachno::AppActionValidationResult invalidVelocity = arachno::validateAppActionParameters(
        "editor.step.note",
        {{"note", "C5"}, {"velocity", "1.8"}});
    assert(!invalidVelocity.ok);
    assert(invalidVelocity.fieldErrors.front().name == "velocity");

    const arachno::AppActionValidationResult validWithoutOptionalVelocity = arachno::validateAppActionParameters(
        "editor.step.note",
        {{"note", "C5"}});
    assert(validWithoutOptionalVelocity.ok);

    const arachno::AppActionSchema openSchema = arachno::buildAppActionSchema("project.open");
    assert(openSchema.requiresPath);
    assert(openSchema.parameters.size() == 1);
    assert(openSchema.parameters.front().type == arachno::AppActionParameterType::FilePath);

    const arachno::AppActionSchema audioSchema = arachno::buildAppActionSchema("audio.runtime.configure");
    assert(audioSchema.kind == arachno::AppActionKind::Application);
    assert(audioSchema.parameters.size() == 7);
    assert(audioSchema.parameters[0].name == "backend");
    assert(audioSchema.parameters[0].type == arachno::AppActionParameterType::Choice);
    assert(audioSchema.parameters[5].name == "realtime_priority");
    assert(audioSchema.parameters[5].type == arachno::AppActionParameterType::Boolean);
    const arachno::AppActionSchema audioStatusSchema = arachno::buildAppActionSchema("audio.runtime.status");
    assert(audioStatusSchema.parameters.empty());
    const arachno::AppActionSchema audioDevicesSchema = arachno::buildAppActionSchema("audio.runtime.devices");
    assert(audioDevicesSchema.parameters.size() == 2);
    assert(audioDevicesSchema.parameters.front().name == "backend");
    assert(audioDevicesSchema.parameters.front().type == arachno::AppActionParameterType::Choice);
    assert(audioDevicesSchema.parameters[1].name == "only_available");
    assert(audioDevicesSchema.parameters[1].type == arachno::AppActionParameterType::Boolean);
    const arachno::AppActionSchema audioRenderSchema =
        arachno::buildAppActionSchema("audio.runtime.render_test");
    assert(audioRenderSchema.parameters.size() == 2);
    assert(audioRenderSchema.parameters.front().name == "frame_count");
    assert(audioRenderSchema.parameters.front().type == arachno::AppActionParameterType::Integer);
    const arachno::AppActionSchema audioUnderrunSchema =
        arachno::buildAppActionSchema("audio.runtime.simulate_underrun");
    assert(audioUnderrunSchema.parameters.size() == 1);
    assert(audioUnderrunSchema.parameters.front().name == "detail");
    const arachno::AppActionSchema taskCancelSchema = arachno::buildAppActionSchema("task.cancel");
    assert(taskCancelSchema.parameters.size() == 2);
    assert(taskCancelSchema.parameters.front().name == "task_id");
    assert(taskCancelSchema.parameters.front().type == arachno::AppActionParameterType::Integer);
    assert(taskCancelSchema.parameters.front().required);
    assert(taskCancelSchema.parameters[1].name == "message");
    assert(taskCancelSchema.parameters[1].type == arachno::AppActionParameterType::Text);
    assert(!taskCancelSchema.parameters[1].required);
    const arachno::AppActionSchema taskClearSchema = arachno::buildAppActionSchema("task.clear_finished");
    assert(taskClearSchema.parameters.empty());
    const arachno::AppActionSchema sessionEventsSchema = arachno::buildAppActionSchema("session.events");
    assert(sessionEventsSchema.parameters.size() == 6);
    assert(sessionEventsSchema.parameters[0].name == "since");
    assert(sessionEventsSchema.parameters[0].type == arachno::AppActionParameterType::Integer);
    assert(sessionEventsSchema.parameters[1].name == "drain");
    assert(sessionEventsSchema.parameters[1].type == arachno::AppActionParameterType::Boolean);
    assert(sessionEventsSchema.parameters[2].name == "max_events");
    assert(sessionEventsSchema.parameters[2].type == arachno::AppActionParameterType::Integer);
    assert(sessionEventsSchema.parameters[3].name == "include_snapshot");
    assert(sessionEventsSchema.parameters[3].type == arachno::AppActionParameterType::Boolean);
    assert(sessionEventsSchema.parameters[4].name == "snapshot_grid_start_row");
    assert(sessionEventsSchema.parameters[4].type == arachno::AppActionParameterType::Integer);
    assert(sessionEventsSchema.parameters[5].name == "snapshot_grid_row_count");
    assert(sessionEventsSchema.parameters[5].type == arachno::AppActionParameterType::Integer);
    const arachno::AppActionSchema sessionSnapshotSchema = arachno::buildAppActionSchema("session.snapshot");
    assert(sessionSnapshotSchema.parameters.size() == 2);
    assert(sessionSnapshotSchema.parameters[0].name == "grid_start_row");
    assert(sessionSnapshotSchema.parameters[0].type == arachno::AppActionParameterType::Integer);
    assert(sessionSnapshotSchema.parameters[1].name == "grid_row_count");
    assert(sessionSnapshotSchema.parameters[1].type == arachno::AppActionParameterType::Integer);
    const arachno::AppActionSchema checkpointSaveSchema = arachno::buildAppActionSchema("session.checkpoint.save");
    assert(checkpointSaveSchema.parameters.size() == 4);
    assert(checkpointSaveSchema.parameters[0].name == "name");
    assert(checkpointSaveSchema.parameters[1].name == "event_sequence");
    assert(checkpointSaveSchema.parameters[2].name == "task_id");
    assert(checkpointSaveSchema.parameters[3].name == "max_checkpoints");
    const arachno::AppActionSchema checkpointAdvanceSchema = arachno::buildAppActionSchema("session.checkpoint.advance");
    assert(checkpointAdvanceSchema.parameters.size() == 6);
    assert(checkpointAdvanceSchema.parameters[0].name == "name");
    assert(checkpointAdvanceSchema.parameters[1].name == "event_sequence");
    assert(checkpointAdvanceSchema.parameters[2].name == "task_id");
    assert(checkpointAdvanceSchema.parameters[3].name == "expected_event_sequence");
    assert(checkpointAdvanceSchema.parameters[4].name == "expected_task_id");
    assert(checkpointAdvanceSchema.parameters[5].name == "max_checkpoints");
    const arachno::AppActionSchema checkpointLoadSchema = arachno::buildAppActionSchema("session.checkpoint.load");
    assert(checkpointLoadSchema.parameters.size() == 1);
    assert(checkpointLoadSchema.parameters.front().name == "name");
    const arachno::AppActionSchema checkpointClearSchema = arachno::buildAppActionSchema("session.checkpoint.clear");
    assert(checkpointClearSchema.parameters.size() == 1);
    assert(checkpointClearSchema.parameters.front().name == "name");
    const arachno::AppActionSchema checkpointListSchema = arachno::buildAppActionSchema("session.checkpoint.list");
    assert(checkpointListSchema.parameters.empty());
    const arachno::AppActionSchema sessionSyncSchema = arachno::buildAppActionSchema("session.sync");
    assert(sessionSyncSchema.parameters.size() == 8);
    assert(sessionSyncSchema.parameters[0].name == "checkpoint_name");
    assert(sessionSyncSchema.parameters[1].name == "mode");
    assert(sessionSyncSchema.parameters[2].name == "stale_policy");
    assert(sessionSyncSchema.parameters[3].name == "max_events");
    assert(sessionSyncSchema.parameters[4].name == "snapshot_grid_start_row");
    assert(sessionSyncSchema.parameters[5].name == "snapshot_grid_row_count");
    assert(sessionSyncSchema.parameters[6].name == "update_checkpoint");
    assert(sessionSyncSchema.parameters[7].name == "create_if_missing");
    const arachno::AppActionSchema exportMixdownSchema = arachno::buildAppActionSchema("export.mixdown");
    assert(exportMixdownSchema.parameters.size() == 1);
    assert(exportMixdownSchema.parameters.front().name == "path");
    const arachno::AppActionSchema exportStemsSchema = arachno::buildAppActionSchema("export.stems");
    assert(exportStemsSchema.parameters.size() == 2);
    assert(exportStemsSchema.parameters[1].name == "format");
    assert(exportStemsSchema.parameters[1].type == arachno::AppActionParameterType::Choice);
    const arachno::AppActionSchema exportMidiSchema = arachno::buildAppActionSchema("export.midi");
    assert(exportMidiSchema.parameters.size() == 2);
    assert(exportMidiSchema.parameters[1].name == "ticks_per_quarter");
    assert(exportMidiSchema.parameters[1].type == arachno::AppActionParameterType::Integer);
    const arachno::AppActionSchema importMidiSchema = arachno::buildAppActionSchema("import.midi");
    assert(importMidiSchema.parameters.size() == 6);
    assert(importMidiSchema.parameters[1].name == "rows_per_beat");
    assert(importMidiSchema.parameters[2].name == "pattern_rows");
    assert(importMidiSchema.parameters[3].name == "split_by_track");
    assert(importMidiSchema.parameters[4].name == "split_by_program");
    assert(importMidiSchema.parameters[5].name == "preserve_tempo_map");
    const arachno::AppActionSchema scriptImportSchema = arachno::buildAppActionSchema("script.import");
    assert(scriptImportSchema.parameters.size() == 2);
    assert(scriptImportSchema.parameters[1].name == "name_override");

    const arachno::AppActionSchema probabilitySchema = arachno::buildAppActionSchema("editor.step.probability");
    assert(probabilitySchema.parameters.size() == 1);
    assert(probabilitySchema.parameters.front().name == "value");
    assert(!probabilitySchema.parameters.front().choices.empty());
    assert(arachno::validateAppActionParameters("editor.step.probability", {{"value", "clear"}}).ok);
    assert(arachno::validateAppActionParameters("editor.step.probability", {{"value", "0.5"}}).ok);
    assert(!arachno::validateAppActionParameters("editor.step.probability", {{"value", "1.5"}}).ok);

    const arachno::AppActionValidationResult invalidWave = arachno::validateAppActionParameters(
        "editor.instrument.wave",
        {{"instrument", "1"}, {"oscillator", "A"}, {"wave", "laser"}});
    assert(!invalidWave.ok);
    assert(invalidWave.fieldErrors.front().name == "wave");

    const arachno::AppActionValidationResult missingPath = arachno::validateAppActionParameters(
        "project.open",
        {});
    assert(!missingPath.ok);
    assert(missingPath.missingParameters.front() == "path");

    const arachno::AppActionValidationResult invalidBackend = arachno::validateAppActionParameters(
        "audio.runtime.configure",
        {{"backend", "wasapi"}});
    assert(!invalidBackend.ok);
    assert(invalidBackend.fieldErrors.front().name == "backend");
    assert(arachno::validateAppActionParameters(
        "audio.runtime.configure",
        {{"backend", "dummy"}, {"sample_rate", "48000"}, {"buffer_frames", "128"}, {"periods", "2"}})
        .ok);
    assert(!arachno::validateAppActionParameters("task.cancel", {}).ok);
    assert(!arachno::validateAppActionParameters("task.cancel", {{"task_id", "not-a-number"}}).ok);
    assert(arachno::validateAppActionParameters("session.events", {{"since", "0"}, {"drain", "false"}}).ok);
    assert(!arachno::validateAppActionParameters("session.events", {{"since", "bad"}}).ok);
    assert(!arachno::validateAppActionParameters("session.events", {{"drain", "maybe"}}).ok);
    assert(!arachno::validateAppActionParameters("session.events", {{"include_snapshot", "maybe"}}).ok);
    assert(arachno::validateAppActionParameters("session.events", {{"max_events", "0"}}).ok);
    assert(arachno::validateAppActionParameters("session.checkpoint.save", {{"name", "main"}}).ok);
    assert(!arachno::validateAppActionParameters("session.checkpoint.save", {}).ok);
    assert(!arachno::validateAppActionParameters("session.checkpoint.save", {{"name", "main"}, {"task_id", "bad"}}).ok);
    assert(arachno::validateAppActionParameters("session.checkpoint.advance", {{"name", "main"}, {"event_sequence", "12"}}).ok);
    assert(!arachno::validateAppActionParameters("session.checkpoint.advance", {{"name", "main"}}).ok);
    assert(!arachno::validateAppActionParameters("session.checkpoint.advance", {{"name", "main"}, {"event_sequence", "bad"}}).ok);
    assert(arachno::validateAppActionParameters("session.checkpoint.load", {{"name", "main"}}).ok);
    assert(!arachno::validateAppActionParameters("session.checkpoint.clear", {}).ok);
    assert(arachno::validateAppActionParameters("session.checkpoint.list", {}).ok);
    assert(arachno::validateAppActionParameters("session.sync", {{"checkpoint_name", "main"}}).ok);
    assert(!arachno::validateAppActionParameters("session.sync", {}).ok);
    assert(!arachno::validateAppActionParameters("session.sync", {{"checkpoint_name", "main"}, {"mode", "bad"}}).ok);
    assert(!arachno::validateAppActionParameters("session.sync", {{"checkpoint_name", "main"}, {"stale_policy", "bad"}}).ok);
    assert(!arachno::validateAppActionParameters("session.sync", {{"checkpoint_name", "main"}, {"update_checkpoint", "maybe"}}).ok);
    assert(!arachno::validateAppActionParameters("session.sync", {{"checkpoint_name", "main"}, {"create_if_missing", "maybe"}}).ok);
    assert(arachno::validateAppActionParameters("session.snapshot", {{"grid_start_row", "0"}, {"grid_row_count", "-1"}}).ok);
    assert(!arachno::validateAppActionParameters("session.snapshot", {{"grid_start_row", "bad"}}).ok);
    assert(arachno::validateAppActionParameters("export.stems", {{"path", "/tmp/stems"}, {"format", "wav"}}).ok);
    assert(!arachno::validateAppActionParameters("export.stems", {{"path", "/tmp/stems"}, {"format", "flac"}}).ok);
    assert(arachno::validateAppActionParameters("export.midi", {{"path", "/tmp/demo.mid"}, {"ticks_per_quarter", "0"}}).ok);
    assert(arachno::validateAppActionParameters("import.midi", {{"path", "/tmp/demo.mid"}}).ok);
    assert(!arachno::validateAppActionParameters("import.midi", {{"path", "/tmp/demo.mid"}, {"rows_per_beat", "bad"}}).ok);

    arachno::AppActionResult missingCommand = arachno::executeAppAction(app, {"editor.step.note"});
    assert(!missingCommand.ok);
    assert(
        missingCommand.error.find("command text") != std::string::npos
        || missingCommand.error.find("required action parameters") != std::string::npos);

    arachno::AppActionRequest invalidNoteRequest;
    invalidNoteRequest.actionId = "editor.step.note";
    invalidNoteRequest.parameters = {{"note", "H2"}, {"velocity", "0.8"}};
    const arachno::AppActionResult invalidNoteResult = arachno::executeAppAction(app, invalidNoteRequest);
    assert(!invalidNoteResult.ok);
    assert(
        invalidNoteResult.error.find("invalid action parameters") != std::string::npos
        || invalidNoteResult.error.find("command text") != std::string::npos
        || invalidNoteResult.error.find("required action parameters") != std::string::npos);

    arachno::AppActionRequest noteRequest;
    noteRequest.actionId = "editor.step.note";
    noteRequest.parameters = {{"note", "C5"}, {"velocity", "0.8"}};
    const arachno::AppActionResult note = arachno::executeAppAction(app, noteRequest);
    assert(note.ok);
    assert(note.projectChanged);
    assert(app.dirty());

    const arachno::AppActionResult saveDisabled = arachno::executeAppAction(app, {"project.save"});
    assert(!saveDisabled.ok);
    assert(saveDisabled.error.find("not been saved") != std::string::npos);

    const std::filesystem::path temp = std::filesystem::temp_directory_path();
    const std::filesystem::path projectPath = temp / "arachno-action-open.arachno";
    const std::filesystem::path savePath = temp / "arachno-action-save.arachno";
    const std::filesystem::path recoveryPath = temp / "arachno-action-recovery.arachno";
    const std::filesystem::path mixdownPath = temp / "arachno-action-mixdown.wav";
    const std::filesystem::path midiPath = temp / "arachno-action-export.mid";
    const std::filesystem::path midiImportPath = temp / "arachno-action-import.mid";
    const std::filesystem::path stemsPath = temp / "arachno-action-stems";
    const std::filesystem::path scriptPatchPath = temp / "arachno-action-script.arachnopatch";
    const std::filesystem::path scriptCommandsPath = temp / "arachno-action-script.arachno-edit";
    arachno::saveProject(arachno::makeDemoSong(), projectPath.string());
    {
        arachno::SynthPatch patch;
        patch.name = "ScriptPatch";
        patch.oscillatorA = arachno::Waveform::Saw;
        patch.cutoff = 0.88;
        arachno::savePatch(patch, scriptPatchPath.string());
    }
    {
        std::ofstream out(scriptCommandsPath);
        out << "move 2 1\n";
        out << "note D5 0.6\n";
    }
    arachno::exportMidiFile(arachno::makeDemoSong(), midiImportPath.string());

    arachno::AppActionRequest openWaiting;
    openWaiting.actionId = "project.open";
    openWaiting.path = projectPath.string();
    const arachno::AppActionResult waiting = arachno::executeAppAction(app, openWaiting);
    assert(!waiting.ok);
    assert(waiting.requiresUnsavedDecision);

    arachno::AppActionRequest saveAs;
    saveAs.actionId = "project.save_as";
    saveAs.path = savePath.string();
    const arachno::AppActionResult saved = arachno::executeAppAction(app, saveAs);
    assert(saved.ok);
    assert(!app.dirty());
    assert(app.projectPath() == savePath.string());

    app.applyEditorCommand("title Dirty Again");
    arachno::AppActionRequest openSave;
    openSave.actionId = "project.open";
    openSave.path = projectPath.string();
    openSave.unsavedChoice = arachno::UnsavedChangesChoice::Save;
    const arachno::AppActionResult opened = arachno::executeAppAction(app, openSave);
    assert(opened.ok);
    assert(!app.dirty());
    assert(app.projectPath() == projectPath.string());

    // Discard flow: dirty session + project.open with Discard must replace the
    // project (this is the GUI "DISCARD" button path for abandoning edits).
    app.applyEditorCommand("title Unfinished Work");
    assert(app.dirty());
    arachno::AppActionRequest openDiscard;
    openDiscard.actionId = "project.open";
    openDiscard.path = savePath.string();
    openDiscard.unsavedChoice = arachno::UnsavedChangesChoice::Discard;
    const arachno::AppActionResult discarded = arachno::executeAppAction(app, openDiscard);
    assert(discarded.ok);
    assert(!app.dirty());
    assert(app.projectPath() == savePath.string());
    // New-project discard: replaces the loaded song with a blank one.
    app.applyEditorCommand("title Unfinished Again");
    assert(app.dirty());
    arachno::AppActionRequest newDiscard;
    newDiscard.actionId = "project.new";
    newDiscard.unsavedChoice = arachno::UnsavedChangesChoice::Discard;
    const arachno::AppActionResult newProject = arachno::executeAppAction(app, newDiscard);
    assert(newProject.ok);
    assert(!app.dirty());

    const arachno::AppActionResult play = arachno::executeAppAction(app, {"playback.play"});
    assert(play.ok);
    assert(app.playback().snapshot().state == arachno::TransportState::Playing);
    const arachno::AppActionResult playAgain = arachno::executeAppAction(app, {"playback.play"});
    assert(!playAgain.ok);
    const arachno::AppActionResult stop = arachno::executeAppAction(app, {"playback.stop"});
    assert(stop.ok);

    const std::vector<arachno::AppActionEntry> audioEntries =
        arachno::buildApplicationActionPalette(app, "audio");
    assert(std::any_of(
        audioEntries.begin(),
        audioEntries.end(),
        [](const arachno::AppActionEntry& entry) {
            return entry.id == "audio.runtime.configure" && entry.parameterCount == 7;
        }));
    assert(std::any_of(
        audioEntries.begin(),
        audioEntries.end(),
        [](const arachno::AppActionEntry& entry) {
            return entry.id == "audio.runtime.devices" && entry.parameterCount == 2;
        }));
    assert(std::any_of(
        audioEntries.begin(),
        audioEntries.end(),
        [](const arachno::AppActionEntry& entry) {
            return entry.id == "audio.runtime.status" && entry.parameterCount == 0;
        }));
    assert(std::any_of(
        audioEntries.begin(),
        audioEntries.end(),
        [](const arachno::AppActionEntry& entry) {
            return entry.id == "audio.runtime.render_test" && entry.parameterCount == 2;
        }));
    assert(std::any_of(
        audioEntries.begin(),
        audioEntries.end(),
        [](const arachno::AppActionEntry& entry) {
            return entry.id == "audio.runtime.simulate_underrun" && entry.parameterCount == 1;
        }));
    const std::vector<arachno::AppActionEntry> taskEntriesAtRest =
        arachno::buildApplicationActionPalette(app, "task");
    assert(std::any_of(
        taskEntriesAtRest.begin(),
        taskEntriesAtRest.end(),
        [](const arachno::AppActionEntry& entry) {
            return entry.id == "task.cancel" && entry.parameterCount == 2 && !entry.enabled;
        }));
    assert(std::any_of(
        taskEntriesAtRest.begin(),
        taskEntriesAtRest.end(),
        [](const arachno::AppActionEntry& entry) {
            return entry.id == "task.clear_finished" && entry.parameterCount == 0 && !entry.enabled;
        }));
    const std::vector<arachno::AppActionEntry> sessionEntries =
        arachno::buildApplicationActionPalette(app, "session");
    assert(std::any_of(
        sessionEntries.begin(),
        sessionEntries.end(),
        [](const arachno::AppActionEntry& entry) {
            return entry.id == "session.events" && entry.parameterCount == 6;
        }));
    assert(std::any_of(
        sessionEntries.begin(),
        sessionEntries.end(),
        [](const arachno::AppActionEntry& entry) {
            return entry.id == "session.snapshot" && entry.parameterCount == 2;
        }));
    assert(std::any_of(
        sessionEntries.begin(),
        sessionEntries.end(),
        [](const arachno::AppActionEntry& entry) {
            return entry.id == "session.checkpoint.save" && entry.parameterCount == 4;
        }));
    assert(std::any_of(
        sessionEntries.begin(),
        sessionEntries.end(),
        [](const arachno::AppActionEntry& entry) {
            return entry.id == "session.checkpoint.advance" && entry.parameterCount == 6;
        }));
    assert(std::any_of(
        sessionEntries.begin(),
        sessionEntries.end(),
        [](const arachno::AppActionEntry& entry) {
            return entry.id == "session.checkpoint.load" && entry.parameterCount == 1;
        }));
    assert(std::any_of(
        sessionEntries.begin(),
        sessionEntries.end(),
        [](const arachno::AppActionEntry& entry) {
            return entry.id == "session.checkpoint.clear" && entry.parameterCount == 1;
        }));
    assert(std::any_of(
        sessionEntries.begin(),
        sessionEntries.end(),
        [](const arachno::AppActionEntry& entry) {
            return entry.id == "session.sync" && entry.parameterCount == 8;
        }));
    assert(std::any_of(
        sessionEntries.begin(),
        sessionEntries.end(),
        [](const arachno::AppActionEntry& entry) {
            return entry.id == "session.checkpoint.list" && entry.parameterCount == 0;
        }));
    const std::vector<arachno::AppActionEntry> exportEntries =
        arachno::buildApplicationActionPalette(app, "export");
    assert(std::any_of(
        exportEntries.begin(),
        exportEntries.end(),
        [](const arachno::AppActionEntry& entry) {
            return entry.id == "export.mixdown" && entry.requiresPath && entry.parameterCount == 1;
        }));
    assert(std::any_of(
        exportEntries.begin(),
        exportEntries.end(),
        [](const arachno::AppActionEntry& entry) {
            return entry.id == "export.stems" && entry.requiresPath && entry.parameterCount == 2;
        }));
    assert(std::any_of(
        exportEntries.begin(),
        exportEntries.end(),
        [](const arachno::AppActionEntry& entry) {
            return entry.id == "export.midi" && entry.requiresPath && entry.parameterCount == 2;
        }));
    const std::vector<arachno::AppActionEntry> scriptEntries =
        arachno::buildApplicationActionPalette(app, "script");
    assert(std::any_of(
        scriptEntries.begin(),
        scriptEntries.end(),
        [](const arachno::AppActionEntry& entry) {
            return entry.id == "script.import" && entry.requiresPath && entry.parameterCount == 2;
        }));

    arachno::AppActionRequest badAudioConfig;
    badAudioConfig.actionId = "audio.runtime.configure";
    badAudioConfig.parameters = {{"sample_rate", "bad"}};
    const arachno::AppActionResult badConfigured = arachno::executeAppAction(app, badAudioConfig);
    assert(!badConfigured.ok);
    assert(badConfigured.error.find("invalid action parameters") != std::string::npos);

    arachno::AppActionRequest audioConfig;
    audioConfig.actionId = "audio.runtime.configure";
    audioConfig.parameters = {
        {"backend", "dummy"},
        {"device_id", "dummy/offline"},
        {"sample_rate", "44100"},
        {"buffer_frames", "128"},
        {"periods", "3"},
        {"realtime_priority", "false"},
        {"connect_outputs", "false"}};
    const arachno::AppActionResult configured = arachno::executeAppAction(app, audioConfig);
    assert(configured.ok);
    assert(app.settings().audioRuntime.backend == "dummy");
    assert(app.settings().audioRuntime.deviceId == "dummy/offline");
    assert(app.settings().audioRuntime.sampleRate == 44100);
    assert(app.settings().audioRuntime.bufferFrames == 128);
    assert(app.settings().audioRuntime.periods == 3);
    assert(!app.settings().audioRuntime.realtimePriority);
    assert(!app.settings().audioRuntime.connectSystemOutputs);
    const std::size_t audioTaskCountAfterConfigure = app.snapshot().tasks.size();
    assert(audioTaskCountAfterConfigure > 0);
    assert(app.snapshot().tasks.back().kind == arachno::AppTaskKind::Audio);
    assert(app.snapshot().tasks.back().state == arachno::AppTaskState::Succeeded);

    const arachno::AppActionResult audioStart = arachno::executeAppAction(app, {"audio.runtime.start"});
    assert(audioStart.ok);
    assert(app.audioRuntimeHealth().active);
    assert(audioStart.hasAudioRuntime);
    assert(audioStart.audioRuntime.active);
    assert(audioStart.hasMessageSeverity);
    assert(audioStart.messageSeverity == arachno::AppMessageSeverity::Info);
    const std::size_t audioTaskCountAfterStart = app.snapshot().tasks.size();
    assert(audioTaskCountAfterStart > audioTaskCountAfterConfigure);
    assert(app.snapshot().tasks.back().kind == arachno::AppTaskKind::Audio);
    assert(app.snapshot().tasks.back().state == arachno::AppTaskState::Succeeded);
    const arachno::AppActionResult audioStartAgain = arachno::executeAppAction(app, {"audio.runtime.start"});
    assert(!audioStartAgain.ok);
    assert(audioStartAgain.hasMessageSeverity);
    assert(audioStartAgain.messageSeverity == arachno::AppMessageSeverity::Error);
    const arachno::AppActionResult audioStop = arachno::executeAppAction(app, {"audio.runtime.stop"});
    assert(audioStop.ok);
    assert(!app.audioRuntimeHealth().active);
    assert(audioStop.hasAudioRuntime);
    assert(!audioStop.audioRuntime.active);
    assert(audioStop.hasMessageSeverity);
    assert(audioStop.messageSeverity == arachno::AppMessageSeverity::Info);
    assert(app.snapshot().tasks.size() > audioTaskCountAfterStart);
    assert(app.snapshot().tasks.back().kind == arachno::AppTaskKind::Audio);
    assert(app.snapshot().tasks.back().state == arachno::AppTaskState::Succeeded);

    const arachno::AppActionResult audioStatus = arachno::executeAppAction(app, {"audio.runtime.status"});
    assert(audioStatus.ok);
    assert(audioStatus.hasAudioRuntime);
    assert(audioStatus.audioRuntime.backend == app.audioRuntimeHealth().backend);
    assert(audioStatus.hasMessageSeverity);
    assert(audioStatus.messageSeverity == arachno::AppMessageSeverity::Info);
    const arachno::AppActionResult audioDevices = arachno::executeAppAction(app, {"audio.runtime.devices"});
    assert(audioDevices.ok);
    assert(audioDevices.hasAudioDevices);
    assert(!audioDevices.audioDevices.empty());
    assert(audioDevices.hasMessageSeverity);
    assert(audioDevices.messageSeverity == arachno::AppMessageSeverity::Info);
    const std::string renderedAudioDevices = arachno::renderApplicationActionResult(audioDevices);
    assert(renderedAudioDevices.find("Action result: audio.runtime.devices") != std::string::npos);
    assert(renderedAudioDevices.find("audio_devices:") != std::string::npos);
    arachno::AppActionRequest filteredAudioDevices;
    filteredAudioDevices.actionId = "audio.runtime.devices";
    filteredAudioDevices.parameters = {{"backend", "dummy"}, {"only_available", "true"}};
    const arachno::AppActionResult filteredDevices = arachno::executeAppAction(app, filteredAudioDevices);
    assert(filteredDevices.ok);
    assert(filteredDevices.hasAudioDevices);
    assert(filteredDevices.audioDevices.size() == 1);
    assert(filteredDevices.audioDevices.front().backend == arachno::AudioBackendType::Dummy);
    assert(filteredDevices.audioDevices.front().available);
    arachno::AppActionRequest badDeviceFilter;
    badDeviceFilter.actionId = "audio.runtime.devices";
    badDeviceFilter.parameters = {{"only_available", "maybe"}};
    const arachno::AppActionResult badDevices = arachno::executeAppAction(app, badDeviceFilter);
    assert(!badDevices.ok);
    assert(badDevices.messageSeverity == arachno::AppMessageSeverity::Error);
    arachno::AppActionRequest badRenderTest;
    badRenderTest.actionId = "audio.runtime.render_test";
    badRenderTest.parameters = {{"frame_count", "bad"}, {"block_count", "2"}};
    const arachno::AppActionResult badRendered = arachno::executeAppAction(app, badRenderTest);
    assert(!badRendered.ok);
    assert(badRendered.messageSeverity == arachno::AppMessageSeverity::Error);
    arachno::AppActionRequest renderTest;
    renderTest.actionId = "audio.runtime.render_test";
    renderTest.parameters = {{"frame_count", "64"}, {"block_count", "3"}};
    const arachno::AppActionResult renderTestResult = arachno::executeAppAction(app, renderTest);
    assert(renderTestResult.ok);
    assert(renderTestResult.hasAudioRuntime);
    assert(renderTestResult.hasAudioRenderTest);
    assert(renderTestResult.audioRenderFrameCount == 64);
    assert(renderTestResult.audioRenderBlockCount == 3);
    assert(renderTestResult.audioRenderProcessedBlocks == 3);
    assert(renderTestResult.audioRenderProcessedFrames == 192);
    assert(renderTestResult.audioRenderUnderrunsAfter >= renderTestResult.audioRenderUnderrunsBefore);
    assert(renderTestResult.messageSeverity == arachno::AppMessageSeverity::Info);
    const int underrunsBeforeSimulate = app.audioRuntimeHealth().underrunCount;
    arachno::AppActionRequest simulateUnderrun;
    simulateUnderrun.actionId = "audio.runtime.simulate_underrun";
    simulateUnderrun.parameters = {{"detail", "ui diagnostic test"}};
    const arachno::AppActionResult simulated = arachno::executeAppAction(app, simulateUnderrun);
    assert(simulated.ok);
    assert(simulated.hasAudioRuntime);
    assert(simulated.audioRuntime.underrunCount == underrunsBeforeSimulate + 1);
    assert(simulated.hasMessageSeverity);
    assert(simulated.messageSeverity == arachno::AppMessageSeverity::Warning);
    const std::string renderedActionResult = arachno::renderApplicationActionResult(simulated);
    assert(renderedActionResult.find("severity: warning") != std::string::npos);
    assert(renderedActionResult.find("audio: backend=") != std::string::npos);
    const std::string serializedActionResult = arachno::serializeApplicationActionResult(simulated);
    assert(serializedActionResult.find("\"action_id\":\"audio.runtime.simulate_underrun\"") != std::string::npos);
    assert(serializedActionResult.find("\"severity\":\"warning\"") != std::string::npos);
    assert(serializedActionResult.find("\"audio_runtime\"") != std::string::npos);
    assert(app.snapshot().tasks.back().kind == arachno::AppTaskKind::Audio);
    assert(app.snapshot().tasks.back().state == arachno::AppTaskState::Succeeded);

    const arachno::AppTaskId activeTask = app.taskManager().startTask(
        arachno::AppTaskKind::Script,
        "Long script",
        10,
        "script.py");
    const std::vector<arachno::AppActionEntry> activeTaskEntries =
        arachno::buildApplicationActionPalette(app, "task");
    assert(std::any_of(
        activeTaskEntries.begin(),
        activeTaskEntries.end(),
        [](const arachno::AppActionEntry& entry) {
            return entry.id == "task.cancel" && entry.enabled;
        }));

    arachno::AppActionRequest unknownTaskCancel;
    unknownTaskCancel.actionId = "task.cancel";
    unknownTaskCancel.parameters = {{"task_id", "99999"}};
    const arachno::AppActionResult unknownCanceled = arachno::executeAppAction(app, unknownTaskCancel);
    assert(!unknownCanceled.ok);
    assert(unknownCanceled.error.find("not found") != std::string::npos);

    arachno::AppActionRequest taskCancel;
    taskCancel.actionId = "task.cancel";
    taskCancel.parameters = {
        {"task_id", std::to_string(activeTask)},
        {"message", "Stopped from UI"}};
    const arachno::AppActionResult canceledTask = arachno::executeAppAction(app, taskCancel);
    assert(canceledTask.ok);
    assert(canceledTask.hasMessageSeverity);
    assert(canceledTask.messageSeverity == arachno::AppMessageSeverity::Warning);
    const arachno::AppTaskSnapshot* canceledTaskSnapshot = app.taskManager().findTask(activeTask);
    assert(canceledTaskSnapshot != nullptr);
    assert(canceledTaskSnapshot->state == arachno::AppTaskState::Canceled);
    assert(canceledTaskSnapshot->message == "Stopped from UI");

    const arachno::AppActionResult clearedTasks = arachno::executeAppAction(app, {"task.clear_finished"});
    assert(clearedTasks.ok);
    assert(app.taskManager().tasks().empty());
    const std::vector<arachno::AppActionEntry> taskEntriesAfterClear =
        arachno::buildApplicationActionPalette(app, "task");
    assert(std::any_of(
        taskEntriesAfterClear.begin(),
        taskEntriesAfterClear.end(),
        [](const arachno::AppActionEntry& entry) {
            return entry.id == "task.clear_finished" && !entry.enabled;
        }));

    app.drainEvents();
    app.applyEditorCommand("move 1 1");
    arachno::AppActionRequest badSessionSnapshot;
    badSessionSnapshot.actionId = "session.snapshot";
    badSessionSnapshot.parameters = {{"grid_start_row", "-1"}};
    const arachno::AppActionResult badSnapshot = arachno::executeAppAction(app, badSessionSnapshot);
    assert(!badSnapshot.ok);
    assert(badSnapshot.error.find("non-negative integer") != std::string::npos);

    arachno::AppActionRequest sessionSnapshot;
    sessionSnapshot.actionId = "session.snapshot";
    sessionSnapshot.parameters = {{"grid_start_row", "4"}, {"grid_row_count", "12"}};
    const arachno::AppActionResult snapshotResult = arachno::executeAppAction(app, sessionSnapshot);
    assert(snapshotResult.ok);
    assert(snapshotResult.hasSessionSnapshot);
    assert(snapshotResult.hasLastEventSequence);
    assert(snapshotResult.sessionSnapshot.editor.activeGrid.startRow == 4);
    assert(snapshotResult.sessionSnapshot.editor.activeGrid.rowCount == 12);
    assert(snapshotResult.sessionSnapshot.editor.status.cursorRow == 1);
    assert(snapshotResult.sessionSnapshot.editor.status.cursorTrack == 1);
    const std::string renderedSnapshot = arachno::renderApplicationActionResult(snapshotResult);
    assert(renderedSnapshot.find("session_snapshot:") != std::string::npos);
    const std::string serializedSnapshot = arachno::serializeApplicationActionResult(snapshotResult);
    assert(serializedSnapshot.find("\"session_snapshot\"") != std::string::npos);
    assert(serializedSnapshot.find("\"grid_start_row\":4") != std::string::npos);
    assert(serializedSnapshot.find("\"grid_row_count\":12") != std::string::npos);

    arachno::AppActionRequest badSessionEvents;
    badSessionEvents.actionId = "session.events";
    badSessionEvents.parameters = {{"since", "-1"}};
    const arachno::AppActionResult badEvents = arachno::executeAppAction(app, badSessionEvents);
    assert(!badEvents.ok);
    assert(
        badEvents.error.find("non-negative integer") != std::string::npos
        || badEvents.error.find("invalid action parameters") != std::string::npos);
    arachno::AppActionRequest badSessionEventLimit;
    badSessionEventLimit.actionId = "session.events";
    badSessionEventLimit.parameters = {{"max_events", "0"}};
    const arachno::AppActionResult badEventLimit = arachno::executeAppAction(app, badSessionEventLimit);
    assert(!badEventLimit.ok);
    assert(badEventLimit.error.find("positive integer") != std::string::npos);

    arachno::AppActionRequest eventsSince;
    eventsSince.actionId = "session.events";
    eventsSince.parameters = {{"since", "0"}};
    const arachno::AppActionResult sessionEvents = arachno::executeAppAction(app, eventsSince);
    assert(sessionEvents.ok);
    assert(sessionEvents.hasEvents);
    assert(sessionEvents.hasEventDeltaSummary);
    assert(sessionEvents.hasEventCursor);
    assert(!sessionEvents.events.empty());
    assert(sessionEvents.hasLastEventSequence);
    assert(sessionEvents.lastEventSequence >= sessionEvents.events.back().sequence);
    assert(sessionEvents.eventDeltaSummary.total == static_cast<int>(sessionEvents.events.size()));
    assert(sessionEvents.eventDeltaSummary.returned == static_cast<int>(sessionEvents.events.size()));
    assert(!sessionEvents.eventDeltaSummary.truncated);
    assert(sessionEvents.eventCursor.requestedSince == 0);
    assert(sessionEvents.eventCursor.hasReturnedRange);
    assert(sessionEvents.eventCursor.returnedTo == sessionEvents.events.back().sequence);
    assert(sessionEvents.eventCursor.recommendedSince == sessionEvents.events.back().sequence);
    assert(!sessionEvents.eventCursor.includesSnapshot);
    assert(!sessionEvents.eventCursor.drain);
    assert(std::any_of(
        sessionEvents.events.begin(),
        sessionEvents.events.end(),
        [](const arachno::AppEvent& event) {
            return event.type == arachno::AppEventType::EditorChanged;
        }));
    const std::string renderedEvents = arachno::renderApplicationActionResult(sessionEvents);
    assert(renderedEvents.find("events:") != std::string::npos);
    assert(renderedEvents.find("event_delta:") != std::string::npos);
    assert(renderedEvents.find("event_cursor:") != std::string::npos);
    assert(renderedEvents.find("last_event_sequence:") != std::string::npos);
    const std::string serializedEvents = arachno::serializeApplicationActionResult(sessionEvents);
    assert(serializedEvents.find("\"event_delta\"") != std::string::npos);
    assert(serializedEvents.find("\"event_cursor\"") != std::string::npos);
    assert(serializedEvents.find("\"events\"") != std::string::npos);
    assert(serializedEvents.find("\"items\"") != std::string::npos);

    arachno::AppActionRequest limitedEventsSince;
    limitedEventsSince.actionId = "session.events";
    limitedEventsSince.parameters = {
        {"since", "0"},
        {"max_events", "1"},
        {"snapshot_grid_start_row", "2"},
        {"snapshot_grid_row_count", "10"}};
    const arachno::AppActionResult limitedEvents = arachno::executeAppAction(app, limitedEventsSince);
    assert(limitedEvents.ok);
    assert(limitedEvents.hasEvents);
    assert(limitedEvents.hasEventDeltaSummary);
    assert(limitedEvents.events.size() == 1);
    assert(limitedEvents.eventDeltaSummary.total >= 1);
    assert(limitedEvents.eventDeltaSummary.returned == 1);
    assert(limitedEvents.eventDeltaSummary.truncated == (limitedEvents.eventDeltaSummary.total > 1));
    assert(limitedEvents.eventDeltaSummary.dropped
        == limitedEvents.eventDeltaSummary.total - limitedEvents.eventDeltaSummary.returned);
    assert(limitedEvents.hasEventCursor);
    assert(limitedEvents.eventCursor.requestedSince == 0);
    assert(limitedEvents.eventCursor.hasReturnedRange);
    assert(limitedEvents.eventCursor.returnedTo == limitedEvents.events.back().sequence);
    assert(limitedEvents.eventCursor.truncated == limitedEvents.eventDeltaSummary.truncated);
    if (limitedEvents.eventDeltaSummary.truncated) {
        assert(limitedEvents.hasSessionSnapshot);
        assert(limitedEvents.sessionSnapshot.editor.activeGrid.startRow == 2);
        assert(limitedEvents.sessionSnapshot.editor.activeGrid.rowCount == 10);
        assert(limitedEvents.eventCursor.includesSnapshot);
        assert(limitedEvents.eventCursor.recommendedSince == limitedEvents.lastEventSequence);
    }

    arachno::AppActionRequest eventsWithSnapshot;
    eventsWithSnapshot.actionId = "session.events";
    eventsWithSnapshot.parameters = {
        {"since", "0"},
        {"max_events", "1"},
        {"include_snapshot", "true"},
        {"snapshot_grid_start_row", "3"},
        {"snapshot_grid_row_count", "11"}};
    const arachno::AppActionResult eventsWithExplicitSnapshot =
        arachno::executeAppAction(app, eventsWithSnapshot);
    assert(eventsWithExplicitSnapshot.ok);
    assert(eventsWithExplicitSnapshot.hasEvents);
    assert(eventsWithExplicitSnapshot.hasSessionSnapshot);
    assert(eventsWithExplicitSnapshot.hasEventCursor);
    assert(eventsWithExplicitSnapshot.eventCursor.includesSnapshot);
    assert(eventsWithExplicitSnapshot.eventCursor.recommendedSince == eventsWithExplicitSnapshot.lastEventSequence);
    assert(eventsWithExplicitSnapshot.sessionSnapshot.editor.activeGrid.startRow == 3);
    assert(eventsWithExplicitSnapshot.sessionSnapshot.editor.activeGrid.rowCount == 11);

    arachno::AppActionRequest eventsDrain;
    eventsDrain.actionId = "session.events";
    eventsDrain.parameters = {{"drain", "true"}};
    const arachno::AppActionResult drainedEvents = arachno::executeAppAction(app, eventsDrain);
    assert(drainedEvents.ok);
    assert(drainedEvents.hasEvents);
    assert(drainedEvents.hasEventCursor);
    assert(drainedEvents.eventCursor.drain);
    assert(drainedEvents.eventCursor.recommendedSince == drainedEvents.lastEventSequence);
    assert(!drainedEvents.events.empty());
    const arachno::AppActionResult drainedEventsAgain = arachno::executeAppAction(app, eventsDrain);
    assert(drainedEventsAgain.ok);
    assert(drainedEventsAgain.hasEvents);
    assert(drainedEventsAgain.events.empty());

    arachno::AppActionRequest bootstrapSyncMissing;
    bootstrapSyncMissing.actionId = "session.sync";
    bootstrapSyncMissing.parameters = {{"checkpoint_name", "bootstrap-ui"}};
    const arachno::AppActionResult bootstrapMissing = arachno::executeAppAction(app, bootstrapSyncMissing);
    assert(!bootstrapMissing.ok);
    assert(bootstrapMissing.error.find("not found") != std::string::npos);
    assert(bootstrapMissing.hasSyncCheckpointCompatibility);
    assert(bootstrapMissing.syncCheckpointCompatibility == arachno::SyncCheckpointCompatibility::Missing);
    assert(bootstrapMissing.hasSyncCheckpointUpdateStatus);
    assert(bootstrapMissing.syncCheckpointUpdateStatus == arachno::SyncCheckpointUpdateStatus::Failed);

    arachno::AppActionRequest bootstrapSyncCreate = bootstrapSyncMissing;
    bootstrapSyncCreate.parameters = {
        {"checkpoint_name", "bootstrap-ui"},
        {"create_if_missing", "true"},
        {"mode", "force_snapshot"},
        {"snapshot_grid_start_row", "5"},
        {"snapshot_grid_row_count", "9"}};
    const arachno::AppActionResult bootstrapCreated = arachno::executeAppAction(app, bootstrapSyncCreate);
    assert(bootstrapCreated.ok);
    assert(bootstrapCreated.hasSyncCheckpoint);
    assert(bootstrapCreated.syncCheckpoint.name == "bootstrap-ui");
    assert(bootstrapCreated.hasSyncCheckpointCompatibility);
    assert(bootstrapCreated.syncCheckpointCompatibility == arachno::SyncCheckpointCompatibility::Ok);
    assert(bootstrapCreated.hasSyncCheckpointUpdateStatus);
    assert(bootstrapCreated.syncCheckpointUpdateStatus == arachno::SyncCheckpointUpdateStatus::Advanced);
    assert(bootstrapCreated.hasSuggestedSyncCheckpoint);
    assert(bootstrapCreated.suggestedSyncCheckpointSafeToCommit);
    assert(bootstrapCreated.suggestedSyncCheckpoint.name == "bootstrap-ui");
    assert(bootstrapCreated.hasSessionSnapshot);
    assert(bootstrapCreated.sessionSnapshot.editor.activeGrid.startRow == 5);
    assert(bootstrapCreated.sessionSnapshot.editor.activeGrid.rowCount == 9);

    arachno::AppActionRequest saveCheckpoint;
    saveCheckpoint.actionId = "session.checkpoint.save";
    saveCheckpoint.parameters = {{"name", "main-ui"}, {"event_sequence", "55"}, {"task_id", "12"}};
    const arachno::AppActionResult savedCheckpoint = arachno::executeAppAction(app, saveCheckpoint);
    assert(savedCheckpoint.ok);
    assert(savedCheckpoint.hasSyncCheckpoint);
    assert(savedCheckpoint.hasSyncCheckpointCompatibility);
    assert(savedCheckpoint.syncCheckpointCompatibility == arachno::SyncCheckpointCompatibility::Ok);
    assert(savedCheckpoint.syncCheckpoint.name == "main-ui");
    assert(savedCheckpoint.syncCheckpoint.eventSequence == 55);
    assert(savedCheckpoint.syncCheckpoint.taskId == 12);

    arachno::AppActionRequest saveCasCheckpoint;
    saveCasCheckpoint.actionId = "session.checkpoint.save";
    saveCasCheckpoint.parameters = {{"name", "cas-ui"}, {"event_sequence", "10"}, {"task_id", "2"}};
    const arachno::AppActionResult savedCasCheckpoint = arachno::executeAppAction(app, saveCasCheckpoint);
    assert(savedCasCheckpoint.ok);
    assert(savedCasCheckpoint.hasSyncCheckpoint);
    assert(savedCasCheckpoint.syncCheckpoint.eventSequence == 10);

    arachno::AppActionRequest advanceCasCheckpoint;
    advanceCasCheckpoint.actionId = "session.checkpoint.advance";
    advanceCasCheckpoint.parameters = {
        {"name", "cas-ui"},
        {"event_sequence", "20"},
        {"task_id", "3"},
        {"expected_event_sequence", "10"},
        {"expected_task_id", "2"}};
    const arachno::AppActionResult advancedCasCheckpoint = arachno::executeAppAction(app, advanceCasCheckpoint);
    assert(advancedCasCheckpoint.ok);
    assert(advancedCasCheckpoint.hasSyncCheckpoint);
    assert(advancedCasCheckpoint.syncCheckpoint.eventSequence == 20);
    assert(advancedCasCheckpoint.syncCheckpoint.taskId == 3);
    assert(advancedCasCheckpoint.hasSyncCheckpointUpdateStatus);
    assert(advancedCasCheckpoint.syncCheckpointUpdateStatus == arachno::SyncCheckpointUpdateStatus::Advanced);

    arachno::AppActionRequest advanceCasConflict = advanceCasCheckpoint;
    advanceCasConflict.parameters = {
        {"name", "cas-ui"},
        {"event_sequence", "30"},
        {"task_id", "4"},
        {"expected_event_sequence", "10"}};
    const arachno::AppActionResult conflictCasCheckpoint = arachno::executeAppAction(app, advanceCasConflict);
    assert(!conflictCasCheckpoint.ok);
    assert(conflictCasCheckpoint.hasSyncCheckpoint);
    assert(conflictCasCheckpoint.syncCheckpoint.eventSequence == 20);
    assert(conflictCasCheckpoint.hasSyncCheckpointUpdateStatus);
    assert(conflictCasCheckpoint.syncCheckpointUpdateStatus == arachno::SyncCheckpointUpdateStatus::Failed);
    assert(conflictCasCheckpoint.error.find("conflict") != std::string::npos);

    app.applyEditorCommand("move 3 1");
    app.applyEditorCommand("move 4 1");
    arachno::AppActionRequest syncDeltaOnly;
    syncDeltaOnly.actionId = "session.sync";
    syncDeltaOnly.parameters = {
        {"checkpoint_name", "main-ui"},
        {"mode", "delta_only"},
        {"max_events", "1"},
        {"update_checkpoint", "false"}};
    const arachno::AppActionResult syncedDeltaOnly = arachno::executeAppAction(app, syncDeltaOnly);
    assert(syncedDeltaOnly.ok);
    assert(syncedDeltaOnly.hasEvents);
    assert(syncedDeltaOnly.hasEventDeltaSummary);
    assert(syncedDeltaOnly.events.size() == 1);
    assert(!syncedDeltaOnly.hasSessionSnapshot);
    assert(syncedDeltaOnly.hasSyncCheckpoint);
    assert(syncedDeltaOnly.hasSyncCheckpointCompatibility);
    assert(syncedDeltaOnly.syncCheckpointCompatibility == arachno::SyncCheckpointCompatibility::Ok);
    assert(syncedDeltaOnly.hasSyncCheckpointUpdateStatus);
    assert(syncedDeltaOnly.syncCheckpointUpdateStatus == arachno::SyncCheckpointUpdateStatus::SkippedUpdateDisabled);
    assert(syncedDeltaOnly.hasEventCursor);
    assert(syncedDeltaOnly.eventCursor.requestedSince == 55);
    assert(syncedDeltaOnly.eventCursor.hasReturnedRange);
    assert(syncedDeltaOnly.eventCursor.recommendedSince == syncedDeltaOnly.events.back().sequence);
    assert(syncedDeltaOnly.hasSuggestedSyncCheckpoint);
    assert(syncedDeltaOnly.suggestedSyncCheckpoint.name == "main-ui");
    if (syncedDeltaOnly.eventDeltaSummary.truncated) {
        assert(!syncedDeltaOnly.suggestedSyncCheckpointSafeToCommit);
        assert(syncedDeltaOnly.suggestedSyncCheckpoint.eventSequence == 55);
    } else {
        assert(syncedDeltaOnly.suggestedSyncCheckpointSafeToCommit);
        assert(syncedDeltaOnly.suggestedSyncCheckpoint.eventSequence == syncedDeltaOnly.lastEventSequence);
    }
    assert(syncedDeltaOnly.syncCheckpoint.eventSequence == 55);

    arachno::AppActionRequest syncWithFallback;
    syncWithFallback.actionId = "session.sync";
    syncWithFallback.parameters = {
        {"checkpoint_name", "main-ui"},
        {"mode", "delta_with_fallback"},
        {"max_events", "1"},
        {"snapshot_grid_start_row", "6"},
        {"snapshot_grid_row_count", "8"},
        {"update_checkpoint", "true"}};
    const arachno::AppActionResult syncedFallback = arachno::executeAppAction(app, syncWithFallback);
    assert(syncedFallback.ok);
    assert(syncedFallback.hasEvents);
    assert(syncedFallback.hasEventDeltaSummary);
    if (syncedFallback.eventDeltaSummary.truncated) {
        assert(syncedFallback.hasSessionSnapshot);
        assert(syncedFallback.sessionSnapshot.editor.activeGrid.startRow == 6);
        assert(syncedFallback.sessionSnapshot.editor.activeGrid.rowCount == 8);
    }
    assert(syncedFallback.hasSyncCheckpoint);
    assert(syncedFallback.hasSyncCheckpointUpdateStatus);
    assert(syncedFallback.syncCheckpointUpdateStatus == arachno::SyncCheckpointUpdateStatus::Advanced);
    assert(syncedFallback.hasEventCursor);
    assert(syncedFallback.hasSuggestedSyncCheckpoint);
    assert(syncedFallback.suggestedSyncCheckpointSafeToCommit);
    assert(syncedFallback.suggestedSyncCheckpoint.eventSequence == syncedFallback.lastEventSequence);
    if (syncedFallback.hasSessionSnapshot) {
        assert(syncedFallback.eventCursor.includesSnapshot);
        assert(syncedFallback.eventCursor.recommendedSince == syncedFallback.lastEventSequence);
    }

    arachno::AppActionRequest saveTruncatedCheckpoint;
    saveTruncatedCheckpoint.actionId = "session.checkpoint.save";
    saveTruncatedCheckpoint.parameters = {{"name", "truncated-ui"}};
    const arachno::AppActionResult savedTruncatedCheckpoint = arachno::executeAppAction(app, saveTruncatedCheckpoint);
    assert(savedTruncatedCheckpoint.ok);
    assert(savedTruncatedCheckpoint.hasSyncCheckpoint);
    const std::uint64_t truncatedStartSequence = savedTruncatedCheckpoint.syncCheckpoint.eventSequence;

    app.applyEditorCommand("move 7 1");
    app.applyEditorCommand("move 8 1");
    arachno::AppActionRequest truncatedDeltaSync;
    truncatedDeltaSync.actionId = "session.sync";
    truncatedDeltaSync.parameters = {
        {"checkpoint_name", "truncated-ui"},
        {"mode", "delta_only"},
        {"max_events", "1"},
        {"update_checkpoint", "true"}};
    const arachno::AppActionResult truncatedDeltaResult = arachno::executeAppAction(app, truncatedDeltaSync);
    assert(truncatedDeltaResult.ok);
    assert(truncatedDeltaResult.hasEventDeltaSummary);
    assert(truncatedDeltaResult.eventDeltaSummary.truncated);
    assert(truncatedDeltaResult.hasSyncCheckpointUpdateStatus);
    assert(truncatedDeltaResult.syncCheckpointUpdateStatus == arachno::SyncCheckpointUpdateStatus::SkippedUnsafeDelta);
    assert(truncatedDeltaResult.hasEventCursor);
    assert(truncatedDeltaResult.eventCursor.hasReturnedRange);
    assert(truncatedDeltaResult.eventCursor.recommendedSince == truncatedDeltaResult.events.back().sequence);
    assert(truncatedDeltaResult.hasSuggestedSyncCheckpoint);
    assert(!truncatedDeltaResult.suggestedSyncCheckpointSafeToCommit);
    assert(truncatedDeltaResult.suggestedSyncCheckpoint.eventSequence == truncatedStartSequence);
    assert(truncatedDeltaResult.syncCheckpoint.eventSequence == truncatedStartSequence);

    arachno::AppActionRequest saveStaleCheckpoint;
    saveStaleCheckpoint.actionId = "session.checkpoint.save";
    saveStaleCheckpoint.parameters = {{"name", "stale-ui"}};
    const arachno::AppActionResult savedStaleCheckpoint = arachno::executeAppAction(app, saveStaleCheckpoint);
    assert(savedStaleCheckpoint.ok);
    assert(savedStaleCheckpoint.hasSyncCheckpoint);
    assert(!savedStaleCheckpoint.syncCheckpoint.projectFingerprint.empty());

    app.applyEditorCommand("title Fingerprint Shift");
    arachno::AppActionRequest syncStaleCheckpoint;
    syncStaleCheckpoint.actionId = "session.sync";
    syncStaleCheckpoint.parameters = {
        {"checkpoint_name", "stale-ui"},
        {"mode", "delta_only"},
        {"update_checkpoint", "false"}};
    const arachno::AppActionResult staleSyncResult = arachno::executeAppAction(app, syncStaleCheckpoint);
    assert(staleSyncResult.ok);
    assert(staleSyncResult.hasSyncCheckpointCompatibility);
    assert(staleSyncResult.syncCheckpointCompatibility == arachno::SyncCheckpointCompatibility::StaleFingerprint);
    assert(staleSyncResult.syncCheckpointStale);
    assert(staleSyncResult.hasSessionSnapshot);
    assert(staleSyncResult.events.empty());
    assert(staleSyncResult.hasEventCursor);
    assert(staleSyncResult.eventCursor.includesSnapshot);
    assert(staleSyncResult.eventCursor.recommendedSince == staleSyncResult.lastEventSequence);
    assert(staleSyncResult.hasSuggestedSyncCheckpoint);
    assert(staleSyncResult.suggestedSyncCheckpointSafeToCommit);
    assert(staleSyncResult.suggestedSyncCheckpoint.eventSequence == staleSyncResult.lastEventSequence);
    assert(staleSyncResult.hasSyncCheckpointUpdateStatus);
    assert(staleSyncResult.syncCheckpointUpdateStatus == arachno::SyncCheckpointUpdateStatus::SkippedUpdateDisabled);
    const std::string serializedStaleSync = arachno::serializeApplicationActionResult(staleSyncResult);
    assert(serializedStaleSync.find("\"sync_checkpoint_stale\"") != std::string::npos);
    assert(serializedStaleSync.find("\"sync_checkpoint_update\"") != std::string::npos);
    assert(serializedStaleSync.find("\"suggested_sync_checkpoint\"") != std::string::npos);

    arachno::AppActionRequest syncStaleError = syncStaleCheckpoint;
    syncStaleError.parameters = {
        {"checkpoint_name", "stale-ui"},
        {"mode", "delta_only"},
        {"stale_policy", "error"}};
    const arachno::AppActionResult staleSyncError = arachno::executeAppAction(app, syncStaleError);
    assert(!staleSyncError.ok);
    assert(staleSyncError.hasSyncCheckpointCompatibility);
    assert(staleSyncError.syncCheckpointCompatibility == arachno::SyncCheckpointCompatibility::StaleFingerprint);
    assert(staleSyncError.hasSyncCheckpointUpdateStatus);
    assert(staleSyncError.syncCheckpointUpdateStatus == arachno::SyncCheckpointUpdateStatus::SkippedStalePolicy);
    assert(staleSyncError.error.find("stale") != std::string::npos);

    arachno::AppActionRequest syncStaleIgnore = syncStaleCheckpoint;
    syncStaleIgnore.parameters = {
        {"checkpoint_name", "stale-ui"},
        {"mode", "delta_only"},
        {"stale_policy", "ignore"},
        {"max_events", "1"}};
    const arachno::AppActionResult staleSyncIgnore = arachno::executeAppAction(app, syncStaleIgnore);
    assert(staleSyncIgnore.ok);
    assert(staleSyncIgnore.hasSyncCheckpointCompatibility);
    assert(staleSyncIgnore.syncCheckpointCompatibility == arachno::SyncCheckpointCompatibility::StaleFingerprint);
    assert(staleSyncIgnore.hasEvents);
    assert(!staleSyncIgnore.events.empty());
    assert(staleSyncIgnore.hasEventCursor);
    assert(staleSyncIgnore.eventCursor.hasReturnedRange);
    assert(staleSyncIgnore.eventCursor.recommendedSince == staleSyncIgnore.events.back().sequence);
    assert(staleSyncIgnore.hasSuggestedSyncCheckpoint);
    assert(staleSyncIgnore.suggestedSyncCheckpointSafeToCommit);
    assert(staleSyncIgnore.suggestedSyncCheckpoint.eventSequence == staleSyncIgnore.lastEventSequence);
    assert(staleSyncIgnore.hasSyncCheckpointUpdateStatus);
    assert(staleSyncIgnore.syncCheckpointUpdateStatus == arachno::SyncCheckpointUpdateStatus::Advanced);

    const arachno::AppActionResult listedCheckpoints = arachno::executeAppAction(app, {"session.checkpoint.list"});
    assert(listedCheckpoints.ok);
    assert(listedCheckpoints.hasSyncCheckpoints);
    assert(listedCheckpoints.syncCheckpoints.size() >= 3);
    assert(std::any_of(
        listedCheckpoints.syncCheckpoints.begin(),
        listedCheckpoints.syncCheckpoints.end(),
        [](const arachno::SyncCheckpoint& checkpoint) {
            return checkpoint.name == "main-ui";
        }));
    const std::string serializedCheckpointList = arachno::serializeApplicationActionResult(listedCheckpoints);
    assert(serializedCheckpointList.find("\"sync_checkpoints\"") != std::string::npos);

    arachno::AppActionRequest loadCheckpoint;
    loadCheckpoint.actionId = "session.checkpoint.load";
    loadCheckpoint.parameters = {{"name", "main-ui"}};
    const arachno::AppActionResult loadedCheckpoint = arachno::executeAppAction(app, loadCheckpoint);
    assert(loadedCheckpoint.ok);
    assert(loadedCheckpoint.hasSyncCheckpoint);
    assert(loadedCheckpoint.hasSyncCheckpointCompatibility);
    assert(loadedCheckpoint.syncCheckpointCompatibility == arachno::SyncCheckpointCompatibility::StaleFingerprint);
    assert(loadedCheckpoint.syncCheckpoint.name == "main-ui");
    if (syncedFallback.eventDeltaSummary.truncated) {
        assert(loadedCheckpoint.syncCheckpoint.eventSequence == syncedFallback.lastEventSequence);
    }
    const std::string serializedCheckpoint = arachno::serializeApplicationActionResult(loadedCheckpoint);
    assert(serializedCheckpoint.find("\"sync_checkpoint\"") != std::string::npos);
    assert(serializedCheckpoint.find("\"sync_checkpoint_compatibility\"") != std::string::npos);
    assert(serializedCheckpoint.find("\"name\":\"main-ui\"") != std::string::npos);

    arachno::AppActionRequest clearCheckpoint;
    clearCheckpoint.actionId = "session.checkpoint.clear";
    clearCheckpoint.parameters = {{"name", "main-ui"}};
    const arachno::AppActionResult clearedCheckpoint = arachno::executeAppAction(app, clearCheckpoint);
    assert(clearedCheckpoint.ok);
    arachno::AppActionRequest clearBootstrapCheckpoint;
    clearBootstrapCheckpoint.actionId = "session.checkpoint.clear";
    clearBootstrapCheckpoint.parameters = {{"name", "bootstrap-ui"}};
    const arachno::AppActionResult clearedBootstrapCheckpoint =
        arachno::executeAppAction(app, clearBootstrapCheckpoint);
    assert(clearedBootstrapCheckpoint.ok);
    arachno::AppActionRequest clearStaleCheckpoint;
    clearStaleCheckpoint.actionId = "session.checkpoint.clear";
    clearStaleCheckpoint.parameters = {{"name", "stale-ui"}};
    const arachno::AppActionResult clearedStaleCheckpoint =
        arachno::executeAppAction(app, clearStaleCheckpoint);
    assert(clearedStaleCheckpoint.ok);
    arachno::AppActionRequest clearCasCheckpoint;
    clearCasCheckpoint.actionId = "session.checkpoint.clear";
    clearCasCheckpoint.parameters = {{"name", "cas-ui"}};
    const arachno::AppActionResult clearedCasCheckpoint =
        arachno::executeAppAction(app, clearCasCheckpoint);
    assert(clearedCasCheckpoint.ok);
    arachno::AppActionRequest clearTruncatedCheckpoint;
    clearTruncatedCheckpoint.actionId = "session.checkpoint.clear";
    clearTruncatedCheckpoint.parameters = {{"name", "truncated-ui"}};
    const arachno::AppActionResult clearedTruncatedCheckpoint =
        arachno::executeAppAction(app, clearTruncatedCheckpoint);
    assert(clearedTruncatedCheckpoint.ok);
    const arachno::AppActionResult missingCheckpoint = arachno::executeAppAction(app, loadCheckpoint);
    assert(!missingCheckpoint.ok);
    assert(missingCheckpoint.hasSyncCheckpointCompatibility);
    assert(missingCheckpoint.syncCheckpointCompatibility == arachno::SyncCheckpointCompatibility::Missing);
    assert(missingCheckpoint.error.find("not found") != std::string::npos);

    const arachno::AppActionRequest recoverySave {"recovery.save", recoveryPath.string()};
    const arachno::AppActionResult recoverySaved = arachno::executeAppAction(app, recoverySave);
    assert(recoverySaved.ok);
    assert(std::filesystem::exists(recoveryPath));

    app.applyEditorCommand("title Needs Recovery Decision");
    const arachno::AppActionRequest recoveryRestoreWaiting {"recovery.restore", recoveryPath.string()};
    const arachno::AppActionResult recoveryWaiting =
        arachno::executeAppAction(app, recoveryRestoreWaiting);
    assert(!recoveryWaiting.ok);
    assert(recoveryWaiting.requiresUnsavedDecision);

    arachno::AppActionRequest recoveryRestore = recoveryRestoreWaiting;
    recoveryRestore.unsavedChoice = arachno::UnsavedChangesChoice::Discard;
    const arachno::AppActionResult restored = arachno::executeAppAction(app, recoveryRestore);
    assert(restored.ok);
    assert(app.dirty());

    std::filesystem::remove(mixdownPath);
    std::filesystem::remove(midiPath);
    std::filesystem::remove_all(stemsPath);

    arachno::AppActionRequest exportMixdown;
    exportMixdown.actionId = "export.mixdown";
    exportMixdown.path = mixdownPath.string();
    const arachno::AppActionResult mixdownResult = arachno::executeAppAction(app, exportMixdown);
    assert(mixdownResult.ok);
    assert(mixdownResult.hasExportResult);
    assert(mixdownResult.exportResult.ok);
    assert(mixdownResult.exportResult.target == arachno::ExportTarget::Mixdown);
    assert(!mixdownResult.exportResult.files.empty());
    assert(std::filesystem::exists(mixdownPath));

    arachno::AppActionRequest exportStems;
    exportStems.actionId = "export.stems";
    exportStems.path = stemsPath.string();
    exportStems.parameters = {{"format", "wav"}};
    const arachno::AppActionResult stemsResult = arachno::executeAppAction(app, exportStems);
    assert(stemsResult.ok);
    assert(stemsResult.hasExportResult);
    assert(stemsResult.exportResult.ok);
    assert(stemsResult.exportResult.target == arachno::ExportTarget::Stems);
    assert(stemsResult.exportResult.files.size() == app.song().tracks.size());
    assert(std::filesystem::exists(stemsResult.exportResult.files.front().path));

    arachno::AppActionRequest exportMidi;
    exportMidi.actionId = "export.midi";
    exportMidi.path = midiPath.string();
    exportMidi.parameters = {{"ticks_per_quarter", "960"}};
    const arachno::AppActionResult midiResult = arachno::executeAppAction(app, exportMidi);
    assert(midiResult.ok);
    assert(midiResult.hasExportResult);
    assert(midiResult.exportResult.ok);
    assert(midiResult.exportResult.target == arachno::ExportTarget::Midi);
    assert(std::filesystem::exists(midiPath));
    const std::string serializedExport = arachno::serializeApplicationActionResult(midiResult);
    assert(serializedExport.find("\"export_result\"") != std::string::npos);

    arachno::AppActionRequest badExportMidi;
    badExportMidi.actionId = "export.midi";
    badExportMidi.path = midiPath.string();
    badExportMidi.parameters = {{"ticks_per_quarter", "0"}};
    const arachno::AppActionResult badMidi = arachno::executeAppAction(app, badExportMidi);
    assert(!badMidi.ok);
    assert(badMidi.error.find("positive integer") != std::string::npos);

    arachno::AppActionRequest importMidi;
    importMidi.actionId = "import.midi";
    importMidi.path = midiImportPath.string();
    importMidi.parameters = {
        {"rows_per_beat", "4"},
        {"pattern_rows", "64"},
        {"split_by_track", "true"},
        {"split_by_program", "true"},
        {"preserve_tempo_map", "true"}};
    const arachno::AppActionResult importedMidi = arachno::executeAppAction(app, importMidi);
    assert(importedMidi.ok);
    assert(importedMidi.projectChanged);
    assert(importedMidi.hasMidiImportReport);
    assert(importedMidi.midiImportReport.importedNoteCount > 0);
    assert(!importedMidi.midiImportReport.trackMappings.empty());
    assert(app.dirty());
    assert(!app.song().patterns.empty());
    assert(!app.song().order.empty());
    assert(!app.song().instruments.empty());
    const std::string serializedImportMidi = arachno::serializeApplicationActionResult(importedMidi);
    assert(serializedImportMidi.find("\"midi_import_report\"") != std::string::npos);

    arachno::AppActionRequest importPatch;
    importPatch.actionId = "script.import";
    importPatch.path = scriptPatchPath.string();
    importPatch.parameters = {{"name_override", "Imported Script Bass"}};
    const arachno::AppActionResult patchImportResult = arachno::executeAppAction(app, importPatch);
    assert(patchImportResult.ok);
    assert(patchImportResult.hasScriptImportResult);
    assert(patchImportResult.scriptImportResult.ok);
    assert(patchImportResult.scriptImportResult.type == arachno::ScriptArtifactType::Patch);
    assert(patchImportResult.scriptImportResult.importedInstrument >= 0);
    assert(app.song().instruments.back().patch.name == "Imported Script Bass");

    arachno::AppActionRequest importCommands;
    importCommands.actionId = "script.import";
    importCommands.path = scriptCommandsPath.string();
    const arachno::AppActionResult commandsImportResult = arachno::executeAppAction(app, importCommands);
    assert(commandsImportResult.ok);
    assert(commandsImportResult.hasScriptImportResult);
    assert(commandsImportResult.scriptImportResult.type == arachno::ScriptArtifactType::CommandFile);
    assert(commandsImportResult.scriptImportResult.appliedCommandCount == 2);
    const std::string serializedScript = arachno::serializeApplicationActionResult(commandsImportResult);
    assert(serializedScript.find("\"script_import_result\"") != std::string::npos);

    const std::string rendered = arachno::renderApplicationActionPalette(
        arachno::buildApplicationActionPalette(app, "recovery"));
    assert(rendered.find("Application actions") != std::string::npos);
    assert(rendered.find("recovery.restore") != std::string::npos);
    assert(rendered.find("params=1") != std::string::npos);

    const std::string schemaText = arachno::renderAppActionSchema(noteSchema);
    assert(schemaText.find("Action schema") != std::string::npos);
    assert(schemaText.find("type=note") != std::string::npos);

    std::filesystem::remove(projectPath);
    std::filesystem::remove(savePath);
    std::filesystem::remove(recoveryPath);
    std::filesystem::remove(mixdownPath);
    std::filesystem::remove(midiPath);
    std::filesystem::remove(midiImportPath);
    std::filesystem::remove_all(stemsPath);
    std::filesystem::remove(scriptPatchPath);
    std::filesystem::remove(scriptCommandsPath);
}

void testApplicationTaskTracking() {
    arachno::AppTaskManager tasks;
    const arachno::AppTaskId exportTask = tasks.startTask(
        arachno::AppTaskKind::Export,
        "Export stems",
        4,
        "/tmp/stems");
    assert(tasks.hasActiveTasks());
    assert(tasks.activeTaskCount() == 1);
    assert(std::string(arachno::appTaskKindName(arachno::AppTaskKind::Export)) == "export");
    assert(std::string(arachno::appTaskKindName(arachno::AppTaskKind::Audio)) == "audio");
    assert(std::string(arachno::appTaskStateName(arachno::AppTaskState::Running)) == "running");

    assert(tasks.updateTask(exportTask, 2, "Rendered two stems"));
    const arachno::AppTaskSnapshot* running = tasks.findTask(exportTask);
    assert(running != nullptr);
    assert(running->progress.current == 2);
    assert(running->progress.total == 4);
    assert(running->progress.fraction == 0.5);
    assert(running->progress.label == "Rendered two stems");

    assert(tasks.addTaskOutputFile(exportTask, "/tmp/stems/1_bass.wav"));
    assert(tasks.finishTask(exportTask, "Exported stems"));
    const arachno::AppTaskSnapshot* finished = tasks.findTask(exportTask);
    assert(finished != nullptr);
    assert(finished->state == arachno::AppTaskState::Succeeded);
    assert(finished->progress.fraction == 1.0);
    assert(finished->outputFiles.size() == 1);
    assert(!tasks.hasActiveTasks());
    assert(tasks.clearFinishedTasks());
    assert(tasks.tasks().empty());

    const arachno::AppTaskId failedTask = tasks.startTask(arachno::AppTaskKind::Script, "Run script");
    assert(tasks.failTask(failedTask, "script failed"));
    const arachno::AppTaskSnapshot* failed = tasks.findTask(failedTask);
    assert(failed != nullptr);
    assert(failed->state == arachno::AppTaskState::Failed);
    assert(failed->error == "script failed");

    const arachno::AppTaskId canceledTask = tasks.startTask(arachno::AppTaskKind::PluginScan, "Scan plugins");
    assert(tasks.cancelTask(canceledTask));
    const arachno::AppTaskSnapshot* canceled = tasks.findTask(canceledTask);
    assert(canceled != nullptr);
    assert(canceled->state == arachno::AppTaskState::Canceled);

    arachno::ApplicationSession app(arachno::makeDemoSong());
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "arachno-task-export.wav";
    const arachno::ExportResult exported = app.exportProjectWithTask(arachno::mixdownExportRequest(path.string()));
    assert(exported.ok);
    const arachno::AppSessionSnapshot snapshot = app.snapshot();
    assert(!snapshot.hasActiveTasks);
    assert(snapshot.activeTaskCount == 0);
    assert(!snapshot.tasks.empty());
    assert(snapshot.tasks.back().state == arachno::AppTaskState::Succeeded);
    assert(snapshot.tasks.back().outputFiles.size() == 1);
    assert(snapshot.tasks.back().outputFiles.front() == path.string());
    assert(snapshot.tasks.back().progress.fraction == 1.0);

    std::filesystem::remove(path);

    const std::filesystem::path stemPath = std::filesystem::temp_directory_path() / "arachno-task-stems";
    const arachno::ExportResult exportedStems = app.exportProjectWithTask(arachno::stemExportRequest(stemPath.string()));
    assert(exportedStems.ok);
    const arachno::AppSessionSnapshot stemSnapshot = app.snapshot();
    assert(stemSnapshot.tasks.back().state == arachno::AppTaskState::Succeeded);
    assert(stemSnapshot.tasks.back().progress.current == static_cast<int>(app.song().tracks.size()));
    assert(stemSnapshot.tasks.back().outputFiles.size() == app.song().tracks.size());
    const std::vector<arachno::AppEvent> taskEvents = app.eventsSince(0);
    assert(std::any_of(
        taskEvents.begin(),
        taskEvents.end(),
        [](const arachno::AppEvent& event) {
            return event.type == arachno::AppEventType::TaskFinished && event.taskId > 0;
        }));

    std::filesystem::remove_all(stemPath);

    arachno::AppAsyncTaskRunner runner;
    const arachno::AppTaskId renderTask = runner.startTask(
        arachno::AppTaskKind::Render,
        "Render preview",
        3,
        "preview",
        [](arachno::AppAsyncJobContext& context) {
            context.update(1, "Rendering");
            context.addOutputFile("/tmp/arachno-preview.wav");
            context.update(3, "Rendered");
            context.finish("Rendered preview");
        });
    assert(runner.wait(renderTask));
    const arachno::AppTaskSnapshot renderSnapshot = runner.taskSnapshot(renderTask);
    assert(renderSnapshot.state == arachno::AppTaskState::Succeeded);
    assert(renderSnapshot.progress.fraction == 1.0);
    assert(renderSnapshot.outputFiles.size() == 1);
    assert(!runner.hasActiveTasks());

    std::atomic<bool> cancelTaskStarted {false};
    const arachno::AppTaskId cancelTask = runner.startTask(
        arachno::AppTaskKind::Script,
        "Run script",
        1,
        "script",
        [&cancelTaskStarted](arachno::AppAsyncJobContext& context) {
            cancelTaskStarted.store(true);
            while (!context.cancellationRequested()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            context.cancel("Stopped by user");
        });
    while (!cancelTaskStarted.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(runner.requestCancel(cancelTask));
    assert(runner.wait(cancelTask));
    const arachno::AppTaskSnapshot canceledSnapshot = runner.taskSnapshot(cancelTask);
    assert(canceledSnapshot.state == arachno::AppTaskState::Canceled);
    assert(canceledSnapshot.message == "Stopped by user");
}

void testProjectRoundTrip() {
    const arachno::Song original = arachno::makeDemoSong();
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "arachno-roundtrip.arachno";
    arachno::saveProject(original, path.string());
    const arachno::Song loaded = arachno::loadProject(path.string());

    assert(loaded.title == original.title);
    assert(loaded.author == original.author);
    assert(loaded.description == original.description);
    assert(loaded.notes == original.notes);
    assert(loaded.tracks.size() == original.tracks.size());
    assert(loaded.instruments.size() == original.instruments.size());
    assert(loaded.patterns.size() == original.patterns.size());
    assert(loaded.order == original.order);
    assert(loaded.instruments[1].patch.pitchEnvelopeSemitones == original.instruments[1].patch.pitchEnvelopeSemitones);
    assert(loaded.instruments[2].patch.highPass == original.instruments[2].patch.highPass);
    assert(loaded.instruments[3].patch.ringMod == original.instruments[3].patch.ringMod);
    assert(loaded.instruments[4].patch.unisonVoices == original.instruments[4].patch.unisonVoices);
    assert(loaded.instruments[5].patch.stereoSpread == original.instruments[5].patch.stereoSpread);
    assert(loaded.instruments[1].patch.click == original.instruments[1].patch.click);
    assert(loaded.instruments[1].patch.transientShape == original.instruments[1].patch.transientShape);
    assert(loaded.instruments[1].patch.transientPitchSemitones == original.instruments[1].patch.transientPitchSemitones);
    assert(loaded.instruments[1].patch.transientBurstCount == original.instruments[1].patch.transientBurstCount);
    assert(loaded.instruments[1].patch.transientTone == original.instruments[1].patch.transientTone);
    assert(loaded.instruments[2].patch.noiseTone == original.instruments[2].patch.noiseTone);
    assert(loaded.instruments[0].patch.pulseWidth == original.instruments[0].patch.pulseWidth);
    assert(loaded.instruments[3].patch.fmAmount == original.instruments[3].patch.fmAmount);
    assert(loaded.instruments[4].patch.chorusMix == original.instruments[4].patch.chorusMix);
    assert(loaded.instruments[4].patch.chorusFeedback == original.instruments[4].patch.chorusFeedback);
    assert(loaded.instruments[4].patch.delayMix == original.instruments[4].patch.delayMix);
    assert(loaded.instruments[4].patch.reverbMix == original.instruments[4].patch.reverbMix);
    assert(loaded.instruments[4].patch.delayStereo == original.instruments[4].patch.delayStereo);
    assert(loaded.instruments[4].patch.reverbDiffusion == original.instruments[4].patch.reverbDiffusion);
    assert(loaded.instruments[4].patch.tapeColor == original.instruments[4].patch.tapeColor);
    assert(loaded.instruments[4].patch.analogWarmth == original.instruments[4].patch.analogWarmth);
    assert(loaded.instruments[4].patch.phaseScatter == original.instruments[4].patch.phaseScatter);
    assert(loaded.instruments[4].patch.delayDiffusion == original.instruments[4].patch.delayDiffusion);
    assert(loaded.instruments[4].patch.outputGlue == original.instruments[4].patch.outputGlue);
    assert(loaded.instruments[4].patch.vintageDrift == original.instruments[4].patch.vintageDrift);
    assert(loaded.instruments[4].patch.wowFlutter == original.instruments[4].patch.wowFlutter);
    assert(loaded.instruments[0].patch.portamentoTime == original.instruments[0].patch.portamentoTime);
    assert(loaded.instruments[0].patch.portamentoLegato == original.instruments[0].patch.portamentoLegato);
    assert(loaded.instruments[0].patch.monoMode == original.instruments[0].patch.monoMode);
    assert(loaded.instruments[1].patch.fmDecay == original.instruments[1].patch.fmDecay);
    assert(loaded.patterns.front().step(0, 0).note.has_value());
    assert(loaded.patterns.front().step(0, 0).note->midi == 36);
    std::filesystem::remove(path);
}

void testMetadataCommands() {
    arachno::Song song = arachno::makeDemoSong();
    arachno::PatternEditorSession editor(song);

    editor.applyCommand("title Midnight System");
    editor.applyCommand("author Ada Composer");
    editor.applyCommand("description A generative tracker sketch");
    editor.applyCommand("notes Bring lead down an octave after the bridge");
    assert(song.title == "Midnight System");
    assert(song.author == "Ada Composer");
    assert(song.description == "A generative tracker sketch");
    assert(song.notes == "Bring lead down an octave after the bridge");

    const std::filesystem::path path = std::filesystem::temp_directory_path() / "arachno-metadata.arachno";
    arachno::saveProject(song, path.string());
    const arachno::Song loaded = arachno::loadProject(path.string());
    assert(loaded.title == song.title);
    assert(loaded.author == song.author);
    assert(loaded.description == song.description);
    assert(loaded.notes == song.notes);
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

    const arachno::EditorCommandResult moveResult = editor.tryApplyCommand("move 0 0");
    assert(moveResult.ok);
    assert(!moveResult.projectChanged);
    assert(moveResult.editorStateChanged);

    const arachno::EditorCommandResult noteResult = editor.tryApplyCommand("note D4 0.5");
    assert(noteResult.ok);
    assert(noteResult.projectChanged);
    assert(song.patterns.front().step(0, 0).note.has_value());

    const arachno::EditorCommandResult errorResult = editor.tryApplyCommand("gate -1");
    assert(!errorResult.ok);
    assert(errorResult.error.find("gate") != std::string::npos);
    assert(song.patterns.front().step(0, 0).gate > 0.0);
}

void testSelectionClipboardAndUndo() {
    arachno::Song song = arachno::makeDemoSong();
    arachno::PatternEditorSession editor(song);

    editor.applyCommand("move 3 2");
    editor.applyCommand("note C5 0.7");
    assert(song.patterns.front().step(3, 2).note.has_value());
    assert(editor.canUndo());

    editor.applyCommand("undo");
    assert(!song.patterns.front().step(3, 2).note.has_value());
    assert(editor.canRedo());

    editor.applyCommand("redo");
    assert(song.patterns.front().step(3, 2).note.has_value());

    editor.applyCommand("select 0 0 2 2");
    editor.applyCommand("copy");
    editor.applyCommand("paste 10 0");
    assert(song.patterns.front().step(10, 0).note.has_value());
    assert(song.patterns.front().step(10, 0).note->midi == song.patterns.front().step(0, 0).note->midi);
    assert(song.patterns.front().step(10, 1).note.has_value());

    editor.applyCommand("cut");
    assert(!song.patterns.front().step(0, 0).note.has_value());
    assert(!song.patterns.front().step(0, 1).note.has_value());
    editor.applyCommand("undo");
    assert(song.patterns.front().step(0, 0).note.has_value());
    assert(song.patterns.front().step(0, 1).note.has_value());

    editor.applyCommand("clear-selection");
    assert(!song.patterns.front().step(0, 0).note.has_value());
}

void testEditorActionRegistry() {
    const arachno::EditorAction* undo = arachno::findEditorAction("history.undo");
    assert(undo != nullptr);
    assert(undo->label == "Undo");
    assert(undo->defaultShortcut == "Ctrl+Z");
    assert(undo->mutatesProject);

    const arachno::EditorAction* copy = arachno::findEditorAction("selection.copy");
    assert(copy != nullptr);
    assert(copy->category == "Selection");
    assert(!copy->mutatesProject);

    const arachno::EditorAction* fillScale = arachno::findEditorAction("generate.scale");
    assert(fillScale != nullptr);
    assert(fillScale->mutatesProject);
    assert(fillScale->command.find("fill-scale") != std::string::npos);

    const std::string table = arachno::renderEditorActionTable();
    assert(table.find("Editor actions") != std::string::npos);
    assert(table.find("Pattern") != std::string::npos);
    assert(table.find("history.undo") != std::string::npos);

    const std::vector<arachno::EditorAction> synthActions = arachno::searchEditorActions("synth");
    assert(!synthActions.empty());
    assert(arachno::findEditorAction("missing.action") == nullptr);

    arachno::Song song = arachno::makeDemoSong();
    arachno::PatternEditorSession editor(song);
    assert(!arachno::isEditorActionEnabled(editor, "history.undo"));
    assert(!arachno::isEditorActionEnabled(editor, "history.redo"));
    assert(!arachno::isEditorActionEnabled(editor, "selection.paste"));
    editor.applyCommand("copy");
    assert(arachno::isEditorActionEnabled(editor, "selection.paste"));
    editor.applyCommand("note C5 0.7");
    assert(arachno::isEditorActionEnabled(editor, "history.undo"));
}

void testEditorShortcutMap() {
    assert(arachno::normalizeShortcut("shift + ctrl + z") == "Ctrl+Shift+Z");
    assert(arachno::normalizeShortcut("control+c") == "Ctrl+C");

    const std::vector<arachno::ShortcutBinding> defaults = arachno::defaultEditorShortcuts();
    assert(!defaults.empty());
    assert(arachno::validateEditorShortcuts(defaults).empty());

    const arachno::EditorAction* undo = arachno::findEditorActionForShortcut(defaults, "ctrl+z");
    assert(undo != nullptr);
    assert(undo->id == "history.undo");

    const std::vector<const arachno::EditorAction*> redo = arachno::findEditorActionsForShortcut(defaults, "CTRL + SHIFT + Z");
    assert(redo.size() == 1);
    assert(redo.front()->id == "history.redo");

    std::vector<arachno::ShortcutBinding> conflicting = defaults;
    conflicting.push_back({"Ctrl+Z", "selection.copy"});
    const std::vector<arachno::ShortcutConflict> conflicts = arachno::validateEditorShortcuts(conflicting);
    assert(conflicts.size() == 1);
    assert(conflicts.front().shortcut == "Ctrl+Z");

    const std::string table = arachno::renderEditorShortcutTable(defaults);
    assert(table.find("Editor shortcuts") != std::string::npos);
    assert(table.find("history.undo") != std::string::npos);
}

void testEditorCommandPalette() {
    arachno::Song song = arachno::makeDemoSong();
    arachno::PatternEditorSession editor(song);

    const std::vector<arachno::CommandPaletteEntry> initial = arachno::buildEditorCommandPalette(editor);
    assert(!initial.empty());
    const auto paste = std::find_if(initial.begin(), initial.end(), [](const arachno::CommandPaletteEntry& entry) {
        return entry.actionId == "selection.paste";
    });
    assert(paste != initial.end());
    assert(!paste->enabled);
    assert(paste->disabledReason == "clipboard is empty");

    editor.applyCommand("copy");
    const std::vector<arachno::CommandPaletteEntry> filtered = arachno::buildEditorCommandPalette(editor, arachno::defaultEditorShortcuts(), "paste");
    assert(filtered.size() == 1);
    assert(filtered.front().actionId == "selection.paste");
    assert(filtered.front().enabled);
    assert(filtered.front().shortcut == "Ctrl+V");

    const std::string table = arachno::renderEditorCommandPalette(filtered);
    assert(table.find("Command palette") != std::string::npos);
    assert(table.find("Paste") != std::string::npos);
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

void testStepProbabilityAndRetrigger() {
    arachno::Song song = arachno::makeDemoSong();
    arachno::PatternEditorSession editor(song);

    editor.applyCommand("move 3 2");
    editor.applyCommand("note C5 0.7");
    editor.applyCommand("probability 0.5");
    editor.applyCommand("retrig 4 0.125 0.7");

    const arachno::PatternStep& step = song.patterns.front().step(3, 2);
    assert(step.probability.has_value());
    assert(step.probability.value() == 0.5);
    assert(step.retriggerCount == 4);
    assert(step.retriggerSpacingRows == 0.125);
    assert(step.retriggerVelocityDecay == 0.7);

    const std::filesystem::path path = std::filesystem::temp_directory_path() / "arachno-retrig.arachno";
    arachno::saveProject(song, path.string());
    const arachno::Song loaded = arachno::loadProject(path.string());
    const arachno::PatternStep& loadedStep = loaded.patterns.front().step(3, 2);
    assert(loadedStep.probability.has_value());
    assert(loadedStep.probability.value() == 0.5);
    assert(loadedStep.retriggerCount == 4);
    assert(loadedStep.retriggerSpacingRows == 0.125);
    assert(loadedStep.retriggerVelocityDecay == 0.7);
    std::filesystem::remove(path);

    editor.applyCommand("probability clear");
    assert(!song.patterns.front().step(3, 2).probability.has_value());
}

void testStepEffects() {
    arachno::Song song = arachno::makeDemoSong();
    arachno::PatternEditorSession editor(song);

    editor.applyCommand("move 0 1");
    editor.applyCommand("fx cutoff 0.42");
    editor.applyCommand("fxp transpose value 2");
    editor.applyCommand("fxp velocity value 0.7");
    const arachno::PatternStep& step = song.patterns.front().step(0, 1);
    assert(step.effects.size() == 3);

    const std::filesystem::path path = std::filesystem::temp_directory_path() / "arachno-step-effects.arachno";
    arachno::saveProject(song, path.string());
    const arachno::Song loaded = arachno::loadProject(path.string());
    const arachno::PatternStep& loadedStep = loaded.patterns.front().step(0, 1);
    assert(loadedStep.effects.size() == 3);
    std::filesystem::remove(path);

    arachno::AudioEngine engine(song.sampleRate);
    const arachno::RenderedAudio rendered = engine.renderSong(song);
    bool hasSignal = false;
    for (float sample : rendered.interleavedStereo) {
        hasSignal = hasSignal || std::abs(sample) > 0.0001f;
    }
    assert(hasSignal);

    editor.applyCommand("fx-clear cutoff");
    assert(song.patterns.front().step(0, 1).effects.size() == 2);
    editor.applyCommand("fx-clear *");
    assert(song.patterns.front().step(0, 1).effects.empty());

    editor.applyCommand("fx delay 0.40");
    editor.applyCommand("fxp delay time 0.22");
    editor.applyCommand("fxp delay feedback 0.66");
    editor.applyCommand("fx reverb 0.35");
    editor.applyCommand("fxp reverb size 0.75");
    editor.applyCommand("fxp reverb damping 0.40");
    editor.applyCommand("fx chorus 0.30");
    editor.applyCommand("fxp chorus rate 0.70");
    editor.applyCommand("fxp chorus depth 0.52");

    arachno::StepSynthesisState state;
    const arachno::PatternStep& fxStep = song.patterns.front().step(0, 1);
    state.note = fxStep.note.value_or(arachno::Note(60, 0.8f));
    state.patch = song.instruments[static_cast<std::size_t>(std::max(0, fxStep.instrument))].patch;
    state.gateRows = fxStep.gate;
    state.pan = 0.0;
    const bool applied = arachno::applyStepSynthesisState(fxStep, state);
    assert(applied);
    assert(state.patch.combMix > 0.15);
    assert(state.patch.combFeedback > 0.5);
    assert(state.patch.chorusEnabled);
    assert(state.patch.chorusMix > 0.1);
    assert(state.patch.chorusDepth > 0.2);
}

void testMidiImport() {
    auto appendVarLen = [](std::vector<std::uint8_t>& data, int value) {
        std::uint32_t buffer = static_cast<std::uint32_t>(std::max(0, value)) & 0x7f;
        while ((value >>= 7) > 0) {
            buffer <<= 8;
            buffer |= static_cast<std::uint32_t>((value & 0x7f) | 0x80);
        }
        while (true) {
            data.push_back(static_cast<std::uint8_t>(buffer & 0xff));
            if (buffer & 0x80) {
                buffer >>= 8;
            } else {
                break;
            }
        }
    };
    auto appendU32 = [](std::vector<std::uint8_t>& data, std::uint32_t value) {
        data.push_back(static_cast<std::uint8_t>((value >> 24) & 0xff));
        data.push_back(static_cast<std::uint8_t>((value >> 16) & 0xff));
        data.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
        data.push_back(static_cast<std::uint8_t>(value & 0xff));
    };
    auto appendU16 = [](std::vector<std::uint8_t>& data, std::uint16_t value) {
        data.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
        data.push_back(static_cast<std::uint8_t>(value & 0xff));
    };

    const std::filesystem::path path = std::filesystem::temp_directory_path() / "arachno-import.mid";
    std::vector<std::uint8_t> track;
    appendVarLen(track, 0);
    track.push_back(0xff);
    track.push_back(0x03);
    track.push_back(0x04);
    track.push_back('L');
    track.push_back('e');
    track.push_back('a');
    track.push_back('d');

    appendVarLen(track, 0);
    track.push_back(0xff);
    track.push_back(0x51);
    track.push_back(0x03);
    track.push_back(0x07);
    track.push_back(0xa1);
    track.push_back(0x20);

    appendVarLen(track, 0);
    track.push_back(0xc0);
    track.push_back(0x15);
    appendVarLen(track, 0);
    track.push_back(0xb0);
    track.push_back(0x4a);
    track.push_back(0x30);
    appendVarLen(track, 0);
    track.push_back(0xb0);
    track.push_back(0x0a);
    track.push_back(0x20);
    appendVarLen(track, 0);
    track.push_back(0x90);
    track.push_back(60);
    track.push_back(100);
    appendVarLen(track, 480);
    track.push_back(0x80);
    track.push_back(60);
    track.push_back(0);
    appendVarLen(track, 0);
    track.push_back(0xff);
    track.push_back(0x2f);
    track.push_back(0x00);

    std::vector<std::uint8_t> midi;
    midi.push_back('M');
    midi.push_back('T');
    midi.push_back('h');
    midi.push_back('d');
    appendU32(midi, 6);
    appendU16(midi, 0);
    appendU16(midi, 1);
    appendU16(midi, 480);
    midi.push_back('M');
    midi.push_back('T');
    midi.push_back('r');
    midi.push_back('k');
    appendU32(midi, static_cast<std::uint32_t>(track.size()));
    midi.insert(midi.end(), track.begin(), track.end());

    {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(midi.data()), static_cast<std::streamsize>(midi.size()));
    }

    const arachno::MidiImportReport report = arachno::importMidiFile(path.string());
    assert(report.importedNoteCount == 1);
    assert(!report.song.patterns.empty());
    assert(report.song.bpm > 119.0 && report.song.bpm < 121.0);
    assert(!report.song.tracks.empty());
    assert(!report.song.instruments.empty());
    assert(report.song.instruments.size() == report.song.tracks.size());
    assert(report.trackMappings.size() == report.song.tracks.size());
    assert(report.trackMappings.front().trackIndex == 0);
    assert(report.trackMappings.front().instrumentIndex == 0);
    assert(report.trackMappings.front().midiSourceTrack == 0);
    assert(report.trackMappings.front().midiChannel == 0);

    // GM preset bank integration: program 0x15 (21) maps to the curated "Accordion"
    // voice, and the bank produces era-authentic parameter signatures.
    assert(report.song.instruments.front().patch.name == "Accordion");
    assert(report.song.instruments.front().patch.vibratoCents >= 7.9);
    assert(report.song.instruments.front().patch.ampEnvelope.sustain >= 0.9);
    const arachno::SynthPatch gmEp = arachno::gmPresetForProgram(4, 60.0);
    assert(gmEp.fmEnabled && gmEp.fmAmount > 0.3);
    const arachno::SynthPatch gmStrings = arachno::gmPresetForProgram(48, 60.0);
    assert(gmStrings.chorusEnabled && gmStrings.chorusEnsemble > 0.4);
    const arachno::SynthPatch gmSynthBass = arachno::gmPresetForProgram(38, 40.0);
    assert(gmSynthBass.portamentoTime > 0.0);

    bool foundNote = false;
    bool foundEffects = false;
    for (const arachno::Pattern& pattern : report.song.patterns) {
        for (int row = 0; row < pattern.rowCount(); ++row) {
            for (int trackIndex = 0; trackIndex < pattern.trackCount(); ++trackIndex) {
                const arachno::PatternStep& step = pattern.step(row, trackIndex);
                if (!step.note.has_value()) {
                    continue;
                }
                foundNote = true;
                foundEffects = foundEffects || !step.effects.empty();
                assert(step.instrument >= 0);
                assert(step.instrument < static_cast<int>(report.song.instruments.size()));
            }
        }
    }
    assert(foundNote);
    assert(foundEffects);

    arachno::AudioEngine engine(report.song.sampleRate);
    const arachno::RenderedAudio rendered = engine.renderSong(report.song);
    bool hasSignal = false;
    for (float sample : rendered.interleavedStereo) {
        hasSignal = hasSignal || std::abs(sample) > 0.0001f;
    }
    assert(hasSignal);

    std::filesystem::remove(path);
}

void testMidiImportTempoMapAndProgramSplit() {
    const std::filesystem::path path("tempo_program_import.mid");

    auto appendU16 = [](std::vector<std::uint8_t>& out, std::uint16_t value) {
        out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
        out.push_back(static_cast<std::uint8_t>(value & 0xff));
    };
    auto appendU32 = [](std::vector<std::uint8_t>& out, std::uint32_t value) {
        out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xff));
        out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xff));
        out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
        out.push_back(static_cast<std::uint8_t>(value & 0xff));
    };
    auto appendVarLen = [](std::vector<std::uint8_t>& out, std::uint32_t value) {
        std::array<std::uint8_t, 5> buffer {};
        int index = 0;
        buffer[index++] = static_cast<std::uint8_t>(value & 0x7f);
        value >>= 7;
        while (value > 0) {
            buffer[index++] = static_cast<std::uint8_t>((value & 0x7f) | 0x80);
            value >>= 7;
        }
        while (index-- > 0) {
            out.push_back(buffer[static_cast<std::size_t>(index)]);
        }
    };

    std::vector<std::uint8_t> track;
    appendVarLen(track, 0);
    track.push_back(0xff);
    track.push_back(0x51);
    track.push_back(0x03);
    track.push_back(0x07);
    track.push_back(0xA1);
    track.push_back(0x20); // 500000us/qn -> 120 BPM

    appendVarLen(track, 0);
    track.push_back(0xc0);
    track.push_back(0x26); // program 38 (synth bass)
    appendVarLen(track, 0);
    track.push_back(0x90);
    track.push_back(36);
    track.push_back(110);
    appendVarLen(track, 480);
    track.push_back(0x80);
    track.push_back(36);
    track.push_back(0);

    appendVarLen(track, 0);
    track.push_back(0xff);
    track.push_back(0x51);
    track.push_back(0x03);
    track.push_back(0x0F);
    track.push_back(0x42);
    track.push_back(0x40); // 1000000us/qn -> 60 BPM

    appendVarLen(track, 0);
    track.push_back(0xc0);
    track.push_back(0x53); // program 83 (lead)
    appendVarLen(track, 480);
    track.push_back(0x90);
    track.push_back(64);
    track.push_back(100);
    appendVarLen(track, 480);
    track.push_back(0x80);
    track.push_back(64);
    track.push_back(0);

    appendVarLen(track, 0);
    track.push_back(0xff);
    track.push_back(0x2f);
    track.push_back(0x00);

    std::vector<std::uint8_t> midi;
    midi.push_back('M');
    midi.push_back('T');
    midi.push_back('h');
    midi.push_back('d');
    appendU32(midi, 6);
    appendU16(midi, 0);
    appendU16(midi, 1);
    appendU16(midi, 480);
    midi.push_back('M');
    midi.push_back('T');
    midi.push_back('r');
    midi.push_back('k');
    appendU32(midi, static_cast<std::uint32_t>(track.size()));
    midi.insert(midi.end(), track.begin(), track.end());

    {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(midi.data()), static_cast<std::streamsize>(midi.size()));
    }

    const arachno::MidiImportReport report = arachno::importMidiFile(path.string());
    assert(report.song.bpm > 119.0 && report.song.bpm < 121.0);
    assert(report.trackMappings.size() >= 2);
    assert(report.song.instruments.size() >= 2);

    bool foundBassProgram = false;
    bool foundLeadProgram = false;
    for (const arachno::MidiImportTrackMapping& mapping : report.trackMappings) {
        foundBassProgram = foundBassProgram || mapping.dominantProgram == 0x26;
        foundLeadProgram = foundLeadProgram || mapping.dominantProgram == 0x53;
    }
    assert(foundBassProgram);
    assert(foundLeadProgram);

    int bassRow = -1;
    int leadRow = -1;
    int rowOffset = 0;
    for (const arachno::Pattern& pattern : report.song.patterns) {
        for (int row = 0; row < pattern.rowCount(); ++row) {
            for (int trackIndex = 0; trackIndex < pattern.trackCount(); ++trackIndex) {
                const arachno::PatternStep& step = pattern.step(row, trackIndex);
                if (!step.note.has_value()) {
                    continue;
                }
                const int absoluteRow = rowOffset + row;
                if (step.note->midi == 36) {
                    bassRow = absoluteRow;
                } else if (step.note->midi == 64) {
                    leadRow = absoluteRow;
                }
            }
        }
        rowOffset += pattern.rowCount();
    }
    assert(bassRow >= 0);
    assert(leadRow >= 0);
    assert(leadRow >= 11); // should land later than simple beat-mapped row 8 due to tempo slowdown.

    std::filesystem::remove(path);
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
    const int initialTracks = static_cast<int>(song.tracks.size());

    editor.applyCommand("new-track Counter");
    assert(song.tracks.size() == static_cast<std::size_t>(initialTracks + 1));
    assert(song.tracks[static_cast<std::size_t>(initialTracks)].name == "Counter");
    assert(song.patterns.front().trackCount() == initialTracks + 1);

    editor.applyCommand("move 0 " + std::to_string(initialTracks));
    editor.applyCommand("note C5 0.8");
    assert(song.patterns.front().step(0, initialTracks).note.has_value());

    editor.applyCommand("duplicate-track " + std::to_string(initialTracks) + " CounterCopy");
    assert(song.tracks.size() == static_cast<std::size_t>(initialTracks + 2));
    assert(song.tracks[static_cast<std::size_t>(initialTracks + 1)].name == "CounterCopy");
    assert(song.patterns.front().trackCount() == initialTracks + 2);
    assert(song.patterns.front().step(0, initialTracks + 1).note.has_value());
    assert(song.patterns.front().step(0, initialTracks + 1).note->midi == 72);

    editor.applyCommand("track-name " + std::to_string(initialTracks + 1) + " Answer");
    assert(song.tracks[static_cast<std::size_t>(initialTracks + 1)].name == "Answer");

    editor.applyCommand("clear-track " + std::to_string(initialTracks + 1));
    assert(!song.patterns.front().step(0, initialTracks + 1).note.has_value());

    editor.applyCommand("resize-pattern 96");
    assert(song.patterns.front().rowCount() == 96);
    assert(song.patterns.front().trackCount() == initialTracks + 2);
}

void testDeletionAndOrderCommands() {
    arachno::Song song = arachno::makeDemoSong();
    arachno::PatternEditorSession editor(song);
    const int initialTracks = static_cast<int>(song.tracks.size());
    const std::string shiftedTrackName = song.tracks[2].name;

    editor.applyCommand("new-pattern Bridge 16");
    editor.applyCommand("new-pattern Outro 16");
    assert(song.patterns.size() == 3);

    editor.applyCommand("set-order 0 1 2 1");
    editor.applyCommand("insert-order 1 2");
    assert((song.order == std::vector<int> {0, 2, 1, 2, 1}));

    editor.applyCommand("remove-order 2");
    assert((song.order == std::vector<int> {0, 2, 2, 1}));

    editor.applyCommand("delete-pattern 1");
    assert(song.patterns.size() == 2);
    assert((song.order == std::vector<int> {0, 1, 1}));

    editor.applyCommand("new-track Counter");
    editor.applyCommand("move 0 " + std::to_string(initialTracks));
    editor.applyCommand("note C5 0.8");
    editor.applyCommand("delete-track 1");
    assert(song.tracks.size() == static_cast<std::size_t>(initialTracks));
    assert(song.patterns.front().trackCount() == initialTracks);
    assert(song.tracks[1].name == shiftedTrackName);
}

void testInstrumentCommands() {
    arachno::Song song = arachno::makeDemoSong();
    arachno::PatternEditorSession editor(song);
    const int initialInstruments = static_cast<int>(song.instruments.size());

    editor.applyCommand("new-instrument Glass");
    assert(song.instruments.size() == static_cast<std::size_t>(initialInstruments + 1));
    assert(song.instruments[static_cast<std::size_t>(initialInstruments)].id == initialInstruments);
    assert(song.instruments[static_cast<std::size_t>(initialInstruments)].patch.name == "Glass");

    editor.applyCommand("instrument-wave " + std::to_string(initialInstruments) + " A sine");
    editor.applyCommand("instrument-wave " + std::to_string(initialInstruments) + " B triangle");
    editor.applyCommand("instrument-param " + std::to_string(initialInstruments) + " cutoff 0.91");
    editor.applyCommand("instrument-param " + std::to_string(initialInstruments) + " vibrato 5");
    editor.applyCommand("instrument-param " + std::to_string(initialInstruments) + " pitch-env 12");
    editor.applyCommand("instrument-param " + std::to_string(initialInstruments) + " ring-mod 0.25");
    editor.applyCommand("instrument-param " + std::to_string(initialInstruments) + " bitcrush 0.2");
    editor.applyCommand("instrument-param " + std::to_string(initialInstruments) + " unison 3");
    editor.applyCommand("instrument-param " + std::to_string(initialInstruments) + " unison-detune 8");
    editor.applyCommand("instrument-param " + std::to_string(initialInstruments) + " spread 0.4");
    editor.applyCommand("instrument-param " + std::to_string(initialInstruments) + " pulse-width 0.38");
    editor.applyCommand("instrument-param " + std::to_string(initialInstruments) + " pwm 0.2");
    editor.applyCommand("instrument-param " + std::to_string(initialInstruments) + " fm 0.3");
    editor.applyCommand("instrument-param " + std::to_string(initialInstruments) + " fm-ratio 3");
    editor.applyCommand("instrument-param " + std::to_string(initialInstruments) + " chorus 0.22");
    editor.applyCommand("instrument-param " + std::to_string(initialInstruments) + " chorus-rate 0.6");
    editor.applyCommand("instrument-param " + std::to_string(initialInstruments) + " chorus-depth 0.5");
    editor.applyCommand("instrument-param " + std::to_string(initialInstruments) + " click 0.12");
    editor.applyCommand("instrument-param " + std::to_string(initialInstruments) + " transient-noise 0.3");
    editor.applyCommand("instrument-param " + std::to_string(initialInstruments) + " noise-tone 0.77");
    editor.applyCommand("instrument-param " + std::to_string(initialInstruments) + " transient-shape 0.35");
    editor.applyCommand("instrument-param " + std::to_string(initialInstruments) + " transient-pitch 11");
    editor.applyCommand("instrument-param " + std::to_string(initialInstruments) + " transient-pitch-decay 0.014");
    editor.applyCommand("instrument-param " + std::to_string(initialInstruments) + " transient-burst-count 3");
    editor.applyCommand("instrument-param " + std::to_string(initialInstruments) + " transient-burst-spacing 0.004");
    editor.applyCommand("instrument-param " + std::to_string(initialInstruments) + " transient-burst-decay 0.62");
    editor.applyCommand("instrument-param " + std::to_string(initialInstruments) + " transient-tone 0.88");
    assert(song.instruments[static_cast<std::size_t>(initialInstruments)].patch.oscillatorA == arachno::Waveform::Sine);
    assert(song.instruments[static_cast<std::size_t>(initialInstruments)].patch.oscillatorB == arachno::Waveform::Triangle);
    assert(song.instruments[static_cast<std::size_t>(initialInstruments)].patch.cutoff == 0.91);
    assert(song.instruments[static_cast<std::size_t>(initialInstruments)].patch.vibratoCents == 5.0);
    assert(song.instruments[static_cast<std::size_t>(initialInstruments)].patch.pitchEnvelopeSemitones == 12.0);
    assert(song.instruments[static_cast<std::size_t>(initialInstruments)].patch.ringMod == 0.25);
    assert(song.instruments[static_cast<std::size_t>(initialInstruments)].patch.bitCrush == 0.2);
    assert(song.instruments[static_cast<std::size_t>(initialInstruments)].patch.unisonVoices == 3);
    assert(song.instruments[static_cast<std::size_t>(initialInstruments)].patch.unisonDetuneCents == 8.0);
    assert(song.instruments[static_cast<std::size_t>(initialInstruments)].patch.stereoSpread == 0.4);
    assert(song.instruments[static_cast<std::size_t>(initialInstruments)].patch.pulseWidth == 0.38);
    assert(song.instruments[static_cast<std::size_t>(initialInstruments)].patch.pwmDepth == 0.2);
    assert(song.instruments[static_cast<std::size_t>(initialInstruments)].patch.fmAmount == 0.3);
    assert(song.instruments[static_cast<std::size_t>(initialInstruments)].patch.fmRatio == 3.0);
    assert(song.instruments[static_cast<std::size_t>(initialInstruments)].patch.chorusMix == 0.22);
    assert(song.instruments[static_cast<std::size_t>(initialInstruments)].patch.chorusRate == 0.6);
    assert(song.instruments[static_cast<std::size_t>(initialInstruments)].patch.chorusDepth == 0.5);
    assert(song.instruments[static_cast<std::size_t>(initialInstruments)].patch.click == 0.12);
    assert(song.instruments[static_cast<std::size_t>(initialInstruments)].patch.transientNoise == 0.3);
    assert(song.instruments[static_cast<std::size_t>(initialInstruments)].patch.noiseTone == 0.77);
    assert(song.instruments[static_cast<std::size_t>(initialInstruments)].patch.transientShape == 0.35);
    assert(song.instruments[static_cast<std::size_t>(initialInstruments)].patch.transientPitchSemitones == 11.0);
    assert(song.instruments[static_cast<std::size_t>(initialInstruments)].patch.transientPitchDecay == 0.014);
    assert(song.instruments[static_cast<std::size_t>(initialInstruments)].patch.transientBurstCount == 3);
    assert(song.instruments[static_cast<std::size_t>(initialInstruments)].patch.transientBurstSpacing == 0.004);
    assert(song.instruments[static_cast<std::size_t>(initialInstruments)].patch.transientBurstDecay == 0.62);
    assert(song.instruments[static_cast<std::size_t>(initialInstruments)].patch.transientTone == 0.88);

    editor.applyCommand("clone-instrument " + std::to_string(initialInstruments) + " GlassCopy");
    assert(song.instruments.size() == static_cast<std::size_t>(initialInstruments + 2));
    assert(song.instruments[static_cast<std::size_t>(initialInstruments + 1)].patch.name == "GlassCopy");
    assert(song.instruments[static_cast<std::size_t>(initialInstruments + 1)].patch.cutoff == 0.91);

    editor.applyCommand("instrument-name " + std::to_string(initialInstruments + 1) + " Air");
    assert(song.instruments[static_cast<std::size_t>(initialInstruments + 1)].patch.name == "Air");

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
    patch.pitchEnvelopeSemitones = 19.0;
    patch.pitchEnvelopeDecay = 0.044;
    patch.unisonVoices = 5;
    patch.unisonDetuneCents = 13.0;
    patch.stereoSpread = 0.62;
    patch.pulseWidth = 0.36;
    patch.pwmDepth = 0.17;
    patch.fmAmount = 0.24;
    patch.fmRatio = 4.0;
    patch.chorusMix = 0.31;
    patch.chorusRate = 0.47;
    patch.chorusDepth = 0.53;
    patch.chorusFeedback = 0.17;
    patch.chorusDelay = 0.49;
    patch.chorusWidth = 0.74;
    patch.chorusEnsemble = 0.46;
    patch.ringMod = 0.33;
    patch.hardSync = 0.27;
    patch.bitCrush = 0.18;
    patch.sampleRateReduction = 0.21;
    patch.delayMix = 0.19;
    patch.delayTime = 0.26;
    patch.delayFeedback = 0.37;
    patch.delayTone = 0.59;
    patch.delayStereo = 0.44;
    patch.delayModDepth = 0.41;
    patch.delayDrive = 0.28;
    patch.delayDucking = 0.24;
    patch.reverbMix = 0.23;
    patch.reverbSize = 0.68;
    patch.reverbDamping = 0.41;
    patch.reverbPreDelay = 0.14;
    patch.reverbDiffusion = 0.62;
    patch.reverbWidth = 0.57;
    patch.reverbShimmer = 0.18;
    patch.reverbModDepth = 0.26;
    patch.highPass = 0.42;
    patch.click = 0.2;
    patch.noiseTone = 0.74;
    patch.transientShape = 0.57;
    patch.transientNoise = 0.4;
    patch.transientPitchSemitones = 15.0;
    patch.transientPitchDecay = 0.013;
    patch.transientBurstCount = 4;
    patch.transientBurstSpacing = 0.0045;
    patch.transientBurstDecay = 0.64;
    patch.transientTone = 0.91;
    patch.transientDecay = 0.018;
    patch.vintageDrift = 0.61;
    patch.wowFlutter = 0.22;
    patch.tapeColor = 0.31;
    patch.airBoost = 0.29;
    patch.lowPunch = 0.38;
    patch.analogWarmth = 0.67;
    patch.voiceSlop = 0.44;
    patch.phaseScatter = 0.52;
    patch.chorusTone = 0.71;
    patch.delayDiffusion = 0.39;
    patch.reverbDecay = 0.83;
    patch.reverbEarlyMix = 0.28;
    patch.consoleCrosstalk = 0.11;
    patch.outputGlue = 0.49;
    patch.ampEnvelope.attack = 0.03;
    patch.portamentoTime = 0.055;
    patch.portamentoLegato = true;
    patch.fmDecay = 0.42;
    patch.velocityToFm = 0.6;
    patch.monoMode = true;
    patch.oscBRatio = 3.25;
    patch.oscCRatio = 5.5;
    patch.oscDRatio = 0.5;
    patch.oscBDecay = 0.17;
    patch.oscCDecay = 0.42;
    patch.oscDDecay = 1.10;
    patch.velocityToDecay = 0.66;
    patch.keyTrackDecay = 0.77;

    const std::filesystem::path path = std::filesystem::temp_directory_path() / "arachno-glass.arachnopatch";
    arachno::savePatch(patch, path.string());
    const arachno::SynthPatch loaded = arachno::loadPatch(path.string());

    assert(loaded.name == "Glass");
    assert(loaded.oscillatorA == arachno::Waveform::Sine);
    assert(loaded.oscillatorB == arachno::Waveform::Triangle);
    assert(loaded.cutoff == 0.91);
    assert(loaded.vibratoCents == 5.0);
    assert(loaded.pitchEnvelopeSemitones == 19.0);
    assert(loaded.pitchEnvelopeDecay == 0.044);
    assert(loaded.unisonVoices == 5);
    assert(loaded.unisonDetuneCents == 13.0);
    assert(loaded.stereoSpread == 0.62);
    assert(loaded.pulseWidth == 0.36);
    assert(loaded.pwmDepth == 0.17);
    assert(loaded.fmAmount == 0.24);
    assert(loaded.fmRatio == 4.0);
    assert(loaded.chorusMix == 0.31);
    assert(loaded.chorusRate == 0.47);
    assert(loaded.chorusDepth == 0.53);
    assert(loaded.chorusFeedback == 0.17);
    assert(loaded.chorusDelay == 0.49);
    assert(loaded.chorusWidth == 0.74);
    assert(loaded.chorusEnsemble == 0.46);
    assert(loaded.ringMod == 0.33);
    assert(loaded.hardSync == 0.27);
    assert(loaded.bitCrush == 0.18);
    assert(loaded.sampleRateReduction == 0.21);
    assert(loaded.delayMix == 0.19);
    assert(loaded.delayTime == 0.26);
    assert(loaded.delayFeedback == 0.37);
    assert(loaded.delayTone == 0.59);
    assert(loaded.delayStereo == 0.44);
    assert(loaded.delayModDepth == 0.41);
    assert(loaded.delayDrive == 0.28);
    assert(loaded.delayDucking == 0.24);
    assert(loaded.reverbMix == 0.23);
    assert(loaded.reverbSize == 0.68);
    assert(loaded.reverbDamping == 0.41);
    assert(loaded.reverbPreDelay == 0.14);
    assert(loaded.reverbDiffusion == 0.62);
    assert(loaded.reverbWidth == 0.57);
    assert(loaded.reverbShimmer == 0.18);
    assert(loaded.reverbModDepth == 0.26);
    assert(loaded.highPass == 0.42);
    assert(loaded.click == 0.2);
    assert(loaded.noiseTone == 0.74);
    assert(loaded.transientShape == 0.57);
    assert(loaded.transientNoise == 0.4);
    assert(loaded.transientPitchSemitones == 15.0);
    assert(loaded.transientPitchDecay == 0.013);
    assert(loaded.transientBurstCount == 4);
    assert(loaded.transientBurstSpacing == 0.0045);
    assert(loaded.transientBurstDecay == 0.64);
    assert(loaded.transientTone == 0.91);
    assert(loaded.transientDecay == 0.018);
    assert(loaded.vintageDrift == 0.61);
    assert(loaded.wowFlutter == 0.22);
    assert(loaded.tapeColor == 0.31);
    assert(loaded.airBoost == 0.29);
    assert(loaded.lowPunch == 0.38);
    assert(loaded.analogWarmth == 0.67);
    assert(loaded.voiceSlop == 0.44);
    assert(loaded.phaseScatter == 0.52);
    assert(loaded.chorusTone == 0.71);
    assert(loaded.delayDiffusion == 0.39);
    assert(loaded.reverbDecay == 0.83);
    assert(loaded.reverbEarlyMix == 0.28);
    assert(loaded.consoleCrosstalk == 0.11);
    assert(loaded.outputGlue == 0.49);
    assert(loaded.ampEnvelope.attack == 0.03);
    assert(loaded.portamentoTime == 0.055);
    assert(loaded.portamentoLegato);
    assert(loaded.fmDecay == 0.42);
    assert(loaded.velocityToFm == 0.6);
    assert(loaded.monoMode);
    assert(loaded.oscBRatio == 3.25);
    assert(loaded.oscCRatio == 5.5);
    assert(loaded.oscDRatio == 0.5);
    assert(loaded.oscBDecay == 0.17);
    assert(loaded.oscCDecay == 0.42);
    assert(loaded.oscDDecay == 1.10);
    assert(loaded.velocityToDecay == 0.66);
    assert(loaded.keyTrackDecay == 0.77);
    std::filesystem::remove(path);
}

void testLayeredTimbreDsp() {
    // Two-layer patch: a slow sine body (osc A) plus a fast-decaying high-ratio
    // layer (osc B). The attack must be measurably brighter than the sustain,
    // and the body must persist after the clang layer dies.
    arachno::Song song = arachno::makeBlankSong();
    arachno::SynthPatch& patch = song.instruments[0].patch;
    patch.oscillatorA = arachno::Waveform::Sine;
    patch.oscillatorB = arachno::Waveform::Sine;
    patch.oscillatorBEnabled = true;
    patch.oscillatorMix = 0.50;
    patch.oscBRatio = 4.0;
    patch.oscBDecay = 0.10;
    patch.oscillatorCEnabled = false;
    patch.oscillatorDEnabled = false;
    patch.noiseEnabled = false;
    patch.subEnabled = false;
    patch.cutoff = 1.0;
    patch.resonance = 0.0;
    patch.filterNonlinearity = 0.0;
    patch.filterDrive = 0.0;
    patch.drive = 0.0;
    patch.analogColor = 0.0;
    patch.analogWarmth = 0.0;
    patch.vintageDrift = 0.0;
    patch.phaseScatter = 0.0;
    patch.voiceSlop = 0.0;
    patch.tapeColor = 0.0;
    patch.airBoost = 0.0;
    patch.lowPunch = 0.0;
    patch.hifiExciter = 0.0;
    patch.outputTransformer = 0.0;
    patch.outputSoftClip = 0.0;
    patch.outputGlue = 0.0;
    patch.toneTilt = 0.0;
    patch.wowFlutter = 0.0;
    patch.consoleCrosstalk = 0.0;
    patch.stereoDepth = 0.0;
    patch.reverbMix = 0.0;
    patch.delayMix = 0.0;
    patch.chorusMix = 0.0;
    patch.gain = 0.8;
    patch.ampEnvelope.attack = 0.001;
    patch.ampEnvelope.decay = 2.0;
    patch.ampEnvelope.sustain = 1.0;
    patch.ampEnvelope.release = 0.10;

    arachno::PatternStep& step = song.patterns[0].step(0, 0);
    step.note = arachno::Note(arachno::noteNameToMidi("A4"), 1.0f);
    step.instrument = 0;
    step.gate = 24.0;

    arachno::AudioEngine engine(song.sampleRate);
    const arachno::RenderedAudio audio = engine.renderSong(song);
    const int rate = audio.sampleRate;
    auto magnitudeAt = [&](const arachno::RenderedAudio& buffer, double startSec, double durationSec, double targetHz) {
        // Goertzel magnitude of the left channel over the window.
        const int start = static_cast<int>(startSec * rate);
        const int count = static_cast<int>(durationSec * rate);
        const double omega = 2.0 * 3.14159265358979323846 * targetHz / rate;
        const double coeff = 2.0 * std::cos(omega);
        double s0 = 0.0;
        double s1 = 0.0;
        double s2 = 0.0;
        for (int i = 0; i < count; ++i) {
            const std::size_t idx = static_cast<std::size_t>(start + i) * 2;
            if (idx + 1 >= buffer.interleavedStereo.size()) {
                break;
            }
            s0 = static_cast<double>(buffer.interleavedStereo[idx]) + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        return std::sqrt(std::max(0.0, s1 * s1 + s2 * s2 - coeff * s1 * s2)) / std::max(1, count);
    };
    const double fundamental = 440.0;
    const double clang = 440.0 * 4.0;
    // Attack window: clang layer present. Sustain window: clang layer dead.
    const double clangEarly = magnitudeAt(audio, 0.02, 0.05, clang);
    const double clangLate = magnitudeAt(audio, 0.60, 0.10, clang);
    const double bodyLate = magnitudeAt(audio, 0.60, 0.10, fundamental);
    assert(clangEarly > clangLate * 4.0);
    assert(bodyLate > clangLate * 4.0);

    // Defaults stay clean: a plain sine with every color field at zero keeps THD < 1%.
    patch.oscBDecay = 0.0;
    patch.oscBRatio = 1.0;
    patch.oscillatorBEnabled = false;
    patch.oscillatorMix = 0.0;
    const arachno::RenderedAudio clean = engine.renderSong(song);
    const double fund = magnitudeAt(clean, 0.30, 0.20, fundamental);
    double harmonicSum = 0.0;
    for (int harmonic = 2; harmonic <= 8; ++harmonic) {
        const double h = magnitudeAt(clean, 0.30, 0.20, fundamental * harmonic);
        harmonicSum += h * h;
    }
    const double thd = std::sqrt(harmonicSum) / std::max(0.000001, fund);
    assert(thd < 0.01);
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
    arachno::Song song = arachno::makeDemoSong();
    const std::string table = arachno::renderPatternTable(song, 0, 0, 4);
    assert(table.find("Pattern 0") != std::string::npos);
    assert(table.find("Bass") != std::string::npos);
    assert(table.find("C2") != std::string::npos);

    arachno::PatternEditorSession editor(song);
    editor.applyCommand("move 0 1");
    editor.applyCommand("select 0 0 2 2");
    const arachno::PatternGrid grid = arachno::buildPatternGrid(
        song,
        0,
        0,
        4,
        &editor.cursor(),
        &editor.selection());
    assert(grid.patternName == song.patterns.front().name());
    assert(grid.rowCount == 4);
    assert(grid.trackCount == static_cast<int>(song.tracks.size()));
    assert(grid.cells.size() == static_cast<std::size_t>(grid.rowCount * grid.trackCount));
    assert(grid.cells[0].trackName == "Bass");
    assert(grid.cells[0].selected);
    assert(grid.cells[1].cursor);
    assert(grid.cells[1].selected);
    assert(grid.cells[0].hasNote);
}

void testEditorViewModel() {
    arachno::Song song = arachno::makeDemoSong();
    arachno::PatternEditorSession editor(song);
    editor.applyCommand("move 0 1");
    editor.applyCommand("select 0 0 2 2");
    editor.applyCommand("param cutoff 0.95");
    editor.applyCommand("probability 0.75");
    editor.applyCommand("retrig 2 0.2 0.6");
    editor.applyCommand("copy");

    const arachno::EditorViewModel model = arachno::buildEditorViewModel(song, editor, 0, 8);
    assert(model.status.title == song.title);
    assert(model.status.activePattern == 0);
    assert(model.status.cursorTrack == 1);
    assert(model.status.cursorTrackName == song.tracks[1].name);
    assert(model.status.selectionRows == 2);
    assert(model.status.canUndo);

    assert(model.patterns.size() == song.patterns.size());
    assert(model.patterns[0].active);
    assert(model.patterns[0].orderUseCount > 0);

    assert(model.order.size() == song.order.size());
    assert(model.order[0].startRow == 0);
    assert(!model.order[0].missing);

    assert(model.tracks.size() == song.tracks.size());
    assert(model.tracks[1].active);
    assert(model.tracks[1].noteCount > 0);
    assert(model.tracks[1].automatedStepCount == 1);

    assert(model.instruments.size() == song.instruments.size());
    assert(model.instruments[0].noteUseCount > 0);
    assert(!model.instruments[0].oscillatorA.empty());

    assert(model.activeStep.row == 0);
    assert(model.activeStep.track == 1);
    assert(model.activeStep.trackName == song.tracks[1].name);
    assert(model.activeStep.hasNote);
    assert(model.activeStep.instrumentName == song.instruments[model.activeStep.instrument].patch.name);
    assert(model.activeStep.automation.size() == 1);
    assert(model.activeStep.automation.front().parameter == "cutoff");
    assert(model.activeStep.hasProbability);
    assert(model.activeStep.probability == 0.75);
    assert(model.activeStep.retriggerCount == 2);
    assert(model.activeStep.retriggerSpacingRows == 0.2);
    assert(model.activeStep.retriggerVelocityDecay == 0.6);

    assert(model.selection.stepCount == 4);
    assert(model.selection.noteCount > 0);
    assert(model.selection.automatedStepCount == 1);
    assert(model.selection.containsCursor);

    assert(model.clipboard.available);
    assert(model.clipboard.rowCount == 2);
    assert(model.clipboard.trackCount == 2);

    assert(model.activeGrid.rowCount == 8);
    assert(model.activeGrid.cells[1].cursor);
    assert(model.activeGrid.cells[1].selected);
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

void testProjectStats() {
    arachno::Song song = arachno::makeDemoSong();
    arachno::PatternEditorSession editor(song);
    editor.applyCommand("move 0 1");
    editor.applyCommand("param cutoff 0.95");

    const std::string stats = arachno::renderProjectStats(song);
    assert(stats.find("Project stats") != std::string::npos);
    assert(stats.find("notes:") != std::string::npos);
    assert(stats.find("automated steps:") != std::string::npos);
    assert(stats.find("Notes by track") != std::string::npos);
    assert(stats.find("Bright Twin") != std::string::npos);
}

void testProjectDiagnostics() {
    arachno::Song song = arachno::makeDemoSong();
    const std::vector<arachno::ProjectDiagnostic> ok = arachno::validateProject(song);
    assert(!arachno::hasErrors(ok));

    song.patterns.front().step(0, 0).instrument = 99;
    const std::vector<arachno::ProjectDiagnostic> broken = arachno::validateProject(song);
    assert(arachno::hasErrors(broken));
    assert(arachno::countDiagnostics(broken, arachno::DiagnosticSeverity::Error) == 1);
    assert(arachno::diagnosticSeverityName(arachno::DiagnosticSeverity::Error) == std::string("error"));
    const auto missingInstrument = std::find_if(
        broken.begin(),
        broken.end(),
        [](const arachno::ProjectDiagnostic& diagnostic) {
            return diagnostic.code == "step.missing_instrument";
        });
    assert(missingInstrument != broken.end());
    assert(missingInstrument->location.pattern == 0);
    assert(missingInstrument->location.row == 0);
    assert(missingInstrument->location.track == 0);
    assert(arachno::formatDiagnosticLocation(missingInstrument->location) == "pattern 0 row 0 track 0");
    const std::string formatted = arachno::formatDiagnostics(broken);
    assert(formatted.find("[step.missing_instrument]") != std::string::npos);
    assert(formatted.find("missing instrument") != std::string::npos);

    song.patterns.front().step(0, 0).automation["not_a_synth_parameter"] = 0.5;
    const std::vector<arachno::ProjectDiagnostic> automatedBroken = arachno::validateProject(song);
    assert(std::any_of(
        automatedBroken.begin(),
        automatedBroken.end(),
        [](const arachno::ProjectDiagnostic& diagnostic) {
            return diagnostic.code == "step.unknown_automation";
        }));
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

void testGuiShellContract() {
    arachno::ApplicationSession session;
    arachno::GuiShellOptions options;
    options.checkpointName = "gui-contract";
    options.createCheckpointIfMissing = true;
    options.maxEvents = 4;
    options.snapshotGridStartRow = 2;
    options.snapshotGridRowCount = 10;

    std::istringstream input(
        "status\n"
        "sync mode=delta_with_fallback\n"
        "schema session.checkpoint.advance\n"
        "do session.checkpoint.list\n"
        "do session.checkpoint.advance name=gui-contract event_sequence=1\n"
        "quit\n");
    std::ostringstream output;

    const int code = arachno::runGuiShell(session, input, output, options);
    const std::string text = output.str();
    assert(code == 0);
    assert(text.find("ArachnoTracker GUI Shell") != std::string::npos);
    assert(text.find("Action result: session.sync") != std::string::npos);
    assert(text.find("Action result: session.snapshot") != std::string::npos);
    assert(text.find("session.checkpoint.advance") != std::string::npos);
    assert(text.find("Action result: session.checkpoint.advance") != std::string::npos);
    assert(text.find("Exiting GUI shell.") != std::string::npos);
}

int main() {
    auto trace = [](const char* name) { std::fprintf(stderr, "[test] %s\n", name); };
    trace("testNotes();");
    testNotes();
    trace("testTrackerModel();");
    testTrackerModel();
    trace("testRenderDemoSong();");
    testRenderDemoSong();
    trace("testWavExport();");
    testWavExport();
    trace("testTrackStemRendering();");
    testTrackStemRendering();
    trace("testRealtimePlaybackContract();");
    testRealtimePlaybackContract();
    trace("testSynthParameterSurface();");
    testSynthParameterSurface();
    trace("testSynthStereoAndHeadroom();");
    testSynthStereoAndHeadroom();
    trace("testSynthEnvelopeGateRelease();");
    testSynthEnvelopeGateRelease();
    trace("testSynthNoteOffSustain();");
    testSynthNoteOffSustain();
    trace("testSequencerNoteOff();");
    testSequencerNoteOff();
    trace("testNoteOffStepRoundTrip();");
    testNoteOffStepRoundTrip();
    trace("testLegatoInput();");
    testLegatoInput();
    trace("testLayeredTimbreDsp();");
    testLayeredTimbreDsp();
    trace("testAudioProducerThreadContract();");
    testAudioProducerThreadContract();
    trace("testAudioRuntimeContract();");
    testAudioRuntimeContract();
    trace("testApplicationSessionState();");
    testApplicationSessionState();
    trace("testAppSettingsPersistence();");
    testAppSettingsPersistence();
    trace("testPatchBrowserUxHelpers();");
    testPatchBrowserUxHelpers();
    trace("testScriptIntegrationSurface();");
    testScriptIntegrationSurface();
    trace("testFileCompatibilityInspection();");
    testFileCompatibilityInspection();
    trace("testExportWorkflow();");
    testExportWorkflow();
    trace("testAutoSaveRecovery();");
    testAutoSaveRecovery();
    trace("testProjectLifecyclePlanning();");
    testProjectLifecyclePlanning();
    trace("testApplicationActionBridge();");
    testApplicationActionBridge();
    trace("testApplicationTaskTracking();");
    testApplicationTaskTracking();
    trace("testProjectRoundTrip();");
    testProjectRoundTrip();
    trace("testMetadataCommands();");
    testMetadataCommands();
    trace("testPatternEditorCommands();");
    testPatternEditorCommands();
    trace("testSelectionClipboardAndUndo();");
    testSelectionClipboardAndUndo();
    trace("testEditorActionRegistry();");
    testEditorActionRegistry();
    trace("testEditorShortcutMap();");
    testEditorShortcutMap();
    trace("testEditorCommandPalette();");
    testEditorCommandPalette();
    trace("testStepAutomation();");
    testStepAutomation();
    trace("testStepProbabilityAndRetrigger();");
    testStepProbabilityAndRetrigger();
    trace("testStepEffects();");
    testStepEffects();
    trace("testArrangementCommands();");
    testArrangementCommands();
    trace("testTrackLifecycleCommands();");
    testTrackLifecycleCommands();
    trace("testDeletionAndOrderCommands();");
    testDeletionAndOrderCommands();
    trace("testInstrumentCommands();");
    testInstrumentCommands();
    trace("testPatchRoundTrip();");
    testPatchRoundTrip();
    trace("testCompositionalTransforms();");
    testCompositionalTransforms();
    trace("testPatternView();");
    testPatternView();
    trace("testEditorViewModel();");
    testEditorViewModel();
    trace("testArrangementView();");
    testArrangementView();
    trace("testProjectStats();");
    testProjectStats();
    trace("testProjectDiagnostics();");
    testProjectDiagnostics();
    trace("testMidiExport();");
    testMidiExport();
    trace("testMidiImport();");
    testMidiImport();
    trace("testMidiImportTempoMapAndProgramSplit();");
    testMidiImportTempoMapAndProgramSplit();
    trace("testGuiShellContract();");
    testGuiShellContract();
    return 0;
}
