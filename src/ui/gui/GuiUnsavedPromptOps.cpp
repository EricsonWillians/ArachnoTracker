#include "ui/gui/GuiUnsavedPromptOps.h"

#include "ui/gui/GuiPathDefaults.h"

namespace arachno {

void resolveUnsavedPromptChoice(const GuiUnsavedPromptResolveContext& context, UnsavedChangesChoice choice) {
    if (!context.unsavedPrompt.active) {
        return;
    }
    AppActionRequest request = context.unsavedPrompt.request;
    context.clearUnsavedPrompt();
    if (choice == UnsavedChangesChoice::Cancel) {
        context.lastAction.ok = true;
        context.lastAction.actionId = request.actionId;
        context.lastAction.message = "action canceled";
        context.lastAction.error.clear();
        return;
    }
    if (choice == UnsavedChangesChoice::Save) {
        const AppSessionSnapshot snap = context.activeSnapshot();
        if (!snap.hasProjectPath || snap.projectPath.empty()) {
            context.hasDeferredPostSaveAction = true;
            context.deferredPostSaveAction = request;
            context.beginInlinePrompt(
                InlinePromptKind::SaveProjectPath,
                "Save project before continue",
                "Path to save current project",
                defaultProjectPath(snap.hasProjectPath, snap.projectPath),
                -1,
                -1);
            return;
        }
    }
    request.unsavedChoice = choice;
    context.runLifecycleAction(request);
}

} // namespace arachno
