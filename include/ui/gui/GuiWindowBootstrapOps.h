#pragma once

#include <optional>
#include <ostream>
#include <string>

#include <X11/Xlib.h>

#include "ui/gui/GuiThemePaletteOps.h"

namespace arachno {

struct GuiWindowBootstrapResult {
    Display* display = nullptr;
    int screen = 0;
    Window window = 0;
    GC gc = nullptr;
    Atom wmDelete = 0;
    XFontStruct* uiFont = nullptr;
    UiThemePalette dosTheme;
    UiThemePalette highContrastTheme;
    GuiThemeMode themeMode = GuiThemeMode::Dos;
    GuiSynthWindowScopeColors scopeColors;
};

std::optional<GuiWindowBootstrapResult> bootstrapGuiWindow(
    std::ostream& output,
    const std::string& windowTitle);

} // namespace arachno
