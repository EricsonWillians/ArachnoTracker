#include "ui/gui/GuiSynthScopeSection.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

#include "Note.h"

namespace arachno {

GuiSynthScopeRenderResult drawSynthScopeSection(const GuiSynthScopeRenderContext& context) {
    GuiSynthScopeRenderResult result;

    int scopeMonitorMidi = std::clamp(context.synthPreviewMidi, 0, 127);
    bool scopeSignalActive = false;
    bool scopeFromPlayback = false;
    double scopeGateSeconds = 0.25;
    std::vector<int> rowChordNotes;
    std::vector<int> rowInstrumentNotes;
    const PlaybackSnapshot playback = context.session.playback().snapshot();
    if (playback.state == TransportState::Playing && playback.position.validPattern) {
        const Song& song = context.session.song();
        const int patternIndex = playback.position.pattern;
        const int rowIndex = playback.position.patternRow;
        if (patternIndex >= 0 && patternIndex < static_cast<int>(song.patterns.size())) {
            const Pattern& playingPattern = song.patterns[static_cast<std::size_t>(patternIndex)];
            if (rowIndex >= 0 && rowIndex < playingPattern.rowCount()) {
                int fallbackMidi = -1;
                double fallbackGateRows = 0.88;
                for (int track = 0; track < playingPattern.trackCount(); ++track) {
                    const PatternStep& step = playingPattern.step(rowIndex, track);
                    if (!step.note.has_value()) {
                        continue;
                    }
                    rowChordNotes.push_back(std::clamp(step.note->midi, 0, 127));
                    if (fallbackMidi < 0) {
                        fallbackMidi = step.note->midi;
                        fallbackGateRows = step.gate;
                    }
                    if (step.instrument == context.instrument) {
                        rowInstrumentNotes.push_back(std::clamp(step.note->midi, 0, 127));
                        scopeMonitorMidi = std::clamp(step.note->midi, 0, 127);
                        scopeFromPlayback = true;
                        scopeSignalActive = true;
                        scopeGateSeconds = std::max(0.01, step.gate * song.secondsPerRow());
                    }
                }
                if (!scopeFromPlayback && fallbackMidi >= 0) {
                    scopeMonitorMidi = std::clamp(fallbackMidi, 0, 127);
                    scopeFromPlayback = true;
                    scopeSignalActive = true;
                    scopeGateSeconds = std::max(0.01, fallbackGateRows * song.secondsPerRow());
                }
            }
        }
    }
    std::sort(rowChordNotes.begin(), rowChordNotes.end());
    rowChordNotes.erase(std::unique(rowChordNotes.begin(), rowChordNotes.end()), rowChordNotes.end());
    std::sort(rowInstrumentNotes.begin(), rowInstrumentNotes.end());
    rowInstrumentNotes.erase(std::unique(rowInstrumentNotes.begin(), rowInstrumentNotes.end()), rowInstrumentNotes.end());
    result.previewNotes = context.collectSynthPreviewNotes();
    if (!scopeSignalActive && playback.previewActive) {
        scopeSignalActive = true;
        scopeMonitorMidi = std::clamp(context.synthPreviewMidi, 0, 127);
        scopeGateSeconds = 0.35;
    }
    if (scopeMonitorMidi != context.runtime.trackedMidi && scopeSignalActive) {
        context.runtime.trackedMidi = scopeMonitorMidi;
        for (std::size_t index = 0; index < context.runtime.phase.size(); ++index) {
            context.runtime.phase[index] = std::fmod(
                (context.runtime.phase[index] * 0.35) + (0.17 * static_cast<double>(index + 1)),
                1.0);
        }
    }

    const int scopeSampleRate = std::max(8000, playback.sampleRate);
    if (scopeSignalActive) {
        const bool retrigger = context.runtime.retriggerRequested
            || !context.runtime.laneActive
            || context.runtime.laneInstrument != context.instrument
            || context.runtime.laneMidi != scopeMonitorMidi
            || std::abs(context.runtime.laneGateSeconds - scopeGateSeconds) > 0.0001;
        if (retrigger) {
            for (int lane = 0; lane < 4; ++lane) {
                SynthPatch lanePatch = context.patch;
                lanePatch.oscillatorAEnabled = lane == 0;
                lanePatch.oscillatorBEnabled = lane == 1;
                lanePatch.oscillatorCEnabled = lane == 2;
                lanePatch.oscillatorDEnabled = lane == 3;
                lanePatch.subEnabled = false;
                lanePatch.noiseEnabled = false;
                lanePatch.fmEnabled = false;
                lanePatch.ringEnabled = false;
                lanePatch.hardSyncEnabled = false;
                lanePatch.chorusEnabled = false;
                lanePatch.bitCrushEnabled = false;
                lanePatch.sampleRateReduction = 0.0;
                lanePatch.combMix = 0.0;
                lanePatch.wavefold = 0.0;
                lanePatch.drive = 0.0;
                lanePatch.filterDrive = 0.0;
                lanePatch.filterEnvelopeAmount = 0.0;
                lanePatch.lfoFilterDepth = 0.0;
                lanePatch.lfoPanDepth = 0.0;
                lanePatch.vibratoCents = 0.0;
                lanePatch.tremoloDepth = 0.0;
                lanePatch.pitchEnvelopeSemitones = 0.0;
                lanePatch.highPass = 0.0;
                lanePatch.cutoff = 1.0;
                lanePatch.resonance = 0.0;
                lanePatch.unisonVoices = 1;
                lanePatch.unisonDetuneCents = 0.0;
                lanePatch.ampEnvelope.attack = 0.001;
                lanePatch.ampEnvelope.decay = 0.03;
                lanePatch.ampEnvelope.sustain = 1.0;
                lanePatch.ampEnvelope.release = 0.08;
                lanePatch.name = context.patch.name + " [OSC " + std::string(1, static_cast<char>('A' + lane)) + "]";
                context.runtime.laneSynth[static_cast<std::size_t>(lane)].reset();
                context.runtime.laneSynth[static_cast<std::size_t>(lane)].setSampleRate(static_cast<double>(scopeSampleRate));
                context.runtime.laneSynth[static_cast<std::size_t>(lane)].noteOn(
                    Note(scopeMonitorMidi, std::clamp(context.defaultVelocity, 0.05f, 1.0f)),
                    lanePatch,
                    0.0,
                    scopeGateSeconds);
            }
            context.runtime.laneActive = true;
            context.runtime.laneInstrument = context.instrument;
            context.runtime.laneMidi = scopeMonitorMidi;
            context.runtime.laneGateSeconds = scopeGateSeconds;
            context.runtime.retriggerRequested = false;
        }
        for (int lane = 0; lane < 4; ++lane) {
            auto& leftTrace = context.runtime.laneLeft[static_cast<std::size_t>(lane)];
            auto& rightTrace = context.runtime.laneRight[static_cast<std::size_t>(lane)];
            std::fill(leftTrace.begin(), leftTrace.end(), 0.0f);
            std::fill(rightTrace.begin(), rightTrace.end(), 0.0f);
            context.runtime.laneSynth[static_cast<std::size_t>(lane)].setSampleRate(static_cast<double>(scopeSampleRate));
            context.runtime.laneSynth[static_cast<std::size_t>(lane)].render(
                leftTrace.data(),
                rightTrace.data(),
                static_cast<int>(leftTrace.size()));
        }
    } else if (context.runtime.laneActive) {
        context.runtime.laneActive = false;
        for (Synthesizer& laneSynth : context.runtime.laneSynth) {
            laneSynth.reset();
        }
    }
    const UiRect scopePanel {
        context.panelLeft,
        context.oscTop + (context.oscRowHeight * 4) + 8,
        context.panelRight - context.panelLeft,
        170};
    context.drawFilledRect(scopePanel.x, scopePanel.y, scopePanel.width, scopePanel.height, context.theme.panel);
    context.drawRect(scopePanel.x, scopePanel.y, scopePanel.width, scopePanel.height, context.theme.buttonActive);
    const UiRect scopeHeader {scopePanel.x + 1, scopePanel.y + 1, scopePanel.width - 2, 22};
    context.drawFilledRect(scopeHeader.x, scopeHeader.y, scopeHeader.width, scopeHeader.height, context.theme.gridHeader);
    context.drawRect(scopeHeader.x, scopeHeader.y, scopeHeader.width, scopeHeader.height, context.theme.gridLine);
    context.drawText(scopePanel.x + 8, scopePanel.y + 16, "OSCILLATOR MONITOR (REAL TIME)", context.theme.text);
    std::string scopeSourceText;
    if (scopeSignalActive && scopeFromPlayback) {
        scopeSourceText = "Source: playback note " + midiNoteName(scopeMonitorMidi);
    } else if (scopeSignalActive) {
        scopeSourceText = "Source: preview note " + midiNoteName(scopeMonitorMidi);
    } else {
        scopeSourceText = "Source: idle (no active note)";
    }
    auto compactNoteList = [&](const std::vector<int>& notes) {
        if (notes.empty()) {
            return std::string("-");
        }
        std::ostringstream out;
        const int maxNotes = 5;
        const int count = std::min(maxNotes, static_cast<int>(notes.size()));
        for (int index = 0; index < count; ++index) {
            if (index > 0) {
                out << " ";
            }
            out << midiNoteName(notes[static_cast<std::size_t>(index)]);
        }
        if (static_cast<int>(notes.size()) > maxNotes) {
            out << " ...";
        }
        return out.str();
    };
    // Enhanced polyphony / voice info display
    const std::string scopePolyText =
        "Poly " + std::to_string(result.previewNotes.size())
        + " | Row " + std::to_string(rowChordNotes.size())
        + " | Inst " + std::to_string(rowInstrumentNotes.size())
        + " | Chord " + compactNoteList(!rowInstrumentNotes.empty() ? rowInstrumentNotes : rowChordNotes);
    std::string voiceInfoText = "UNI×" + std::to_string(context.patch.unisonVoices);
    // Show active synthesizer voice count from telemetry if available
    if (context.runtime.laneSynth[0].active()) {
        int totalActiveVoices = 0;
        for (std::size_t li = 0; li < 4; ++li) {
            totalActiveVoices += context.runtime.laneSynth[li].telemetry().activeVoices;
        }
        voiceInfoText += " | Voices " + std::to_string(totalActiveVoices);
    }
    context.drawText(
        scopePanel.x + 308,
        scopePanel.y + 16,
        context.fitText(scopeSourceText + " | " + scopePolyText + " | " + voiceInfoText, std::max(80, scopePanel.width - 316)),
        context.theme.mutedText);
    const int laneTop = scopePanel.y + 28;
    const int laneHeight = 31;
    const int laneGap = 4;
    const int laneWidth = scopePanel.width - 12;
    auto waveTraceColors = [&](Waveform wave) {
        switch (wave) {
            case Waveform::Sine: return std::pair<unsigned long, unsigned long> {context.traceColors.waveSine, context.traceColors.tealGlow};
            case Waveform::Square: return std::pair<unsigned long, unsigned long> {context.traceColors.waveSquare, context.traceColors.tealGlow};
            case Waveform::Saw: return std::pair<unsigned long, unsigned long> {context.traceColors.waveSaw, context.traceColors.tealGlow};
            case Waveform::Triangle: return std::pair<unsigned long, unsigned long> {context.traceColors.waveTriangle, context.traceColors.tealGlow};
            case Waveform::Noise: return std::pair<unsigned long, unsigned long> {context.traceColors.waveNoise, context.traceColors.tealGlow};
            case Waveform::SuperSaw: return std::pair<unsigned long, unsigned long> {context.traceColors.waveSupersaw, context.traceColors.tealGlow};
        }
        return std::pair<unsigned long, unsigned long> {context.traceColors.tealMain, context.traceColors.tealGlow};
    };
    auto drawScopeLane = [&](int laneIndex, Waveform wave, bool enabled, double level) {
        const UiRect lane {
            scopePanel.x + 6,
            laneTop + (laneIndex * (laneHeight + laneGap)),
            laneWidth,
            laneHeight};
        const bool alt = (laneIndex % 2) != 0;
        context.drawFilledRect(lane.x, lane.y, lane.width, lane.height, alt ? context.theme.background : context.theme.panel);
        context.drawRect(lane.x, lane.y, lane.width, lane.height, context.theme.gridLine);
        const UiRect labelPane {lane.x + 1, lane.y + 1, 164, lane.height - 2};
        context.drawFilledRect(labelPane.x, labelPane.y, labelPane.width, labelPane.height, context.theme.gridHeader);
        context.drawRect(labelPane.x, labelPane.y, labelPane.width, labelPane.height, context.theme.gridLine);
        const int levelPct = static_cast<int>(std::lround(std::clamp(level, 0.0, 1.0) * 100.0));
        const std::pair<unsigned long, unsigned long> traceColors = waveTraceColors(wave);
        const unsigned long traceMain = traceColors.first;
        const unsigned long traceGlow = traceColors.second;
        std::string label = std::string("OSC ") + static_cast<char>('A' + laneIndex)
            + "  " + waveformName(wave)
            + "  LVL " + std::to_string(levelPct) + "%";
        // Append unison info when relevant
        if (context.patch.unisonVoices > 1 && enabled) {
            label += "  UNI×" + std::to_string(context.patch.unisonVoices);
        }
        context.drawText(lane.x + 6, lane.y + 13, label, enabled ? traceMain : context.theme.mutedText);
        if (enabled) {
            const int meterX = lane.x + 6;
            const int meterY = lane.y + lane.height - 10;
            const int meterW = 150;
            const int meterH = 5;
            context.drawFilledRect(meterX, meterY, meterW, meterH, context.theme.background);
            context.drawRect(meterX, meterY, meterW, meterH, context.theme.gridLine);
            const int fillW = std::clamp(static_cast<int>(std::lround((static_cast<double>(meterW - 2) * levelPct) / 100.0)), 0, meterW - 2);
            context.drawFilledRect(meterX + 1, meterY + 1, fillW, meterH - 2, traceMain);
            // Voice activity LED: small bright dot when this oscillator is actively sounding
            if (scopeSignalActive && context.runtime.laneSynth[static_cast<std::size_t>(laneIndex)].active()) {
                const int ledX = lane.x + lane.width - 14;
                const int ledY = lane.y + 4;
                context.drawFilledRect(ledX, ledY, 6, 6, traceMain);
                context.drawRect(ledX, ledY, 6, 6, context.theme.gridLine);
            }
        }
        const int graphX = lane.x + 170;
        const int graphY = lane.y + 2;
        const int graphW = lane.width - 174;
        const int graphH = lane.height - 4;
        context.drawFilledRect(graphX, graphY, graphW, graphH, context.theme.background);
        context.drawRect(graphX, graphY, graphW, graphH, context.traceColors.tealMain);
        const int midY = graphY + graphH / 2;
        context.setStrokeColor(context.traceColors.tealDark);
        context.drawLine(graphX + 1, midY, graphX + graphW - 2, midY);
        if (!enabled) {
            context.drawText(graphX + 6, graphY + 14, "OFF", context.theme.mutedText);
            return;
        }
        if (!scopeSignalActive) {
            context.drawText(graphX + 6, graphY + 14, "IDLE", context.theme.mutedText);
            return;
        }
        context.setStrokeColor(context.traceColors.tealDark);
        for (int gx = graphX + 12; gx < graphX + graphW - 2; gx += 12) {
            context.drawPoint(gx, midY);
        }
        context.setStrokeColor(context.traceColors.tealDark);
        for (int gy = graphY + 4; gy < graphY + graphH - 2; gy += 5) {
            context.drawPoint(graphX + 2, gy);
            context.drawPoint(graphX + graphW - 3, gy);
        }
        const int width = graphW - 2;
        const int height = graphH - 2;
        int prevX = graphX + 1;
        const auto& trace = context.runtime.laneLeft[static_cast<std::size_t>(laneIndex)];
        double peak = 0.0001;
        for (float sampleValue : trace) {
            peak = std::max(peak, std::abs(static_cast<double>(sampleValue)));
        }
        const double normalization = std::clamp(0.92 / peak, 0.35, 14.0);
        const auto sampleAt = [&](int pixel) {
            const int lastIndex = static_cast<int>(trace.size()) - 1;
            const int clampedPixel = std::clamp(pixel, 0, std::max(0, width - 1));
            const int index = lastIndex <= 0
                ? 0
                : (clampedPixel * lastIndex) / std::max(1, width - 1);
            const double sampleValue = std::clamp(
                static_cast<double>(trace[static_cast<std::size_t>(index)]) * normalization,
                -1.0,
                1.0);
            return midY - static_cast<int>(std::lround(sampleValue * (height * 0.5)));
        };
        int prevY = sampleAt(0);
        for (int px = 1; px < width; ++px) {
            const int x = graphX + px;
            const int yv = sampleAt(px);
            context.setStrokeColor(traceGlow);
            context.drawLine(prevX, prevY + 1, x, yv + 1);
            context.drawLine(prevX, prevY - 1, x, yv - 1);
            context.setStrokeColor(traceMain);
            context.drawLine(prevX, prevY, x, yv);
            prevX = x;
            prevY = yv;
        }
    };
    drawScopeLane(0, context.patch.oscillatorA, context.patch.oscillatorAEnabled, context.patch.oscALevel);
    drawScopeLane(1, context.patch.oscillatorB, context.patch.oscillatorBEnabled, context.patch.oscBLevel);
    drawScopeLane(2, context.patch.oscillatorC, context.patch.oscillatorCEnabled, context.patch.oscCLevel);
    drawScopeLane(3, context.patch.oscillatorD, context.patch.oscillatorDEnabled, context.patch.oscDLevel);

    result.keyboardTop = scopePanel.y + scopePanel.height + 18;
    return result;
}

} // namespace arachno
