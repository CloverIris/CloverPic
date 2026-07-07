#include "Core/UI/Workspace/Layout/WorkspaceDockLayout.h"
#include "Core/UI/Workspace/WorkspaceLayout.h"
#include <algorithm>

namespace CloverPic::Core {

namespace {

constexpr int PanelGap = 8;
constexpr int PanelHeaderH = 24;
constexpr int PanelFooterMargin = 4;

int DockWidth(WorkspaceDockSide side, const WorkspaceUiState& uiState) {
    if (side == WorkspaceDockSide::Left) {
        return WorkspaceLayout::LeftDockWidth(uiState.leftSidebarExpanded);
    }
    return WorkspaceLayout::RightDockWidth(uiState.rightSidebarExpanded);
}

int DefaultPanelHeight(WorkspacePanelId panelId, const Size& viewport, bool statusBarVisible) {
    const int workspaceBottom = viewport.height - (statusBarVisible ? WorkspaceLayout::StatusBarH : 0);
    switch (panelId) {
        case WorkspacePanelId::Color: return 320;
        case WorkspacePanelId::BrushPreview: return 118;
        case WorkspacePanelId::BrushControl: return 250;
        case WorkspacePanelId::BrushPresets: return std::max(188, workspaceBottom - (WorkspaceLayout::TopBarH + 488));
        case WorkspacePanelId::Navigator: return 194;
        case WorkspacePanelId::Layer: return 332;
        case WorkspacePanelId::BrushSize: return std::max(156, workspaceBottom - (WorkspaceLayout::TopBarH + 520));
        default: return 180;
    }
}

Rect ClampFloatingRect(Rect rect, const Size& viewport) {
    const int minW = 220;
    const int minH = 120;
    if (rect.Width() < minW) rect.right = rect.left + minW;
    if (rect.Height() < minH) rect.bottom = rect.top + minH;
    const int maxLeft = std::max(0, viewport.width - rect.Width());
    const int maxTop = std::max(WorkspaceLayout::TopBarH, viewport.height - rect.Height());
    const int left = std::clamp(rect.left, 0, maxLeft);
    const int top = std::clamp(rect.top, WorkspaceLayout::TopBarH, maxTop);
    return Rect(left, top, left + rect.Width(), top + rect.Height());
}

void AddDockedPanels(WorkspaceDockLayoutResult& result,
                     const WorkspaceUiState& uiState,
                     WorkspaceDockSide side,
                     const Size& viewport,
                     bool statusBarVisible) {
    const bool expanded = side == WorkspaceDockSide::Left ? uiState.leftSidebarExpanded : uiState.rightSidebarExpanded;
    if (!expanded) {
        return;
    }
    const int left = side == WorkspaceDockSide::Left ? result.leftDockRect.left + 4 : result.rightDockRect.left + 8;
    const int right = side == WorkspaceDockSide::Left ? result.leftDockRect.right - 4 : result.rightDockRect.right - 8;
    int y = WorkspaceLayout::TopBarH + 4;
    const auto dockPanels = uiState.PanelsForDock(side);
    for (const auto* panel : dockPanels) {
        WorkspacePanelComputedLayout layout;
        layout.panelId = panel->panelId;
        layout.side = side;
        layout.floating = false;
        layout.zOrder = 40 + panel->dockOrder;
        const int height = DefaultPanelHeight(panel->panelId, viewport, statusBarVisible);
        layout.rect = Rect(left, y, right, y + height);
        layout.headerRect = Rect(layout.rect.left, layout.rect.top, layout.rect.right, layout.rect.top + PanelHeaderH);
        layout.contentRect = Rect(layout.rect.left + 8, layout.rect.top + PanelHeaderH + 4,
                                  layout.rect.right - 8, layout.rect.bottom - PanelFooterMargin);
        result.panels.push_back(layout);
        y += height + PanelGap;
    }
}

void AddFloatingPanels(WorkspaceDockLayoutResult& result, const WorkspaceUiState& uiState, const Size& viewport) {
    std::vector<const WorkspacePanelLayoutState*> floating;
    for (const auto& panel : uiState.panels) {
        if (panel.panelId == WorkspacePanelId::StatusBar || !panel.visible || panel.dockSide != WorkspaceDockSide::Floating) {
            continue;
        }
        floating.push_back(&panel);
    }
    std::sort(floating.begin(), floating.end(), [](const auto* a, const auto* b) {
        return a->floatingZ < b->floatingZ;
    });
    for (const auto* panel : floating) {
        WorkspacePanelComputedLayout layout;
        layout.panelId = panel->panelId;
        layout.side = WorkspaceDockSide::Floating;
        layout.floating = true;
        layout.zOrder = 70 + panel->floatingZ;
        layout.rect = ClampFloatingRect(panel->floatingRect, viewport);
        layout.headerRect = Rect(layout.rect.left, layout.rect.top, layout.rect.right, layout.rect.top + PanelHeaderH);
        layout.contentRect = Rect(layout.rect.left + 8, layout.rect.top + PanelHeaderH + 4,
                                  layout.rect.right - 8, layout.rect.bottom - PanelFooterMargin);
        result.panels.push_back(layout);
    }
}

} // namespace

const WorkspacePanelComputedLayout* WorkspaceDockLayoutResult::FindPanel(WorkspacePanelId panelId) const {
    auto it = std::find_if(panels.begin(), panels.end(), [&](const WorkspacePanelComputedLayout& panel) {
        return panel.panelId == panelId;
    });
    return it == panels.end() ? nullptr : &*it;
}

int WorkspaceDockLayoutResult::InsertIndexForDock(WorkspaceDockSide side, int y) const {
    int index = 0;
    for (const auto& panel : panels) {
        if (panel.side != side || panel.floating) {
            continue;
        }
        const int mid = panel.rect.top + panel.rect.Height() / 2;
        if (y < mid) {
            return index;
        }
        ++index;
    }
    return index;
}

Rect WorkspaceDockLayoutResult::BuildInsertionRect(WorkspaceDockSide side, int insertIndex) const {
    int count = 0;
    for (const auto& panel : panels) {
        if (panel.side != side || panel.floating) {
            continue;
        }
        if (count == insertIndex) {
            return Rect(panel.rect.left + 8, panel.rect.top - 3, panel.rect.right - 8, panel.rect.top + 3);
        }
        ++count;
    }
    Rect dock = side == WorkspaceDockSide::Left ? leftDockRect : rightDockRect;
    int bottom = WorkspaceLayout::TopBarH + 8;
    for (const auto& panel : panels) {
        if (panel.side == side && !panel.floating) {
            bottom = std::max(bottom, panel.rect.bottom + 5);
        }
    }
    return Rect(dock.left + 12, bottom, dock.right - 12, bottom + 6);
}

WorkspaceDockLayoutResult WorkspaceDockLayout::Compute(const Size& viewport,
                                                       const WorkspaceUiState& uiState,
                                                       bool statusBarVisible) {
    WorkspaceDockLayoutResult result;
    result.menuBarRect = Rect(0, 0, viewport.width, WorkspaceLayout::MenuBarH);
    result.commandBarRect = Rect(0, WorkspaceLayout::MenuBarH, viewport.width, WorkspaceLayout::TopBarH);
    result.toolRailRect = Rect(0, WorkspaceLayout::TopBarH, WorkspaceLayout::ToolBarW,
                               viewport.height - (statusBarVisible ? WorkspaceLayout::StatusBarH : 0));

    const int leftDockW = DockWidth(WorkspaceDockSide::Left, uiState);
    const int rightDockW = DockWidth(WorkspaceDockSide::Right, uiState);
    result.leftDockRect = Rect(WorkspaceLayout::ToolBarW, WorkspaceLayout::TopBarH,
                               WorkspaceLayout::ToolBarW + leftDockW,
                               viewport.height - (statusBarVisible ? WorkspaceLayout::StatusBarH : 0));
    result.rightDockRect = Rect(viewport.width - rightDockW, WorkspaceLayout::TopBarH,
                                viewport.width, viewport.height - (statusBarVisible ? WorkspaceLayout::StatusBarH : 0));
    result.canvasRect = WorkspaceLayout::CanvasRect(viewport, uiState.leftSidebarExpanded, uiState.rightSidebarExpanded, statusBarVisible);
    result.statusBarRect = Rect(0, viewport.height - WorkspaceLayout::StatusBarH, viewport.width, viewport.height);
    result.leftToggleRect = Rect(WorkspaceLayout::ToolBarW + (uiState.leftSidebarExpanded ? WorkspaceLayout::LeftPanelW - WorkspaceLayout::EdgeTabW : 0),
                                 WorkspaceLayout::TopBarH + 8,
                                 WorkspaceLayout::ToolBarW + (uiState.leftSidebarExpanded ? WorkspaceLayout::LeftPanelW : WorkspaceLayout::EdgeTabW),
                                 WorkspaceLayout::TopBarH + 86);
    result.rightToggleRect = Rect(viewport.width - (uiState.rightSidebarExpanded ? WorkspaceLayout::RightPanelW : WorkspaceLayout::EdgeTabW),
                                  WorkspaceLayout::TopBarH + 8,
                                  viewport.width - (uiState.rightSidebarExpanded ? WorkspaceLayout::RightPanelW - WorkspaceLayout::EdgeTabW : 0),
                                  WorkspaceLayout::TopBarH + 86);
    result.leftDockHotZone = uiState.leftSidebarExpanded
        ? Rect(WorkspaceLayout::ToolBarW, WorkspaceLayout::TopBarH,
               WorkspaceLayout::ToolBarW + WorkspaceLayout::LeftPanelW,
               viewport.height - (statusBarVisible ? WorkspaceLayout::StatusBarH : 0))
        : Rect();
    result.rightDockHotZone = uiState.rightSidebarExpanded
        ? Rect(viewport.width - WorkspaceLayout::RightPanelW, WorkspaceLayout::TopBarH,
               viewport.width,
               viewport.height - (statusBarVisible ? WorkspaceLayout::StatusBarH : 0))
        : Rect();

    AddDockedPanels(result, uiState, WorkspaceDockSide::Left, viewport, statusBarVisible);
    AddDockedPanels(result, uiState, WorkspaceDockSide::Right, viewport, statusBarVisible);
    AddFloatingPanels(result, uiState, viewport);
    return result;
}

} // namespace CloverPic::Core
