#include "ProjectLifecycle.h"

#include <filesystem>

namespace arachno {

namespace {
std::string projectDisplayName(const ApplicationSession& session) {
    if (session.projectPath().empty()) {
        return "Untitled project";
    }
    const std::filesystem::path path(session.projectPath());
    const std::string filename = path.filename().string();
    return filename.empty() ? session.projectPath() : filename;
}

const char* actionVerb(ProjectLifecycleAction action) {
    switch (action) {
        case ProjectLifecycleAction::NewProject:
            return "create a new project";
        case ProjectLifecycleAction::OpenProject:
            return "open another project";
        case ProjectLifecycleAction::CloseProject:
            return "close this project";
        case ProjectLifecycleAction::QuitApplication:
            return "quit ArachnoTracker";
        case ProjectLifecycleAction::RestoreRecovery:
            return "restore a recovery snapshot";
    }
    return "continue";
}

std::string readyMessage(ProjectLifecycleAction action) {
    return std::string("Ready to ") + actionVerb(action);
}
} // namespace

const char* projectLifecycleActionName(ProjectLifecycleAction action) {
    switch (action) {
        case ProjectLifecycleAction::NewProject:
            return "new-project";
        case ProjectLifecycleAction::OpenProject:
            return "open-project";
        case ProjectLifecycleAction::CloseProject:
            return "close-project";
        case ProjectLifecycleAction::QuitApplication:
            return "quit-application";
        case ProjectLifecycleAction::RestoreRecovery:
            return "restore-recovery";
    }
    return "unknown";
}

const char* unsavedChangesChoiceName(UnsavedChangesChoice choice) {
    switch (choice) {
        case UnsavedChangesChoice::NotNeeded:
            return "not-needed";
        case UnsavedChangesChoice::Save:
            return "save";
        case UnsavedChangesChoice::Discard:
            return "discard";
        case UnsavedChangesChoice::Cancel:
            return "cancel";
    }
    return "unknown";
}

UnsavedChangesPrompt buildUnsavedChangesPrompt(
    const ApplicationSession& session,
    ProjectLifecycleAction action) {
    UnsavedChangesPrompt prompt;
    prompt.required = session.dirty();
    prompt.action = action;
    if (!prompt.required) {
        return prompt;
    }

    const std::string displayName = projectDisplayName(session);
    prompt.title = "Save changes before continuing?";
    prompt.message = displayName + " has unsaved changes.";
    prompt.detail = "Save the current project before you " + std::string(actionVerb(action)) + ".";
    prompt.saveLabel = "Save";
    prompt.discardLabel = "Discard Changes";
    prompt.cancelLabel = "Cancel";
    return prompt;
}

ProjectOpenPreflight preflightOpenProject(
    const std::string& path,
    const std::string& recoveryDirectory) {
    ProjectOpenPreflight preflight;
    preflight.path = path;
    preflight.compatibility = inspectProjectFile(path);
    preflight.canOpen = preflight.compatibility.compatible && preflight.compatibility.loadable;
    if (!preflight.canOpen) {
        preflight.error = preflight.compatibility.error.empty()
            ? "project file cannot be opened"
            : preflight.compatibility.error;
    }

    const std::string recoveryPath = recoveryPathForProject(path, recoveryDirectory);
    preflight.recovery = inspectRecoveryFile(recoveryPath);
    preflight.shouldOfferRecovery = preflight.recovery.exists && preflight.recovery.loadable;

    if (preflight.canOpen && preflight.shouldOfferRecovery) {
        preflight.message = "Project can be opened; recovery snapshot is available";
    } else if (preflight.canOpen) {
        preflight.message = "Project can be opened";
    }
    return preflight;
}

ProjectLifecyclePlan planProjectLifecycleTransition(
    const ApplicationSession& session,
    ProjectLifecycleAction action,
    UnsavedChangesChoice choice,
    const std::string& targetPath,
    const std::string& recoveryDirectory) {
    ProjectLifecyclePlan plan;
    plan.action = action;
    plan.choice = choice;
    plan.targetPath = targetPath;
    plan.unsavedPrompt = buildUnsavedChangesPrompt(session, action);
    plan.requiresUnsavedDecision = plan.unsavedPrompt.required;

    if (action == ProjectLifecycleAction::OpenProject) {
        if (targetPath.empty()) {
            plan.error = "target project path is empty";
            return plan;
        }
        plan.openPreflight = preflightOpenProject(targetPath, recoveryDirectory);
        plan.shouldOfferRecovery = plan.openPreflight.shouldOfferRecovery;
        if (!plan.openPreflight.canOpen) {
            plan.error = plan.openPreflight.error;
            return plan;
        }
    }

    if (plan.requiresUnsavedDecision) {
        if (choice == UnsavedChangesChoice::NotNeeded) {
            plan.message = "Waiting for unsaved-changes decision";
            return plan;
        }
        if (choice == UnsavedChangesChoice::Cancel) {
            plan.message = "Canceled by user";
            return plan;
        }
        if (choice == UnsavedChangesChoice::Save) {
            plan.shouldSaveBeforeProceeding = true;
            if (session.projectPath().empty()) {
                plan.requiresSaveAs = true;
                plan.message = "Save As is required before continuing";
                return plan;
            }
        }
        if (choice == UnsavedChangesChoice::Discard) {
            plan.shouldDiscardChanges = true;
        }
    }

    plan.canProceed = true;
    plan.message = readyMessage(action);
    return plan;
}

} // namespace arachno
