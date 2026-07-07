#pragma once

#include "Core/App/CanvasController.h"
#include "Core/Document/EditorSession.h"
#include <functional>

namespace CloverPic::Core {

struct WorkspaceActionContext {
    EditorSession* session = nullptr;
    CanvasController* canvas = nullptr;
    bool* wantsProgramManager = nullptr;
    bool* wantsProgramManagerSettings = nullptr;
};

struct WorkspaceActionCallbacks {
    std::function<void(const String&)> setStatus;
    std::function<void()> touchWorkspace;
    std::function<void()> resizeCanvasViewport;
};

class WorkspaceActionController {
public:
    static bool SaveProject(bool saveAs,
                            WorkspaceActionContext& context,
                            const WorkspaceActionCallbacks& callbacks);

    static void ExportPng(WorkspaceActionContext& context,
                          const WorkspaceActionCallbacks& callbacks);

    static bool ConfirmAbandonUnsavedChanges(const String& actionLabel,
                                             WorkspaceActionContext& context,
                                             const WorkspaceActionCallbacks& callbacks);

    static void RequestProgramManager(WorkspaceActionContext& context,
                                      const WorkspaceActionCallbacks& callbacks);

    static void RequestProgramManagerSettings(WorkspaceActionContext& context,
                                              const WorkspaceActionCallbacks& callbacks);

    static bool OpenProjectInteractive(WorkspaceActionContext& context,
                                       const WorkspaceActionCallbacks& callbacks);

    static bool OpenProjectPath(const String& path,
                                WorkspaceActionContext& context,
                                const WorkspaceActionCallbacks& callbacks);

    static bool CloseWorkspaceProject(WorkspaceActionContext& context,
                                      const WorkspaceActionCallbacks& callbacks);
};

} // namespace CloverPic::Core
