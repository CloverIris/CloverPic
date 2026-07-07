#pragma once

#include "Core/App/ModalManager.h"
#include "Core/App/WorkspaceEditorFacade.h"
#include "Core/UI/Workspace/Modal/WorkspaceModalState.h"
#include "Core/UI/Scene/UiScene.h"

namespace CloverPic::Core {

class WorkspaceModalBuilder {
public:
    static void Build(ModalKind kind,
                      const Size& viewport,
                      const Rect& fullRect,
                      const WorkspaceEditorFacade& editor,
                      const WorkspaceCanvasSizeState& canvasSizeState,
                      CoreUI::UiScene& scene);
};

} // namespace CloverPic::Core
