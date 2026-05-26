#pragma once

#include <istream>
#include <ostream>
#include <string>

#include "ApplicationSession.h"

namespace arachno {

enum class GuiFrontend {
    Auto,
    X11Window,
    Shell
};

struct GuiShellOptions {
    std::string checkpointName = "gui-main";
    bool createCheckpointIfMissing = true;
    int maxEvents = 64;
    int snapshotGridStartRow = 0;
    int snapshotGridRowCount = 16;
};

int runGuiShell(
    ApplicationSession& session,
    std::istream& input,
    std::ostream& output,
    const GuiShellOptions& options = {});

int runGuiWindow(
    ApplicationSession& session,
    std::ostream& output,
    const GuiShellOptions& options = {});

int runGui(
    ApplicationSession& session,
    std::istream& input,
    std::ostream& output,
    GuiFrontend frontend = GuiFrontend::Auto,
    const GuiShellOptions& options = {});

} // namespace arachno
