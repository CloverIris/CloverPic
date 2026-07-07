#pragma once

#include "Core/App/AppCommandPayload.h"
#include "Utils/Types.h"
#include <vector>

namespace CloverPic::Core {

enum class WorkspaceDockSide : uint8_t {
    Left,
    Right,
    Floating
};

struct WorkspacePanelLayoutState {
    WorkspacePanelId panelId = WorkspacePanelId::Color;
    WorkspaceDockSide dockSide = WorkspaceDockSide::Left;
    int dockOrder = 0;
    Rect floatingRect;
    bool visible = true;
    bool collapsed = false;
    int floatingZ = 0;
};

struct WorkspaceDockInsertionMarker {
    bool visible = false;
    WorkspaceDockSide side = WorkspaceDockSide::Left;
    int insertIndex = 0;
    Rect rect;
};

struct WorkspaceUiState {
    bool leftSidebarExpanded = true;
    bool rightSidebarExpanded = true;
    String openMenu;
    String statusText = L"READY";
    std::vector<WorkspacePanelLayoutState> panels;
    WorkspaceDockInsertionMarker dockInsertion;
    WorkspacePanelId draggedPanelId = WorkspacePanelId::Color;
    bool draggingPanel = false;
    Rect draggedPreviewRect;
    bool layerBlendDropdownOpen = false;
    Rect layerBlendDropdownRect;

    void Reset(const Size& viewport);
    WorkspacePanelLayoutState* FindPanel(WorkspacePanelId panelId);
    const WorkspacePanelLayoutState* FindPanel(WorkspacePanelId panelId) const;
    bool IsPanelVisible(WorkspacePanelId panelId) const;
    void SetPanelVisible(WorkspacePanelId panelId, bool visible);
    void TogglePanel(WorkspacePanelId panelId);
    void NormalizeDockOrder(WorkspaceDockSide side);
    void NormalizeAllDockOrders();
    void BringFloatingPanelToFront(WorkspacePanelId panelId);
    std::vector<WorkspacePanelLayoutState*> PanelsForDock(WorkspaceDockSide side);
    std::vector<const WorkspacePanelLayoutState*> PanelsForDock(WorkspaceDockSide side) const;
};

const wchar_t* PanelTitle(WorkspacePanelId panelId);
WorkspaceDockSide DefaultDockSide(WorkspacePanelId panelId);

} // namespace CloverPic::Core
