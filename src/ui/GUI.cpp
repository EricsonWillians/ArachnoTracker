#include "GUI.h"

namespace arachno {

int runGui(
    ApplicationSession& session,
    std::istream& input,
    std::ostream& output,
    GuiFrontend frontend,
    const GuiShellOptions& options) {
    if (frontend == GuiFrontend::Shell) {
        return runGuiShell(session, input, output, options);
    }
    if (frontend == GuiFrontend::X11Window) {
        return runGuiWindow(session, output, options);
    }
    const int windowResult = runGuiWindow(session, output, options);
    if (windowResult == 2) {
        output << "Falling back to GUI shell.\n";
        return runGuiShell(session, input, output, options);
    }
    return windowResult;
}

} // namespace arachno
