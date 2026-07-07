#pragma once

#include "Core/App/ModalManager.h"
#include "Core/App/WorkspaceEditorFacade.h"
#include "Core/UI/Workspace/Interaction/WorkspaceInteractionController.h"
#include "Core/UI/Workspace/Modal/WorkspaceModalState.h"
#include "Core/Presentation/FrameScheduler.h"
#include "Core/UI/Scene/UiScene.h"
#include <functional>

namespace CloverPic::Core {

struct WorkspacePointerInputContext {
    CoreUI::UiScene* scene = nullptr;
    WorkspaceUiState* uiState = nullptr;
    WorkspaceDockLayoutResult* layout = nullptr;
    WorkspaceInteractionController* interaction = nullptr;
    WorkspaceEditorFacade* editor = nullptr;
    Presentation::FrameScheduler* scheduler = nullptr;
    ModalKind modalKind = ModalKind::None;
    String* openMenu = nullptr;
    WorkspaceCanvasSizeState* canvasSizeState = nullptr;
    Rect canvasRect;
    Rect fullRect;
    Presentation::UiNodeId canvasAnimationRegionId = 0;
};

struct WorkspacePointerInputCallbacks {
    std::function<void(AppCommand, uint64_t, const String&)> dispatch;
    std::function<void(const Rect&)> markDirty;
    std::function<void()> markProjectDirty;
    std::function<void()> rebuildScene;
    std::function<void(const String&)> setStatus;
    std::function<void()> saveWorkspaceUiSettings;
};

class WorkspacePointerInputController {
public:
    static void HandlePointer(const Input::PointerEvent& event,
                              WorkspacePointerInputContext& context,
                              const WorkspacePointerInputCallbacks& callbacks);
};

} // namespace CloverPic::Core
