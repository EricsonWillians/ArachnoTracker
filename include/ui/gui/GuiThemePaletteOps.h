#pragma once

#include <X11/Xlib.h>

#include "ui/gui/GuiSynthWindowDrawOps.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct UiThemePalette {
    unsigned long background = 0;
    unsigned long panel = 0;
    unsigned long gridHeader = 0;
    unsigned long gridLine = 0;
    unsigned long selection = 0;
    unsigned long cursor = 0;
    unsigned long text = 0;
    unsigned long mutedText = 0;
    unsigned long playhead = 0;
    unsigned long button = 0;
    unsigned long buttonActive = 0;
    unsigned long buttonLabel = 0;
    unsigned long buttonLabelActive = 0;
    unsigned long selectionText = 0;
    unsigned long cursorText = 0;
    unsigned long playheadText = 0;
    unsigned long activeTagText = 0;
    unsigned long pianoWhite = 0;
    unsigned long pianoBlack = 0;
};

unsigned long allocateNamedColor(Display* display, int screen, const char* name, unsigned long fallback);

UiThemePalette makeThemePalette(Display* display, int screen, GuiThemeMode mode);

GuiSynthWindowScopeColors makeSynthScopeColors(Display* display, int screen);

} // namespace arachno
