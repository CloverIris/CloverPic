#pragma once

#include "Core/UI/Workspace/Modal/WorkspaceModalState.h"
#include "Core/UI/Workspace/Render/WorkspaceNodeRenderer.h"
#include "Core/Presentation/SoftRenderer.h"

namespace CloverPic::Core {

class WorkspaceModalRenderer {
public:
    static void Render(Presentation::SoftRenderer& renderer,
                       const Rect& fullRect,
                       const Size& viewport,
                       const WorkspaceNodeRenderContext& nodeContext,
                       const WorkspaceCanvasSizeState& canvasSizeState);
};

} // namespace CloverPic::Core
