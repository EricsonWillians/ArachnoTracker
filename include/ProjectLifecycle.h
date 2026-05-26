#pragma once

#include <string>

#include "ApplicationSession.h"
#include "AutoSave.h"
#include "FileCompatibility.h"

namespace arachno {

enum class ProjectLifecycleAction {
    NewProject,
    OpenProject,
    CloseProject,
    QuitApplication,
    RestoreRecovery
};

enum class UnsavedChangesChoice {
    NotNeeded,
    Save,
    Discard,
    Cancel
};

struct UnsavedChangesPrompt {
    bool required = false;
    ProjectLifecycleAction action = ProjectLifecycleAction::OpenProject;
    std::string title;
    std::string message;
    std::string detail;
    std::string saveLabel = "Save";
    std::string discardLabel = "Discard";
    std::string cancelLabel = "Cancel";
};

struct ProjectOpenPreflight {
    std::string path;
    FileCompatibilityReport compatibility;
    RecoveryInfo recovery;
    bool canOpen = false;
    bool shouldOfferRecovery = false;
    std::string message;
    std::string error;
};

struct ProjectLifecyclePlan {
    ProjectLifecycleAction action = ProjectLifecycleAction::OpenProject;
    UnsavedChangesChoice choice = UnsavedChangesChoice::NotNeeded;
    std::string targetPath;
    bool canProceed = false;
    bool requiresUnsavedDecision = false;
    bool shouldSaveBeforeProceeding = false;
    bool requiresSaveAs = false;
    bool shouldDiscardChanges = false;
    bool shouldOfferRecovery = false;
    std::string message;
    std::string error;
    UnsavedChangesPrompt unsavedPrompt;
    ProjectOpenPreflight openPreflight;
};

const char* projectLifecycleActionName(ProjectLifecycleAction action);
const char* unsavedChangesChoiceName(UnsavedChangesChoice choice);
UnsavedChangesPrompt buildUnsavedChangesPrompt(
    const ApplicationSession& session,
    ProjectLifecycleAction action);
ProjectOpenPreflight preflightOpenProject(
    const std::string& path,
    const std::string& recoveryDirectory = "");
ProjectLifecyclePlan planProjectLifecycleTransition(
    const ApplicationSession& session,
    ProjectLifecycleAction action,
    UnsavedChangesChoice choice = UnsavedChangesChoice::NotNeeded,
    const std::string& targetPath = "",
    const std::string& recoveryDirectory = "");

} // namespace arachno
