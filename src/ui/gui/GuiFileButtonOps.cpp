#include "ui/gui/GuiFileButtonOps.h"

#include "ui/gui/GuiPathDefaults.h"

namespace arachno {

namespace {

AppActionRequest makeActionRequest(const std::string& actionId) {
    AppActionRequest request;
    request.actionId = actionId;
    return request;
}

} // namespace

void runFileButtonAction(const GuiFileButtonContext& context, const std::string& actionId) {
    const AppSessionSnapshot snap = context.activeSnapshot();
    if (actionId == "synth.window.toggle") {
        context.toggleSynthWindow();
        return;
    }
    if (actionId == "audio.tuning.toggle") {
        context.audioTuningDialogActive = !context.audioTuningDialogActive;
        return;
    }
    if (actionId == "project.new") {
        context.audioTuningDialogActive = false;
        context.runLifecycleAction(makeActionRequest("project.new"));
        return;
    }
    if (actionId == "project.save") {
        context.audioTuningDialogActive = false;
        if (snap.hasProjectPath) {
            context.runAction(makeActionRequest("project.save"));
        } else {
            context.beginInlinePrompt(
                InlinePromptKind::SaveProjectPath,
                "Save project",
                "Path to save project",
                defaultProjectPath(snap.hasProjectPath, snap.projectPath),
                -1,
                -1);
        }
        return;
    }
    if (actionId == "project.open") {
        context.audioTuningDialogActive = false;
        context.beginInlinePrompt(
            InlinePromptKind::OpenProjectPath,
            "Load project or MIDI",
            "Path to .arachno or .mid/.midi file",
            defaultProjectPath(snap.hasProjectPath, snap.projectPath),
            -1,
            -1);
        return;
    }
    if (actionId == "export.mixdown") {
        context.audioTuningDialogActive = false;
        context.beginInlinePrompt(
            InlinePromptKind::ExportMixdownPath,
            "Export audio or MIDI",
            "Output file (.wav/.mp3/.ogg/.mid)",
            defaultMixdownPath(snap.hasProjectPath, snap.projectPath),
            -1,
            -1);
        return;
    }
    if (actionId == "import.midi") {
        context.audioTuningDialogActive = false;
        context.beginInlinePrompt(
            InlinePromptKind::ImportMidiPath,
            "Import MIDI file",
            "Path to .mid/.midi file (uses sidebar MIDI import preset)",
            defaultMidiImportPath(snap.hasProjectPath, snap.projectPath),
            -1,
            -1);
        return;
    }
}

} // namespace arachno
