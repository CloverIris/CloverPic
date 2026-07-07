#pragma once

#include "Core/App/WorkspaceEditorFacade.h"
#include "Core/UI/Workspace/Layout/WorkspaceDockLayout.h"
#include "Core/UI/Workspace/WorkspaceUiState.h"
#include "Core/UI/Scene/UiScene.h"

namespace CloverPic::Core {

class WorkspaceComponentBuilder {
public:
    static void Build(const Size& viewport,
                      WorkspaceEditorFacade& editor,
                      const WorkspaceUiState& uiState,
                      const WorkspaceDockLayoutResult& layout,
                      CoreUI::UiScene& scene);
};

} // namespace CloverPic::Core
