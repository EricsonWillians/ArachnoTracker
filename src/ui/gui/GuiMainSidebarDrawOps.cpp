#include "ui/gui/GuiMainSidebarDrawOps.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>
#include "Note.h"
#include "ui/gui/GuiInstrumentBrowserOps.h"

namespace arachno {

namespace {

int instrumentCategoryRank(const std::string& category) {
    if (category == "Percussion") {
        return 0;
    }
    if (category == "Bass") {
        return 1;
    }
    if (category == "Lead") {
        return 2;
    }
    if (category == "Pads") {
        return 3;
    }
    if (category == "Strings") {
        return 4;
    }
    if (category == "Brass") {
        return 5;
    }
    if (category == "Guitar") {
        return 6;
    }
    if (category == "FX") {
        return 7;
    }
    return 8;
}

std::string instrumentCategoryShortLabel(const std::string& category) {
    if (category == "Percussion") {
        return "DRUM";
    }
    if (category == "Bass") {
        return "BASS";
    }
    if (category == "Lead") {
        return "LEAD";
    }
    if (category == "Pads") {
        return "PAD";
    }
    if (category == "Strings") {
        return "STR";
    }
    if (category == "Brass") {
        return "BRASS";
    }
    if (category == "Guitar") {
        return "GTR";
    }
    if (category == "FX") {
        return "FX";
    }
    return "MIDI";
}

} // namespace

int drawMainSidebarSections(const GuiMainSidebarDrawContext& context, int sy) {
    auto sidebarLine = [&](const std::string& line, bool muted = false) {
        context.drawText(context.sidebarLeft + 8, sy, line, muted ? context.colorMutedText : context.colorText);
        sy += 16;
    };

    const AppSessionSnapshot& snap = context.snapshot;
    const auto describeInstrument = [&](int index) -> std::string {
        if (index < 0 || index >= static_cast<int>(snap.editor.instruments.size())) {
            return "unset";
        }
        const InstrumentSummary& instrument = snap.editor.instruments[static_cast<std::size_t>(index)];
        std::ostringstream stream;
        stream << "I" << (index < 10 ? "0" : "") << index << "  "
               << instrumentCategoryFromName(instrument.name) << "  " << instrument.name;
        return stream.str();
    };

    sidebarLine("ACTIVE STEP");
    std::ostringstream step;
    step << "Row " << snap.editor.activeStep.row << " Track " << snap.editor.activeStep.trackName;
    sidebarLine(step.str(), true);
    const int activeStepInstrument = (snap.editor.activeStep.instrument >= 0)
        ? snap.editor.activeStep.instrument
        : context.armedInstrument;
    sidebarLine(
        snap.editor.activeStep.hasNote
            ? ("Note " + snap.editor.activeStep.noteName + "  uses  " + describeInstrument(activeStepInstrument))
            : "Note ---  uses  " + describeInstrument(activeStepInstrument));
    {
        std::ostringstream vel;
        vel << "Velocity " << static_cast<int>(snap.editor.activeStep.velocity * 100.0f)
            << "  Gate " << snap.editor.activeStep.gateRows;
        sidebarLine(vel.str(), true);
    }
    sidebarLine("NOTE KEYS");
    sidebarLine("Z S X D C V G B H N J M", true);
    sidebarLine("Q 2 W 3 E R 5 T 6 Y 7 U", true);
    sidebarLine("Paint note " + midiNoteName(context.paintNoteMidi), true);
    sidebarLine("OCTAVE");
    {
        const int octaveButtonY = sy - 12;
        const UiRect minusRect {context.sidebarLeft + 8, octaveButtonY, 20, 20};
        const UiRect plusRect {context.sidebarLeft + 220, octaveButtonY, 20, 20};
        context.drawButton(minusRect, "-", false);
        context.drawButton(plusRect, "+", false);
        context.octaveHitTargets.push_back({minusRect, -1});
        context.octaveHitTargets.push_back({plusRect, 100});

        int ox = context.sidebarLeft + 32;
        for (int octave = 0; octave <= 8; ++octave) {
            const UiRect rect {ox, octaveButtonY, 20, 20};
            context.drawButton(rect, std::to_string(octave), octave == context.armedOctave);
            context.octaveHitTargets.push_back({rect, octave});
            ox += 21;
        }
        sy += 22;
    }
    sidebarLine("PIANO");
    {
        const int whiteKeyWidth = 16;
        const int whiteKeyHeight = 42;
        const int blackKeyWidth = 10;
        const int blackKeyHeight = 24;
        const int pianoX = context.sidebarLeft + 8;
        const int pianoY = sy - 12;
        // Display convention (midiNoteName: 60 = C4): armed octave N starts at (N + 1) * 12.
        const int baseMidi = std::clamp((context.armedOctave + 1) * 12, 0, 115);
        const int whiteSemitones[7] = {0, 2, 4, 5, 7, 9, 11};
        const int blackSemitones[5] = {1, 3, 6, 8, 10};
        const int blackXOffsets[5] = {11, 27, 59, 75, 91};

        for (int white = 0; white < 7; ++white) {
            const int midi = std::clamp(baseMidi + whiteSemitones[white], 0, 127);
            const UiRect rect {pianoX + (white * whiteKeyWidth), pianoY, whiteKeyWidth, whiteKeyHeight};
            const bool active = midi == context.paintNoteMidi;
            context.drawFilledRect(
                rect.x,
                rect.y,
                rect.width,
                rect.height,
                active ? context.colorButtonActive : context.pianoWhite);
            context.drawRect(rect.x, rect.y, rect.width, rect.height, context.colorGridLine);
            context.pianoKeyHits.push_back({rect, midi, false});
        }
        for (int black = 0; black < 5; ++black) {
            const int midi = std::clamp(baseMidi + blackSemitones[black], 0, 127);
            const UiRect rect {pianoX + blackXOffsets[black], pianoY, blackKeyWidth, blackKeyHeight};
            const bool active = midi == context.paintNoteMidi;
            context.drawFilledRect(
                rect.x,
                rect.y,
                rect.width,
                rect.height,
                active ? context.colorButtonActive : context.pianoBlack);
            context.drawRect(rect.x, rect.y, rect.width, rect.height, context.colorGridLine);
            context.pianoKeyHits.push_back({rect, midi, true});
        }
        context.drawText(pianoX + 120, pianoY + 18, midiNoteName(baseMidi), context.colorMutedText);
        context.drawText(
            pianoX + 120,
            pianoY + 34,
            midiNoteName(std::clamp(baseMidi + 12, 0, 127)),
            context.colorMutedText);
        sy += 46;
    }
    sidebarLine("");
    sidebarLine("PATTERN ROWS");
    context.patternRowsMinus = UiRect {context.sidebarLeft + 8, sy - 12, 20, 20};
    context.patternRowsValue = UiRect {context.sidebarLeft + 32, sy - 12, 88, 20};
    context.patternRowsPlus = UiRect {context.sidebarLeft + 124, sy - 12, 20, 20};
    context.drawButton(context.patternRowsMinus, "-", false);
    context.drawButton(context.patternRowsValue, std::to_string(context.activePatternRows), false);
    context.drawButton(context.patternRowsPlus, "+", false);
    sy += 20;
    sidebarLine("Drag row count or Ctrl+Wheel", true);

    context.stepAdvanceButton = UiRect {context.sidebarLeft + 8, sy - 10, 112, 20};
    context.followPlaybackButton = UiRect {context.sidebarLeft + 126, sy - 10, 112, 20};
    context.drawButton(
        context.stepAdvanceButton,
        context.stepAdvance ? "STEP ON" : "STEP OFF",
        context.stepAdvance);
    context.drawButton(
        context.followPlaybackButton,
        context.followPlayback ? "FOLLOW ON" : "FOLLOW OFF",
        context.followPlayback);
    sy += 18;

    context.legatoButton = UiRect {context.sidebarLeft + 8, sy - 10, 112, 20};
    context.drawButton(
        context.legatoButton,
        snap.editor.status.legatoInput ? "LEGATO ON" : "LEGATO OFF",
        snap.editor.status.legatoInput);
    context.drawText(
        context.sidebarLeft + 126,
        sy + 4,
        "(L) sustain",
        context.colorMutedText);
    sy += 18;
    sidebarLine("Legato sustains to next note/===", true);

    sidebarLine("");
    sidebarLine("TRACK METADATA");
    if (!snap.editor.tracks.empty()) {
        const int selectedTrack = std::clamp(
            snap.editor.status.cursorTrack,
            0,
            static_cast<int>(snap.editor.tracks.size()) - 1);
        const TrackStripSummary& selected = snap.editor.tracks[static_cast<std::size_t>(selectedTrack)];
        {
            std::ostringstream trackLine;
            trackLine << "T" << selectedTrack << "  " << selected.name;
            sidebarLine(trackLine.str());
        }
        {
            std::ostringstream mixLine;
            mixLine.setf(std::ios::fixed);
            mixLine.precision(2);
            mixLine << "Vol " << selected.volume << "  Pan " << selected.pan << "  "
                    << (selected.muted ? "M" : "-")
                    << (selected.solo ? "S" : "-");
            sidebarLine(mixLine.str(), true);
        }
        const int buttonY = sy - 12;
        const UiRect renameRect {context.sidebarLeft + 8, buttonY, 64, 20};
        const UiRect volDownRect {context.sidebarLeft + 76, buttonY, 28, 20};
        const UiRect volUpRect {context.sidebarLeft + 108, buttonY, 28, 20};
        const UiRect panLeftRect {context.sidebarLeft + 140, buttonY, 28, 20};
        const UiRect panRightRect {context.sidebarLeft + 172, buttonY, 28, 20};
        const UiRect muteRect {context.sidebarLeft + 204, buttonY, 40, 20};
        const UiRect soloRect {context.sidebarLeft + 248, buttonY, 40, 20};
        context.drawButton(renameRect, "RENAME", false);
        context.drawButton(volDownRect, "V-", false);
        context.drawButton(volUpRect, "V+", false);
        context.drawButton(panLeftRect, "P<", false);
        context.drawButton(panRightRect, "P>", false);
        context.drawButton(muteRect, "MUTE", selected.muted);
        context.drawButton(soloRect, "SOLO", selected.solo);
        context.trackMetadataHits.push_back({renameRect, "rename", selectedTrack});
        context.trackMetadataHits.push_back({volDownRect, "vol_down", selectedTrack});
        context.trackMetadataHits.push_back({volUpRect, "vol_up", selectedTrack});
        context.trackMetadataHits.push_back({panLeftRect, "pan_left", selectedTrack});
        context.trackMetadataHits.push_back({panRightRect, "pan_right", selectedTrack});
        context.trackMetadataHits.push_back({muteRect, "mute", selectedTrack});
        context.trackMetadataHits.push_back({soloRect, "solo", selectedTrack});
        sy += 22;
        sidebarLine("Keys: [ / ] prev-next  Shift+1..0 direct  \\ audition", true);
    } else {
        sidebarLine("No tracks", true);
    }

    sidebarLine("");
    sidebarLine("SONG LENGTH");
    {
        std::ostringstream lengthLine;
        lengthLine.setf(std::ios::fixed);
        lengthLine.precision(2);
        lengthLine << "Current " << (snap.editor.status.durationSeconds / 60.0) << "m";
        sidebarLine(lengthLine.str(), true);
    }
    {
        std::ostringstream targetLine;
        targetLine.setf(std::ios::fixed);
        targetLine.precision(2);
        targetLine << "Target  " << context.targetSongLengthMinutes << "m";
        sidebarLine(targetLine.str(), true);
    }
    {
        const int buttonY = sy - 12;
        const UiRect minusRect {context.sidebarLeft + 8, buttonY, 24, 20};
        const UiRect plusRect {context.sidebarLeft + 36, buttonY, 24, 20};
        const UiRect setRect {context.sidebarLeft + 64, buttonY, 40, 20};
        const UiRect buildRect {context.sidebarLeft + 108, buttonY, 56, 20};
        const UiRect trimRect {context.sidebarLeft + 168, buttonY, 56, 20};
        context.drawButton(minusRect, "-", false);
        context.drawButton(plusRect, "+", false);
        context.drawButton(setRect, "SET", false);
        context.drawButton(buildRect, "BUILD", false);
        context.drawButton(trimRect, "TRIM", false);
        context.songLengthHits.push_back({minusRect, "target_down"});
        context.songLengthHits.push_back({plusRect, "target_up"});
        context.songLengthHits.push_back({setRect, "target_set"});
        context.songLengthHits.push_back({buildRect, "build"});
        context.songLengthHits.push_back({trimRect, "trim"});
        sy += 22;
    }
    sidebarLine("Build extends order; trim removes tail order", true);

    sidebarLine("");
    sidebarLine("MIDI IMPORT SETTINGS");
    {
        std::ostringstream line;
        line << "Rows/beat " << context.midiImportRowsPerBeat;
        sidebarLine(line.str(), true);
        const int buttonY = sy - 12;
        const UiRect rpbDownRect {context.sidebarLeft + 8, buttonY, 24, 20};
        const UiRect rpbUpRect {context.sidebarLeft + 36, buttonY, 24, 20};
        context.drawButton(rpbDownRect, "-", false);
        context.drawButton(rpbUpRect, "+", false);
        context.midiImportSettingHits.push_back({rpbDownRect, "rpb_down"});
        context.midiImportSettingHits.push_back({rpbUpRect, "rpb_up"});
        sy += 22;
    }
    {
        std::ostringstream line;
        line << "Pattern rows " << context.midiImportPatternRows;
        sidebarLine(line.str(), true);
        const int buttonY = sy - 12;
        const UiRect rowsDownRect {context.sidebarLeft + 8, buttonY, 24, 20};
        const UiRect rowsUpRect {context.sidebarLeft + 36, buttonY, 24, 20};
        context.drawButton(rowsDownRect, "-", false);
        context.drawButton(rowsUpRect, "+", false);
        context.midiImportSettingHits.push_back({rowsDownRect, "rows_down"});
        context.midiImportSettingHits.push_back({rowsUpRect, "rows_up"});
        sy += 22;
    }
    {
        const int buttonY = sy - 12;
        const UiRect splitRect {context.sidebarLeft + 8, buttonY, 232, 20};
        context.drawButton(
            splitRect,
            context.midiImportSplitByTrack ? "SPLIT TRACKS" : "MERGE LANES",
            context.midiImportSplitByTrack);
        context.midiImportSettingHits.push_back({splitRect, "split_toggle"});
        sy += 22;
    }
    sidebarLine("Applied when loading .mid via LOAD", true);

    sidebarLine("");
    sidebarLine("INSTRUMENTS");
    const int projectInstrumentCount = static_cast<int>(snap.editor.instruments.size());
    if (projectInstrumentCount > 0 && context.armedInstrument >= 0 && context.armedInstrument < projectInstrumentCount) {
        const InstrumentSummary& armed = snap.editor.instruments[static_cast<std::size_t>(context.armedInstrument)];
        std::ostringstream armedLine;
        armedLine << "ARMED " << (context.armedInstrument < 10 ? "0" : "") << context.armedInstrument
                  << "  " << instrumentCategoryFromName(armed.name) << "  " << armed.name;
        sidebarLine(armedLine.str());
        sidebarLine("Next painted note target: " + describeInstrument(context.armedInstrument), true);
    } else {
        sidebarLine("ARMED -- none", true);
        sidebarLine("Open browser to add one.", true);
    }

    const int controlsY = sy;
    const int controlHeight = 20;
    const int controlRowY = controlsY + 4;
    const UiRect instPrevRect {context.sidebarLeft + 8, controlRowY, 26, controlHeight};
    const UiRect instNextRect {context.sidebarLeft + 38, controlRowY, 26, controlHeight};
    const UiRect instAudRect {context.sidebarLeft + 68, controlRowY, 56, controlHeight};
    const UiRect instBrowseRect {context.sidebarLeft + 128, controlRowY, 66, controlHeight};
    context.drawButton(instPrevRect, "<", false);
    context.drawButton(instNextRect, ">", false);
    context.drawButton(instAudRect, "AUD", false);
    context.drawButton(instBrowseRect, "BROWSE", false);
    context.instrumentControlHits.push_back({instPrevRect, "prev"});
    context.instrumentControlHits.push_back({instNextRect, "next"});
    context.instrumentControlHits.push_back({instAudRect, "audition"});
    context.instrumentControlHits.push_back({instBrowseRect, "browse"});

    sy = controlsY + controlHeight + 8;
    sidebarLine("Use controls: [< >] arm | AUD audition | BROWSE pick");
    sidebarLine("Use Alt+0..9 direct or Ctrl+Shift+I");
    const UiRect instLoadPatchRect {context.sidebarLeft + 8, sy - 12, 112, 20};
    const UiRect instAddPatchRect {context.sidebarLeft + 128, sy - 12, 112, 20};
    context.drawButton(instLoadPatchRect, "LOAD PATCH", false);
    context.drawButton(instAddPatchRect, "ADD PATCH", false);
    context.instrumentControlHits.push_back({instLoadPatchRect, "load_patch"});
    context.instrumentControlHits.push_back({instAddPatchRect, "add_patch"});
    sy += 22;
    sidebarLine("LOAD replaces armed slot | ADD imports new", true);

    sidebarLine("");
    const int listRows = std::max(3, (context.gridTop + context.gridHeight - sy - 142) / 16);
    context.instrumentListVisibleRows = listRows;
    context.clampInstrumentListWindow();
    std::vector<std::pair<int, std::string>> orderedInstrumentRows;
    orderedInstrumentRows.reserve(projectInstrumentCount);
    for (int index = 0; index < projectInstrumentCount; ++index) {
        const InstrumentSummary& instrument = snap.editor.instruments[static_cast<std::size_t>(index)];
        orderedInstrumentRows.push_back({index, instrumentCategoryFromName(instrument.name)});
    }
    std::sort(orderedInstrumentRows.begin(), orderedInstrumentRows.end(), [](const auto& left, const auto& right) {
        const int leftRank = instrumentCategoryRank(left.second);
        const int rightRank = instrumentCategoryRank(right.second);
        if (leftRank != rightRank) {
            return leftRank < rightRank;
        }
        if (left.second != right.second) {
            return left.second < right.second;
        }
        return left.first < right.first;
    });
    const int instrumentCount = static_cast<int>(orderedInstrumentRows.size());
    if (instrumentCount > 0) {
        const int viewStart = std::clamp(context.instrumentListStart, 0, std::max(0, instrumentCount - 1));
        const int viewEnd = std::min(instrumentCount, viewStart + listRows);
        std::ostringstream range;
        range << (viewStart + 1) << "-" << viewEnd << " / " << instrumentCount;
        context.drawText(context.sidebarLeft + 214, sy, range.str(), context.colorMutedText);
    } else {
        context.drawText(context.sidebarLeft + 214, sy, "0 instruments", context.colorMutedText);
    }
    const int listTop = sy + 4;
    const int listHeight = listRows * 16;
    context.instrumentListRect = UiRect {context.sidebarLeft + 6, listTop, context.sidebarWidth - 14, listHeight};

    const int viewStart = std::clamp(context.instrumentListStart, 0, std::max(0, instrumentCount - 1));
    const int viewEnd = std::min(instrumentCount, viewStart + listRows);
    const int rowHeight = 16;
    const int firstRowY = listTop + 2;
    for (int index = viewStart; index < viewEnd; ++index) {
        const int rowOffset = index - viewStart;
        const int rowY = firstRowY + (rowOffset * rowHeight);
        const int instrumentIndex = orderedInstrumentRows[static_cast<std::size_t>(index)].first;
        const InstrumentSummary& instrument = snap.editor.instruments[static_cast<std::size_t>(instrumentIndex)];
        const std::string section = orderedInstrumentRows[static_cast<std::size_t>(index)].second;
        int noteUseCount = 0;
        const bool active = instrument.active;
        noteUseCount = instrument.noteUseCount;
        const bool isArmed = instrumentIndex == context.armedInstrument;
        const UiRect hit {context.sidebarLeft + 6, rowY, context.sidebarWidth - 14, rowHeight};
        if (isArmed || active) {
            context.drawFilledRect(hit.x, hit.y + 1, hit.width, hit.height - 2, context.colorSelection);
        }
        std::ostringstream line;
        line << (isArmed ? "> " : "  ")
             << "[" << instrumentCategoryShortLabel(section) << "] "
             << (active ? "[R]" : "   ")
             << "  "
             << (instrumentIndex < 10 ? "0" : "") << instrumentIndex
             << " "
             << instrument.name
             << "  "
             << instrumentCategoryFromName(instrument.name)
             << "  "
             << noteUseCount
             << "x";
        context.drawText(
            context.sidebarLeft + 8,
            rowY,
            line.str(),
            instrumentIndex == context.armedInstrument ? context.colorText : context.colorMutedText);
        context.instrumentHitTargets.push_back({hit, instrumentIndex});
    }

    sy += (viewEnd - viewStart) * rowHeight;
    if (instrumentCount > viewEnd) {
        context.drawText(context.sidebarLeft + 8, sy, "...", context.colorMutedText);
        sy += 16;
    }
    sidebarLine("Keys: [ / ] prev-next | Ctrl+Up/Down cycle", true);
    sidebarLine("Ctrl+Shift+I browser | Alt+0..9 direct | \\ audition", true);

    sidebarLine("");
    sidebarLine("LAST ACTION");
    if (!context.lastAction.actionId.empty()) {
        sidebarLine(context.lastAction.actionId, true);
        sidebarLine(context.lastAction.ok ? "ok" : "error");
        if (!context.lastAction.error.empty()) {
            sidebarLine(context.lastAction.error, true);
        } else if (!context.lastAction.message.empty()) {
            sidebarLine(context.lastAction.message, true);
        }
    } else {
        sidebarLine("<none>", true);
    }
    if (context.lastAction.hasMidiImportReport) {
        const MidiImportReport& report = context.lastAction.midiImportReport;
        sidebarLine("");
        sidebarLine("MIDI IMPORT");
        {
            std::ostringstream summary;
            summary << "Fmt " << report.midiFormat << "  TPQ " << report.ticksPerQuarterNote;
            sidebarLine(summary.str(), true);
        }
        {
            std::ostringstream counts;
            counts << "Tracks " << report.importedTrackCount
                   << " Inst " << report.trackMappings.size()
                   << " Notes " << report.importedNoteCount;
            sidebarLine(counts.str(), true);
        }
        const int mappingCount = static_cast<int>(report.trackMappings.size());
        const int shown = std::min(6, mappingCount);
        for (int index = 0; index < shown; ++index) {
            const MidiImportTrackMapping& mapping = report.trackMappings[static_cast<std::size_t>(index)];
            std::ostringstream line;
            line << "T" << mapping.trackIndex
                 << ">" << "I" << mapping.instrumentIndex
                 << " ch" << mapping.midiChannel
                 << " pg" << mapping.dominantProgram;
            sidebarLine(line.str(), true);
        }
        if (mappingCount > shown) {
            sidebarLine("... +" + std::to_string(mappingCount - shown) + " lanes", true);
        }
    }
    sidebarLine("");
    sidebarLine("MESSAGE");
    sidebarLine("[" + std::string(appMessageSeverityName(snap.lastMessage.severity)) + "]", true);
    sidebarLine(snap.lastMessage.text);
    return sy;
}

} // namespace arachno
