#include "ui/gui/GuiSynthKeyboardSection.h"

#include <algorithm>
#include <array>

#include "Note.h"

namespace arachno {

int drawSynthKeyboardSection(const GuiSynthKeyboardSectionContext& context) {
    const int keyboardLeft = 16;
    const int whiteKeyWidth = 18;
    const int whiteKeyHeight = 78;
    const int blackKeyWidth = 12;
    const int blackKeyHeight = 48;
    const int infoPaneMinWidth = 300;
    const int keyboardRightBudget = std::max(
        keyboardLeft + 7 * whiteKeyWidth,
        context.panelRight - infoPaneMinWidth - 12);
    const int maxVisibleWhiteKeys = std::max(14, (keyboardRightBudget - keyboardLeft) / whiteKeyWidth);
    const int maxVisibleOctaves = std::max(2, std::min(6, maxVisibleWhiteKeys / 7));
    context.synthKeyboardVisibleOctaves = std::clamp(context.synthKeyboardVisibleOctaves, 2, maxVisibleOctaves);
    const int whiteKeys = context.synthKeyboardVisibleOctaves * 7;
    const int keyboardWidth = whiteKeys * whiteKeyWidth;
    const int maxKeyboardStart = std::max(0, 10 - context.synthKeyboardVisibleOctaves);
    context.synthKeyboardBaseOctave = std::clamp(context.synthKeyboardBaseOctave, 0, maxKeyboardStart);
    const int baseMidi = std::clamp(context.synthKeyboardBaseOctave * 12, 0, 120);
    context.drawText(16, context.keyboardTop - 8, "AUDITION KEYS", context.colors.mutedText);
    const int infoPanelX = keyboardLeft + keyboardWidth + 10;
    const int infoPanelWidth = std::max(140, context.panelRight - infoPanelX);
    const UiRect kbDownRect {infoPanelX + 6, context.keyboardTop - 18, 26, 18};
    const UiRect kbUpRect {infoPanelX + 36, context.keyboardTop - 18, 26, 18};
    const UiRect kbCenterRect {infoPanelX + 66, context.keyboardTop - 18, 48, 18};
    context.drawButton(kbDownRect, "<", false);
    context.drawButton(kbUpRect, ">", false);
    context.drawButton(kbCenterRect, "SYNC", false);
    context.synthWindowHits.push_back({kbDownRect, "kb_octave_down", "", "", "", 0.0});
    context.synthWindowHits.push_back({kbUpRect, "kb_octave_up", "", "", "", 0.0});
    context.synthWindowHits.push_back({kbCenterRect, "kb_octave_sync", "", "", "", 0.0});

    context.drawFilledRect(keyboardLeft, context.keyboardTop, keyboardWidth, whiteKeyHeight, context.colors.button);
    context.drawRect(keyboardLeft, context.keyboardTop, keyboardWidth, whiteKeyHeight, context.colors.gridLine);
    std::array<bool, 128> previewMidiActive {};
    previewMidiActive.fill(false);
    for (int midi : context.previewNotes) {
        if (midi >= 0 && midi <= 127) {
            previewMidiActive[static_cast<std::size_t>(midi)] = true;
        }
    }
    if (context.previewNotes.empty()) {
        previewMidiActive[static_cast<std::size_t>(std::clamp(context.synthPreviewMidi, 0, 127))] = true;
    }
    const std::array<int, 7> whiteSemitones = {0, 2, 4, 5, 7, 9, 11};
    const std::array<int, 5> blackSemitones = {1, 3, 6, 8, 10};
    const std::array<int, 5> blackBeforeWhiteIndex = {0, 1, 3, 4, 5};
    for (int octave = 0; octave < context.synthKeyboardVisibleOctaves; ++octave) {
        const int octaveWhiteOffset = octave * 7;
        const int octaveMidi = baseMidi + (octave * 12);
        for (int degree = 0; degree < 7; ++degree) {
            const int midi = std::clamp(octaveMidi + whiteSemitones[static_cast<std::size_t>(degree)], 0, 127);
            const UiRect keyRect {
                keyboardLeft + ((octaveWhiteOffset + degree) * whiteKeyWidth),
                context.keyboardTop,
                whiteKeyWidth,
                whiteKeyHeight};
            const bool active = previewMidiActive[static_cast<std::size_t>(midi)];
            context.drawFilledRect(
                keyRect.x + 1,
                keyRect.y + 1,
                keyRect.width - 2,
                keyRect.height - 2,
                active ? context.activeKeyColor : context.colors.pianoWhite);
            context.drawRect(keyRect.x, keyRect.y, keyRect.width, keyRect.height, context.colors.gridLine);
            context.synthKeyboardHits.push_back({keyRect, midi, false});
            if (degree == 0) {
                context.drawText(
                    keyRect.x + 2,
                    keyRect.y + whiteKeyHeight - 6,
                    "C" + std::to_string(context.synthKeyboardBaseOctave + octave),
                    context.colors.mutedText);
            }
        }
        for (std::size_t blackIndex = 0; blackIndex < blackSemitones.size(); ++blackIndex) {
            const int midi = std::clamp(octaveMidi + blackSemitones[blackIndex], 0, 127);
            const int whiteIndexBefore = octaveWhiteOffset + blackBeforeWhiteIndex[blackIndex];
            const int boundaryX = keyboardLeft + ((whiteIndexBefore + 1) * whiteKeyWidth);
            const UiRect keyRect {
                boundaryX - (blackKeyWidth / 2),
                context.keyboardTop,
                blackKeyWidth,
                blackKeyHeight};
            const bool active = previewMidiActive[static_cast<std::size_t>(midi)];
            context.drawFilledRect(
                keyRect.x + 1,
                keyRect.y + 1,
                keyRect.width - 2,
                keyRect.height - 2,
                active ? context.activeKeyColor : context.colors.pianoBlack);
            context.drawRect(keyRect.x, keyRect.y, keyRect.width, keyRect.height, context.colors.gridLine);
            context.synthKeyboardHits.push_back({keyRect, midi, true});
        }
    }
    const UiRect infoPanel {infoPanelX, context.keyboardTop, infoPanelWidth, 146};
    context.drawFilledRect(infoPanel.x, infoPanel.y, infoPanel.width, infoPanel.height, context.colors.panel);
    context.drawRect(infoPanel.x, infoPanel.y, infoPanel.width, infoPanel.height, context.colors.gridLine);
    const int infoTextX = infoPanel.x + 6;
    const int infoTextW = std::max(80, infoPanel.width - 12);
    context.drawText(infoTextX, infoPanel.y + 14, "Selected", context.colors.mutedText);
    context.drawText(
        infoTextX,
        infoPanel.y + 30,
        context.fitText(midiNoteName(context.synthPreviewMidi) + " x" + std::to_string(context.previewNotes.size()), infoTextW),
        context.colors.text);
    context.drawText(infoTextX, infoPanel.y + 46, "Range", context.colors.mutedText);
    context.drawText(
        infoTextX,
        infoPanel.y + 62,
        context.fitText(
            "C" + std::to_string(context.synthKeyboardBaseOctave) + "..C"
                + std::to_string(context.synthKeyboardBaseOctave + context.synthKeyboardVisibleOctaves),
            infoTextW),
        context.colors.text);
    context.drawText(infoTextX, infoPanel.y + 78, context.fitText("Mouse click/drag = preview", infoTextW), context.colors.mutedText);
    context.drawText(infoTextX, infoPanel.y + 94, context.fitText("Z..M/Q..U + Enter/Space", infoTextW), context.colors.mutedText);
    context.drawText(infoTextX, infoPanel.y + 110, context.fitText("Octave: [ ] or - + or Ctrl+0..8", infoTextW), context.colors.mutedText);
    // Show polyphony count and unison info
    std::string polyInfo = "Poly: " + std::to_string(context.previewNotes.size());
    if (context.previewNotes.size() > 1) {
        polyInfo += " notes";
    }
    context.drawText(infoTextX, infoPanel.y + 126, context.fitText(polyInfo, infoTextW), context.colors.text);
    context.drawText(infoTextX, infoPanel.y + 142, context.fitText(context.midiStatusText, infoTextW), context.colors.mutedText);

    return std::max(
        context.keyboardTop + whiteKeyHeight + 22,
        infoPanel.y + infoPanel.height + 24);
}

} // namespace arachno
