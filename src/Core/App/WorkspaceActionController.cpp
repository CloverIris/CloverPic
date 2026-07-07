#include "Core/App/WorkspaceActionController.h"
#include "Core/Services/CoreServices.h"

namespace CloverPic::Core {

bool WorkspaceActionController::SaveProject(bool saveAs,
                                            WorkspaceActionContext& context,
                                            const WorkspaceActionCallbacks& callbacks) {
    if (!context.session || !context.canvas) {
        return false;
    }
    if (!context.session->HasProject()) {
        callbacks.setStatus(L"NO PROJECT TO SAVE");
        return false;
    }

    bool ok = false;
    if (saveAs || context.session->GetCurrentFilePath().empty()) {
        auto* dialogs = CoreServices::GetFileDialogService();
        if (!dialogs) {
            return false;
        }
        String path = context.session->GetCurrentFilePath();
        if (!dialogs->PickSaveProjectPath(path)) {
            return false;
        }
        ok = context.session->SaveProjectAs(path, context.canvas->GetLayerManager());
    } else {
        ok = context.session->SaveProject(context.canvas->GetLayerManager());
    }
    callbacks.setStatus(ok ? L"SAVED" : L"SAVE FAILED");
    return ok;
}

void WorkspaceActionController::ExportPng(WorkspaceActionContext& context,
                                          const WorkspaceActionCallbacks& callbacks) {
    if (!context.session || !context.canvas) {
        return;
    }
    if (!context.session->HasProject()) {
        callbacks.setStatus(L"NO PROJECT TO EXPORT");
        return;
    }

    auto* dialogs = CoreServices::GetFileDialogService();
    if (!dialogs) {
        return;
    }
    String path;
    if (!dialogs->PickExportImagePath(path)) {
        return;
    }
    callbacks.setStatus(context.session->ExportPNG(path, context.canvas->GetLayerManager()) ? L"EXPORTED" : L"EXPORT FAILED");
}

bool WorkspaceActionController::ConfirmAbandonUnsavedChanges(const String& actionLabel,
                                                             WorkspaceActionContext& context,
                                                             const WorkspaceActionCallbacks& callbacks) {
    if (!context.session || !context.canvas) {
        return false;
    }
    if (!context.session->HasProject() || !context.session->HasUnsavedChanges()) {
        return true;
    }

    auto* dialogs = CoreServices::GetFileDialogService();
    if (!dialogs) {
        callbacks.setStatus(L"UNSAVED CHANGES");
        return false;
    }

    switch (dialogs->PromptUnsavedChanges(actionLabel)) {
        case UnsavedChangesChoice::Save:
            if (SaveProject(false, context, callbacks)) {
                return true;
            }
            callbacks.setStatus(L"SAVE CANCELLED");
            return false;
        case UnsavedChangesChoice::Discard:
            return true;
        case UnsavedChangesChoice::Cancel:
        default:
            break;
    }

    callbacks.setStatus(L"CANCELLED");
    return false;
}

void WorkspaceActionController::RequestProgramManager(WorkspaceActionContext& context,
                                                      const WorkspaceActionCallbacks& callbacks) {
    if (context.wantsProgramManager) {
        *context.wantsProgramManager = true;
    }
    callbacks.touchWorkspace();
}

void WorkspaceActionController::RequestProgramManagerSettings(WorkspaceActionContext& context,
                                                              const WorkspaceActionCallbacks& callbacks) {
    if (context.wantsProgramManager) {
        *context.wantsProgramManager = true;
    }
    if (context.wantsProgramManagerSettings) {
        *context.wantsProgramManagerSettings = true;
    }
    callbacks.touchWorkspace();
}

bool WorkspaceActionController::OpenProjectInteractive(WorkspaceActionContext& context,
                                                       const WorkspaceActionCallbacks& callbacks) {
    if (!context.session || !context.canvas) {
        return false;
    }
    if (!ConfirmAbandonUnsavedChanges(L"opening another project", context, callbacks)) {
        return false;
    }

    auto* dialogs = CoreServices::GetFileDialogService();
    if (!dialogs) {
        return false;
    }
    String path;
    if (!dialogs->PickOpenProjectPath(path) || path.empty()) {
        return false;
    }
    return OpenProjectPath(path, context, callbacks);
}

bool WorkspaceActionController::OpenProjectPath(const String& path,
                                                WorkspaceActionContext& context,
                                                const WorkspaceActionCallbacks& callbacks) {
    if (!context.session || !context.canvas || path.empty()) {
        return false;
    }
    if (context.session->OpenProject(path, context.canvas->GetLayerManager())) {
        context.canvas->AttachLoadedProject(context.session->GetProject());
        callbacks.resizeCanvasViewport();
        callbacks.setStatus(L"OPENED");
        return true;
    }
    callbacks.setStatus(L"OPEN FAILED");
    return false;
}

bool WorkspaceActionController::CloseWorkspaceProject(WorkspaceActionContext& context,
                                                      const WorkspaceActionCallbacks& callbacks) {
    if (!context.session || !context.canvas) {
        return false;
    }
    if (!ConfirmAbandonUnsavedChanges(L"closing the current project", context, callbacks)) {
        return false;
    }
    context.canvas->CloseProject();
    context.session->AttachProject(nullptr);
    if (context.wantsProgramManager) {
        *context.wantsProgramManager = true;
    }
    callbacks.setStatus(L"NO PROJECT");
    return true;
}

} // namespace CloverPic::Core
