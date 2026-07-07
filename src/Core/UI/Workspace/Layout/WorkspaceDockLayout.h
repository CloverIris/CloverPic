#pragma once

#include "Core/UI/Workspace/WorkspaceUiState.h"
#include "Utils/Types.h"
#include <vector>

namespace CloverPic::Core {

struct WorkspacePanelComputedLayout {
    WorkspacePanelId panelId = WorkspacePanelId::Color;
    WorkspaceDockSide side = WorkspaceDockSide::Left;
    Rect rect;
    Rect headerRect;
    Rect contentRect;
    int zOrder = 0;
    bool floating = false;
};

struct WorkspaceDockLayoutResult {
    Rect menuBarRect;
    Rect commandBarRect;
    Rect toolRailRect;
    Rect leftDockRect;
    Rect rightDockRect;
    Rect canvasRect;
    Rect statusBarRect;
    Rect leftToggleRect;
    Rect rightToggleRect;
    Rect leftDockHotZone;
    Rect rightDockHotZone;
    std::vector<WorkspacePanelComputedLayout> panels;

    const WorkspacePanelComputedLayout* FindPanel(WorkspacePanelId panelId) const;
    int InsertIndexForDock(WorkspaceDockSide side, int y) const;
    Rect BuildInsertionRect(WorkspaceDockSide side, int insertIndex) const;
};

class WorkspaceDockLayout {
public:
    static WorkspaceDockLayoutResult Compute(const Size& viewport,
                                             const WorkspaceUiState& uiState,
                                             bool statusBarVisible);
};

} // namespace CloverPic::Core
