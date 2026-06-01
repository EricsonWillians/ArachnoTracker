#include "ui/gui/GuiThemePaletteOps.h"

namespace arachno {

unsigned long allocateNamedColor(Display* display, int screen, const char* name, unsigned long fallback) {
    Colormap cmap = DefaultColormap(display, screen);
    XColor exact;
    XColor chosen;
    if (XAllocNamedColor(display, cmap, name, &chosen, &exact) != 0) {
        return chosen.pixel;
    }
    return fallback;
}

namespace {

UiThemePalette makeDosThemePalette(Display* display, int screen) {
    return UiThemePalette {
        allocateNamedColor(display, screen, "#000000", BlackPixel(display, screen)),
        allocateNamedColor(display, screen, "#000000", BlackPixel(display, screen)),
        allocateNamedColor(display, screen, "#001408", BlackPixel(display, screen)),
        allocateNamedColor(display, screen, "#008a00", WhitePixel(display, screen)),
        allocateNamedColor(display, screen, "#04270f", BlackPixel(display, screen)),
        allocateNamedColor(display, screen, "#00c94f", WhitePixel(display, screen)),
        allocateNamedColor(display, screen, "#b8ffb8", WhitePixel(display, screen)),
        allocateNamedColor(display, screen, "#66d680", WhitePixel(display, screen)),
        allocateNamedColor(display, screen, "#083b18", WhitePixel(display, screen)),
        allocateNamedColor(display, screen, "#001400", BlackPixel(display, screen)),
        allocateNamedColor(display, screen, "#00a844", WhitePixel(display, screen)),
        allocateNamedColor(display, screen, "#b8ffb8", WhitePixel(display, screen)),
        allocateNamedColor(display, screen, "#000000", BlackPixel(display, screen)),
        allocateNamedColor(display, screen, "#b8ffb8", WhitePixel(display, screen)),
        allocateNamedColor(display, screen, "#000000", BlackPixel(display, screen)),
        allocateNamedColor(display, screen, "#d4ffd4", WhitePixel(display, screen)),
        allocateNamedColor(display, screen, "#000000", BlackPixel(display, screen)),
        allocateNamedColor(display, screen, "#103810", BlackPixel(display, screen)),
        allocateNamedColor(display, screen, "#000000", BlackPixel(display, screen))};
}

UiThemePalette makeHighContrastThemePalette(Display* display, int screen) {
    return UiThemePalette {
        allocateNamedColor(display, screen, "#000000", BlackPixel(display, screen)),
        allocateNamedColor(display, screen, "#000000", BlackPixel(display, screen)),
        allocateNamedColor(display, screen, "#111111", BlackPixel(display, screen)),
        allocateNamedColor(display, screen, "#ffffff", WhitePixel(display, screen)),
        allocateNamedColor(display, screen, "#222222", BlackPixel(display, screen)),
        allocateNamedColor(display, screen, "#005fcc", WhitePixel(display, screen)),
        allocateNamedColor(display, screen, "#ffffff", WhitePixel(display, screen)),
        allocateNamedColor(display, screen, "#e6e6e6", WhitePixel(display, screen)),
        allocateNamedColor(display, screen, "#0038a8", WhitePixel(display, screen)),
        allocateNamedColor(display, screen, "#1a1a1a", BlackPixel(display, screen)),
        allocateNamedColor(display, screen, "#00a6ff", WhitePixel(display, screen)),
        allocateNamedColor(display, screen, "#ffffff", WhitePixel(display, screen)),
        allocateNamedColor(display, screen, "#000000", BlackPixel(display, screen)),
        allocateNamedColor(display, screen, "#ffffff", WhitePixel(display, screen)),
        allocateNamedColor(display, screen, "#000000", BlackPixel(display, screen)),
        allocateNamedColor(display, screen, "#ffffff", WhitePixel(display, screen)),
        allocateNamedColor(display, screen, "#000000", BlackPixel(display, screen)),
        allocateNamedColor(display, screen, "#ffffff", WhitePixel(display, screen)),
        allocateNamedColor(display, screen, "#000000", BlackPixel(display, screen))};
}

} // namespace

UiThemePalette makeThemePalette(Display* display, int screen, GuiThemeMode mode) {
    if (mode == GuiThemeMode::HighContrast) {
        return makeHighContrastThemePalette(display, screen);
    }
    return makeDosThemePalette(display, screen);
}

GuiSynthWindowScopeColors makeSynthScopeColors(Display* display, int screen) {
    return GuiSynthWindowScopeColors {
        allocateNamedColor(display, screen, "#00e7ff", WhitePixel(display, screen)),
        allocateNamedColor(display, screen, "#7ffaff", WhitePixel(display, screen)),
        allocateNamedColor(display, screen, "#008ea3", WhitePixel(display, screen)),
        allocateNamedColor(display, screen, "#00e7ff", WhitePixel(display, screen)),
        allocateNamedColor(display, screen, "#ffd24d", WhitePixel(display, screen)),
        allocateNamedColor(display, screen, "#ff6ac1", WhitePixel(display, screen)),
        allocateNamedColor(display, screen, "#8dff5f", WhitePixel(display, screen)),
        allocateNamedColor(display, screen, "#ff9448", WhitePixel(display, screen)),
        allocateNamedColor(display, screen, "#7ca8ff", WhitePixel(display, screen))};
}

} // namespace arachno
