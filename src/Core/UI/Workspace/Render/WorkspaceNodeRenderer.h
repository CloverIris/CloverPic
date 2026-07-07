#pragma once

#include "Core/App/ModalManager.h"
#include "Core/App/WorkspaceEditorFacade.h"
#include "Core/UI/Workspace/Layout/WorkspaceDockLayout.h"
#include "Core/UI/Workspace/WorkspaceUiState.h"
#include "Core/Presentation/SoftRenderer.h"
#include "Core/UI/Scene/UiScene.h"

namespace CloverPic::Core {

struct WorkspaceNodeRenderContext {
    const CoreUI::UiScene& scene;
    const WorkspaceUiState& uiState;
    WorkspaceEditorFacade& editor;
    ModalKind modalKind = ModalKind::None;
    uint64_t activeCanvasAnchor = 0;
};

class WorkspaceNodeRenderer {
public:
    static void RenderNode(Presentation::SoftRenderer& renderer,
                           const CoreUI::UiNode& node,
                           const WorkspaceNodeRenderContext& context,
                           bool active = false);

    static void RenderPanelChrome(Presentation::SoftRenderer& renderer,
                                  const WorkspacePanelComputedLayout& panel,
                                  const WorkspaceNodeRenderContext& context);

    static void RenderPanelDecorations(Presentation::SoftRenderer& renderer,
                                       const WorkspaceDockLayoutResult& layout,
                                       const WorkspaceNodeRenderContext& context);
};

} // namespace CloverPic::Core
