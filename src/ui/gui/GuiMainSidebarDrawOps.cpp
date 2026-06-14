#include "ui/gui/GuiMainSidebarDrawOps.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

#include "ui/gui/GuiInstrumentBrowserOps.h"
#include "Note.h"

namespace arachno {

namespace {

std::string instrumentCategoryFromName(const std::string& name) {
    const std::string lowered = [&name]() {
        std::string value = name;
        for (char& ch : value) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        return value;
    }();
    const auto token = [&](const std::string& value) {
        return lowered.find(value) != std::string::npos;
    };
    if (token("kick") || token("snare") || token("hat") || token("tom") || token("crash") || token("ride")) {
        return "Percussion";
    }
    if (token("bass") || token("sub") || token("ebm") || token("reese")) {
        return "Bass";
    }
    if (token("lead") || token("solo") || token("arp") || token("pluck")) {
        return "Lead";
    }
    if (token("pad") || token("drone") || token("stabs") || token("soundscape") || token("choir") || token("bell")) {
        return "Pads";
    }
    if (token("clap") || token("shaker") || token("noise")) {
        return "FX";
    }
    if (token("guitar")) {
        return "Guitar";
    }
    if (token("strings") || token("violin") || token("orchestra")) {
        return "Strings";
    }
    if (token("brass") || token("trumpet") || token("sax") || token("horn")) {
        return "Brass";
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

    sidebarLine("ACTIVE STEP");
    std::ostringstream step;
    step << "Row " << snap.editor.activeStep.row << " Track " << snap.editor.activeStep.trackName;
    sidebarLine(step.str(), true);
    sidebarLine(
        snap.editor.activeStep.hasNote
            ? ("Note " + snap.editor.activeStep.noteName + "  Inst "
                + std::to_string(snap.editor.activeStep.instrument))
            : "Note ---  Inst --");
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
        const int baseMidi = std::clamp(context.armedOctave * 12, 0, 115);
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
    sidebarLine("MIDI IMPORT PRESET");
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
        const UiRect splitRect {context.sidebarLeft + 8, buttonY, 116, 20};
        const UiRect importRect {context.sidebarLeft + 128, buttonY, 116, 20};
        context.drawButton(
            splitRect,
            context.midiImportSplitByTrack ? "SPLIT TRACKS" : "MERGE LANES",
            context.midiImportSplitByTrack);
        context.drawButton(importRect, "IMPORT MIDI", false);
        context.midiImportSettingHits.push_back({splitRect, "split_toggle"});
        context.midiImportSettingHits.push_back({importRect, "import"});
        sy += 22;
    }
    sidebarLine("Use Ctrl+M to open picker", true);

    sidebarLine("");
    sidebarLine("INSTRUMENTS");
    const int listRows = std::max(3, (context.gridTop + context.gridHeight - sy - 142) / 16);
    context.instrumentListVisibleRows = listRows;
    context.clampInstrumentListWindow();
    const std::vector<int> orderedInstrumentIndices = filteredInstrumentIndicesForQuery(snap, "");
    const int instrumentCount = static_cast<int>(orderedInstrumentIndices.size());
    const UiRect instPrevRect {context.sidebarLeft + 8, sy - 12, 28, 20};
    const UiRect instNextRect {context.sidebarLeft + 40, sy - 12, 28, 20};
    const UiRect instAudRect {context.sidebarLeft + 72, sy - 12, 48, 20};
    const UiRect instBrowseRect {context.sidebarLeft + 124, sy - 12, 84, 20};
    context.drawButton(instPrevRect, "<", false);
    context.drawButton(instNextRect, ">", false);
    context.drawButton(instAudRect, "AUD", false);
    context.drawButton(instBrowseRect, "BROWSE", false);
    context.instrumentControlHits.push_back({instPrevRect, "prev"});
    context.instrumentControlHits.push_back({instNextRect, "next"});
    context.instrumentControlHits.push_back({instAudRect, "audition"});
    context.instrumentControlHits.push_back({instBrowseRect, "browse"});
    if (instrumentCount > 0) {
        const int viewStart = std::clamp(context.instrumentListStart, 0, std::max(0, instrumentCount - 1));
        const int viewEnd = std::min(instrumentCount, viewStart + listRows);
        std::ostringstream range;
        range << (viewStart + 1) << "-" << viewEnd << " / " << instrumentCount;
        context.drawText(context.sidebarLeft + 214, sy, range.str(), context.colorMutedText);
    } else {
        context.drawText(context.sidebarLeft + 214, sy, "0 instruments", context.colorMutedText);
    }
    sy += 20;
    const int listTop = sy - 12;
    const int listHeight = listRows * 16;
    context.instrumentListRect = UiRect {context.sidebarLeft + 6, listTop, context.sidebarWidth - 14, listHeight};

    const int viewStart = std::clamp(context.instrumentListStart, 0, std::max(0, instrumentCount - 1));
    const int viewEnd = std::min(instrumentCount, viewStart + listRows);
    for (int index = viewStart; index < viewEnd; ++index) {
        const int instrumentIndex = orderedInstrumentIndices[static_cast<std::size_t>(index)];
        const bool isStandardMidi = isStandardMidiBrowserIndex(instrumentIndex);
        std::string category;
        std::string name;
        int noteUseCount = 0;
        bool active = false;
        if (isStandardMidi) {
            category = standardMidiBrowserCategory(instrumentIndex);
            name = standardMidiBrowserName(instrumentIndex);
        } else {
            const InstrumentSummary& instrument = snap.editor.instruments[static_cast<std::size_t>(instrumentIndex)];
            category = instrumentCategoryFromName(instrument.name);
            name = instrument.name;
            noteUseCount = instrument.noteUseCount;
            active = instrument.active;
        }
        const UiRect hit {context.sidebarLeft + 6, sy - 12, context.sidebarWidth - 14, 16};
        if (instrumentIndex == context.armedInstrument || active) {
            context.drawFilledRect(hit.x, hit.y + 1, hit.width, hit.height - 2, context.colorSelection);
        }
        std::ostringstream line;
        line << (instrumentIndex == context.armedInstrument ? "*" : " ")
             << (active ? ">" : " ")
             << " ["
             << category
             << "] "
             << (instrumentIndex < 10 ? "0" : "") << instrumentIndex
             << " "
             << name
             << " (" << noteUseCount << ")";
        context.drawText(
            context.sidebarLeft + 8,
            sy,
            line.str(),
            instrumentIndex == context.armedInstrument ? context.colorText : context.colorMutedText);
        context.instrumentHitTargets.push_back({hit, instrumentIndex});
        sy += 16;
    }
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
