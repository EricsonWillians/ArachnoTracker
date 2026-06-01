#pragma once

#include <functional>
#include <string>
#include <vector>

#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiSynthKeyboardThemeColors {
    unsigned long button = 0;
    unsigned long gridLine = 0;
    unsigned long pianoWhite = 0;
    unsigned long pianoBlack = 0;
    unsigned long mutedText = 0;
    unsigned long text = 0;
    unsigned long panel = 0;
    unsigned long selection = 0;
};

struct GuiSynthKeyboardSectionContext {
    int panelRight = 0;
    int keyboardTop = 0;
    int synthPreviewMidi = 60;
    int& synthKeyboardBaseOctave;
    int& synthKeyboardVisibleOctaves;
    std::string midiStatusText;
    std::vector<int> previewNotes;
    std::vector<PianoKeyHit>& synthKeyboardHits;
    std::vector<SynthWindowHit>& synthWindowHits;
    GuiSynthKeyboardThemeColors colors;
    unsigned long activeKeyColor = 0;

    std::function<void(int, int, int, int, unsigned long)> drawFilledRect;
    std::function<void(int, int, int, int, unsigned long)> drawRect;
    std::function<void(int, int, const std::string&, unsigned long)> drawText;
    std::function<void(const UiRect&, const std::string&, bool)> drawButton;
    std::function<std::string(const std::string&, int)> fitText;
};

int drawSynthKeyboardSection(const GuiSynthKeyboardSectionContext& context);

} // namespace arachno
