#include "ui/gui/GuiWindowBootstrapOps.h"

namespace arachno {

std::optional<GuiWindowBootstrapResult> bootstrapGuiWindow(
    std::ostream& output,
    const std::string& windowTitle) {
    Display* display = XOpenDisplay(nullptr);
    if (display == nullptr) {
        output << "GUI window unavailable: failed to open X11 display.\n";
        return std::nullopt;
    }

    const int screen = DefaultScreen(display);
    const Window window = XCreateSimpleWindow(
        display,
        RootWindow(display, screen),
        80,
        80,
        1280,
        800,
        1,
        BlackPixel(display, screen),
        WhitePixel(display, screen));
    XStoreName(display, window, windowTitle.c_str());
    XSelectInput(
        display,
        window,
        ExposureMask
            | KeyPressMask
            | StructureNotifyMask
            | ButtonPressMask
            | ButtonReleaseMask
            | PointerMotionMask);
    Atom wmDelete = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wmDelete, 1);
    XMapWindow(display, window);

    GC gc = XCreateGC(display, window, 0, nullptr);
    XSetForeground(display, gc, BlackPixel(display, screen));
    XSetBackground(display, gc, WhitePixel(display, screen));

    const UiThemePalette dosTheme = makeThemePalette(display, screen, GuiThemeMode::Dos);
    const UiThemePalette highContrastTheme = makeThemePalette(display, screen, GuiThemeMode::HighContrast);
    const GuiSynthWindowScopeColors scopeColors = makeSynthScopeColors(display, screen);

    XFontStruct* uiFont = XLoadQueryFont(display, "fixed");
    if (uiFont == nullptr) {
        uiFont = XLoadQueryFont(display, "9x15bold");
    }
    if (uiFont != nullptr) {
        XSetFont(display, gc, uiFont->fid);
    }

    return GuiWindowBootstrapResult {
        display,
        screen,
        window,
        gc,
        wmDelete,
        uiFont,
        dosTheme,
        highContrastTheme,
        GuiThemeMode::Dos,
        scopeColors};
}

} // namespace arachno
