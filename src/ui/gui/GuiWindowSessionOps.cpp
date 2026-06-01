#include "ui/gui/GuiWindowSessionOps.h"

#include "ui/gui/GuiWindowShutdownOps.h"

namespace arachno {

void startupGuiWindowSession(const GuiWindowStartupContext& context) {
    context.runSync("force_snapshot");
    context.refreshSnapshot();
    AppActionRequest request;
    request.actionId = "audio.runtime.start";
    (void)context.runAction(request, false);
    context.ensureTrackerBackbuffer();
    (void)context.midiInput.open();
}

void shutdownGuiWindowSession(const GuiWindowShutdownContext& context) {
    context.closeAudioOutput();
    context.midiInput.close();
    shutdownGuiWindowsAndResources(
        context.display,
        context.uiFont,
        context.releaseTrackerBackbuffer,
        context.releaseSynthBackbuffer,
        context.synthWindow,
        context.synthGc,
        context.gc,
        context.window);
}

} // namespace arachno
