#include "EditorViewModel.h"

#include <algorithm>
#include <map>
#include <stdexcept>

namespace arachno {

namespace {
std::string trackNameFor(const Song& song, int track) {
    if (track >= 0 && track < static_cast<int>(song.tracks.size())) {
        return song.tracks[static_cast<std::size_t>(track)].name;
    }
    return "T" + std::to_string(track);
}

std::vector<int> countPatternOrderUses(const Song& song) {
    std::vector<int> counts(song.patterns.size(), 0);
    for (int pattern : song.order) {
        if (pattern >= 0 && pattern < static_cast<int>(counts.size())) {
            ++counts[static_cast<std::size_t>(pattern)];
        }
    }
    return counts;
}

void countContent(
    const Song& song,
    std::vector<int>& notesByTrack,
    std::vector<int>& automationByTrack,
    std::map<int, int>& notesByInstrument) {
    notesByTrack.assign(song.tracks.size(), 0);
    automationByTrack.assign(song.tracks.size(), 0);
    notesByInstrument.clear();

    for (const Pattern& pattern : song.patterns) {
        for (int row = 0; row < pattern.rowCount(); ++row) {
            for (int track = 0; track < pattern.trackCount(); ++track) {
                const PatternStep& step = pattern.step(row, track);
                if ((!step.automation.empty() || !step.effects.empty())
                    && track < static_cast<int>(automationByTrack.size())) {
                    ++automationByTrack[static_cast<std::size_t>(track)];
                }
                if (!step.note.has_value()) {
                    continue;
                }
                if (track < static_cast<int>(notesByTrack.size())) {
                    ++notesByTrack[static_cast<std::size_t>(track)];
                }
                ++notesByInstrument[step.instrument];
            }
        }
    }
}

int activeInstrumentIndex(const Song& song, const EditorCursor& cursor) {
    if (cursor.pattern < 0 || cursor.pattern >= static_cast<int>(song.patterns.size())) {
        return -1;
    }
    const Pattern& pattern = song.patterns[static_cast<std::size_t>(cursor.pattern)];
    if (cursor.row < 0 || cursor.row >= pattern.rowCount()
        || cursor.track < 0 || cursor.track >= pattern.trackCount()) {
        return -1;
    }
    return pattern.step(cursor.row, cursor.track).instrument;
}

std::string instrumentNameFor(const Song& song, int instrument) {
    if (instrument >= 0 && instrument < static_cast<int>(song.instruments.size())) {
        return song.instruments[static_cast<std::size_t>(instrument)].patch.name;
    }
    return "<missing>";
}

bool selectionContainsCursor(const EditorSelection& selection, const EditorCursor& cursor) {
    return cursor.row >= selection.startRow
        && cursor.row < selection.startRow + selection.rowCount
        && cursor.track >= selection.startTrack
        && cursor.track < selection.startTrack + selection.trackCount;
}

ActiveStepSummary buildActiveStepSummary(const Song& song, const EditorCursor& cursor) {
    const Pattern& pattern = song.patterns[static_cast<std::size_t>(cursor.pattern)];
    const PatternStep& step = pattern.step(cursor.row, cursor.track);

    ActiveStepSummary summary;
    summary.row = cursor.row;
    summary.track = cursor.track;
    summary.trackName = trackNameFor(song, cursor.track);
    summary.hasNote = step.note.has_value();
    if (step.note.has_value()) {
        summary.noteName = step.note->name();
        summary.midiNote = step.note->midi;
        summary.velocity = step.note->velocity;
    }
    summary.instrument = step.instrument;
    summary.instrumentName = instrumentNameFor(song, step.instrument);
    summary.gateRows = step.gate;
    summary.microOffsetRows = step.microOffsetRows;
    summary.hasProbability = step.probability.has_value();
    summary.probability = step.probability.value_or(1.0);
    summary.retriggerCount = step.retriggerCount;
    summary.retriggerSpacingRows = step.retriggerSpacingRows;
    summary.retriggerVelocityDecay = step.retriggerVelocityDecay;

    summary.automation.reserve(step.automation.size());
    for (const auto& [parameter, value] : step.automation) {
        summary.automation.push_back({parameter, value});
    }
    summary.effects.reserve(step.effects.size());
    for (const EffectCommand& effect : step.effects) {
        EffectValueSummary effectSummary;
        effectSummary.name = effect.name;
        effectSummary.parameters.reserve(effect.parameters.size());
        for (const auto& [parameter, value] : effect.parameters) {
            effectSummary.parameters.push_back({parameter, value});
        }
        summary.effects.push_back(effectSummary);
    }
    return summary;
}

SelectionSummary buildSelectionSummary(
    const Song& song,
    const EditorCursor& cursor,
    const EditorSelection& selection) {
    const Pattern& pattern = song.patterns[static_cast<std::size_t>(cursor.pattern)];
    SelectionSummary summary;
    summary.startRow = selection.startRow;
    summary.startTrack = selection.startTrack;
    summary.rowCount = selection.rowCount;
    summary.trackCount = selection.trackCount;
    summary.stepCount = selection.rowCount * selection.trackCount;
    summary.containsCursor = selectionContainsCursor(selection, cursor);

    for (int rowOffset = 0; rowOffset < selection.rowCount; ++rowOffset) {
        for (int trackOffset = 0; trackOffset < selection.trackCount; ++trackOffset) {
            const int row = selection.startRow + rowOffset;
            const int track = selection.startTrack + trackOffset;
            if (row < 0 || row >= pattern.rowCount() || track < 0 || track >= pattern.trackCount()) {
                continue;
            }
            const PatternStep& step = pattern.step(row, track);
            if (step.note.has_value()) {
                ++summary.noteCount;
            }
            if (!step.automation.empty() || !step.effects.empty()) {
                ++summary.automatedStepCount;
            }
        }
    }
    return summary;
}
} // namespace

EditorViewModel buildEditorViewModel(
    const Song& song,
    const PatternEditorSession& editor,
    int gridStartRow,
    int gridRowCount) {
    const EditorCursor& cursor = editor.cursor();
    if (cursor.pattern < 0 || cursor.pattern >= static_cast<int>(song.patterns.size())) {
        throw std::out_of_range("editor cursor pattern is out of range");
    }

    EditorViewModel model;
    const Pattern& activePattern = song.patterns[static_cast<std::size_t>(cursor.pattern)];
    const EditorSelection& selection = editor.selection();

    model.status.title = song.title;
    model.status.bpm = song.bpm;
    model.status.rowsPerBeat = song.rowsPerBeat;
    model.status.totalRows = song.totalRows();
    model.status.durationSeconds = song.durationSeconds();
    model.status.activePattern = cursor.pattern;
    model.status.activePatternName = activePattern.name();
    model.status.cursorRow = cursor.row;
    model.status.cursorTrack = cursor.track;
    model.status.cursorTrackName = trackNameFor(song, cursor.track);
    model.status.selectionStartRow = selection.startRow;
    model.status.selectionStartTrack = selection.startTrack;
    model.status.selectionRows = selection.rowCount;
    model.status.selectionTracks = selection.trackCount;
    model.status.canUndo = editor.canUndo();
    model.status.canRedo = editor.canRedo();

    const std::vector<int> orderUseCounts = countPatternOrderUses(song);
    model.patterns.reserve(song.patterns.size());
    for (std::size_t index = 0; index < song.patterns.size(); ++index) {
        const Pattern& pattern = song.patterns[index];
        PatternSummary summary;
        summary.index = static_cast<int>(index);
        summary.name = pattern.name();
        summary.rowCount = pattern.rowCount();
        summary.trackCount = pattern.trackCount();
        summary.orderUseCount = orderUseCounts[index];
        summary.active = summary.index == cursor.pattern;
        model.patterns.push_back(summary);
    }

    int startRow = 0;
    model.order.reserve(song.order.size());
    for (std::size_t index = 0; index < song.order.size(); ++index) {
        const int patternIndex = song.order[index];
        OrderSlotSummary summary;
        summary.index = static_cast<int>(index);
        summary.pattern = patternIndex;
        summary.startRow = startRow;
        summary.activePattern = patternIndex == cursor.pattern;
        if (patternIndex >= 0 && patternIndex < static_cast<int>(song.patterns.size())) {
            const Pattern& pattern = song.patterns[static_cast<std::size_t>(patternIndex)];
            summary.patternName = pattern.name();
            summary.rowCount = pattern.rowCount();
            startRow += pattern.rowCount();
        } else {
            summary.patternName = "<missing>";
            summary.missing = true;
        }
        model.order.push_back(summary);
    }

    std::vector<int> notesByTrack;
    std::vector<int> automationByTrack;
    std::map<int, int> notesByInstrument;
    countContent(song, notesByTrack, automationByTrack, notesByInstrument);

    model.tracks.reserve(song.tracks.size());
    for (std::size_t index = 0; index < song.tracks.size(); ++index) {
        const Track& track = song.tracks[index];
        TrackStripSummary summary;
        summary.index = static_cast<int>(index);
        summary.name = track.name;
        summary.volume = track.volume;
        summary.pan = track.pan;
        summary.muted = track.muted;
        summary.solo = track.solo;
        summary.active = summary.index == cursor.track;
        summary.noteCount = notesByTrack[index];
        summary.automatedStepCount = automationByTrack[index];
        model.tracks.push_back(summary);
    }

    const int activeInstrument = activeInstrumentIndex(song, cursor);
    model.instruments.reserve(song.instruments.size());
    for (std::size_t index = 0; index < song.instruments.size(); ++index) {
        const Instrument& instrument = song.instruments[index];
        InstrumentSummary summary;
        summary.index = static_cast<int>(index);
        summary.name = instrument.patch.name;
        summary.oscillatorA = waveformName(instrument.patch.oscillatorA);
        summary.oscillatorB = waveformName(instrument.patch.oscillatorB);
        summary.oscillatorC = waveformName(instrument.patch.oscillatorC);
        summary.oscillatorD = waveformName(instrument.patch.oscillatorD);
        summary.cutoff = instrument.patch.cutoff;
        summary.drive = instrument.patch.drive;
        summary.gain = instrument.patch.gain;
        summary.noteUseCount = notesByInstrument[summary.index];
        summary.active = summary.index == activeInstrument;
        model.instruments.push_back(summary);
    }

    model.activeStep = buildActiveStepSummary(song, cursor);
    model.selection = buildSelectionSummary(song, cursor, selection);
    model.clipboard.available = editor.hasClipboard();
    model.clipboard.rowCount = editor.clipboardRowCount();
    model.clipboard.trackCount = editor.clipboardTrackCount();

    model.activeGrid = buildPatternGrid(
        song,
        cursor.pattern,
        gridStartRow,
        gridRowCount,
        &cursor,
        &selection);

    return model;
}

} // namespace arachno
