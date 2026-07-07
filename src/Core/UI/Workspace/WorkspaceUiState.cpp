#include "Core/UI/Workspace/WorkspaceUiState.h"
#include "Core/UI/Workspace/WorkspaceLayout.h"
#include <algorithm>

namespace CloverPic::Core {

namespace {

Rect DefaultFloatingRect(WorkspacePanelId panelId, const Size& viewport) {
    const int centerLeft = WorkspaceLayout::ToolBarW + 96;
    const int centerTop = WorkspaceLayout::TopBarH + 72;
    switch (panelId) {
        case WorkspacePanelId::Color:
            return Rect(centerLeft, centerTop, centerLeft + 284, centerTop + 320);
        case WorkspacePanelId::BrushPreview:
            return Rect(centerLeft + 36, centerTop + 44, centerLeft + 320, centerTop + 192);
        case WorkspacePanelId::BrushControl:
            return Rect(centerLeft + 72, centerTop + 18, centerLeft + 336, centerTop + 314);
        case WorkspacePanelId::BrushPresets:
            return Rect(centerLeft + 108, centerTop + 30, centerLeft + 370, std::min(viewport.height - 40, centerTop + 394));
        case WorkspacePanelId::Navigator:
            return Rect(std::max(WorkspaceLayout::ToolBarW + 120, viewport.width - 416), centerTop, std::max(WorkspaceLayout::ToolBarW + 412, viewport.width - 124), centerTop + 196);
        case WorkspacePanelId::Layer:
            return Rect(std::max(WorkspaceLayout::ToolBarW + 140, viewport.width - 436), centerTop + 42, std::max(WorkspaceLayout::ToolBarW + 432, viewport.width - 124), centerTop + 410);
        case WorkspacePanelId::BrushSize:
            return Rect(std::max(WorkspaceLayout::ToolBarW + 164, viewport.width - 392), centerTop + 86, std::max(WorkspaceLayout::ToolBarW + 424, viewport.width - 124), centerTop + 276);
        case WorkspacePanelId::StatusBar:
            return Rect(0, 0, 0, 0);
        default:
            return Rect(centerLeft, centerTop, centerLeft + 280, centerTop + 240);
    }
}

bool DefaultVisible(WorkspacePanelId panelId) {
    return panelId != WorkspacePanelId::StatusBar;
}

} // namespace

const wchar_t* PanelTitle(WorkspacePanelId panelId) {
    switch (panelId) {
        case WorkspacePanelId::Color: return L"Color";
        case WorkspacePanelId::BrushPreview: return L"Brush Preview";
        case WorkspacePanelId::BrushControl: return L"Brush Control";
        case WorkspacePanelId::BrushPresets: return L"Brush: Pen";
        case WorkspacePanelId::Navigator: return L"Navigator";
        case WorkspacePanelId::Layer: return L"Layer";
        case WorkspacePanelId::BrushSize: return L"Brush Size";
        case WorkspacePanelId::StatusBar: return L"Status Bar";
        default: return L"Panel";
    }
}

WorkspaceDockSide DefaultDockSide(WorkspacePanelId panelId) {
    switch (panelId) {
        case WorkspacePanelId::Color:
        case WorkspacePanelId::BrushPreview:
        case WorkspacePanelId::BrushControl:
        case WorkspacePanelId::BrushPresets:
            return WorkspaceDockSide::Left;
        case WorkspacePanelId::Navigator:
        case WorkspacePanelId::Layer:
        case WorkspacePanelId::BrushSize:
            return WorkspaceDockSide::Right;
        case WorkspacePanelId::StatusBar:
            return WorkspaceDockSide::Left;
        default:
            return WorkspaceDockSide::Left;
    }
}

void WorkspaceUiState::Reset(const Size& viewport) {
    leftSidebarExpanded = true;
    rightSidebarExpanded = true;
    openMenu.clear();
    statusText = L"READY";
    dockInsertion = {};
    draggingPanel = false;
    panels.clear();

    const WorkspacePanelId order[] = {
        WorkspacePanelId::Color,
        WorkspacePanelId::BrushPreview,
        WorkspacePanelId::BrushControl,
        WorkspacePanelId::BrushPresets,
        WorkspacePanelId::Navigator,
        WorkspacePanelId::Layer,
        WorkspacePanelId::BrushSize,
        WorkspacePanelId::StatusBar
    };

    int leftOrder = 0;
    int rightOrder = 0;
    int floatingZ = 1;
    for (WorkspacePanelId panelId : order) {
        WorkspacePanelLayoutState state;
        state.panelId = panelId;
        state.dockSide = DefaultDockSide(panelId);
        state.dockOrder = state.dockSide == WorkspaceDockSide::Right ? rightOrder++ : leftOrder++;
        state.floatingRect = DefaultFloatingRect(panelId, viewport);
        state.visible = DefaultVisible(panelId) || panelId == WorkspacePanelId::StatusBar;
        state.collapsed = false;
        state.floatingZ = floatingZ++;
        panels.push_back(state);
    }
}

WorkspacePanelLayoutState* WorkspaceUiState::FindPanel(WorkspacePanelId panelId) {
    auto it = std::find_if(panels.begin(), panels.end(), [&](const WorkspacePanelLayoutState& panel) {
        return panel.panelId == panelId;
    });
    return it == panels.end() ? nullptr : &*it;
}

const WorkspacePanelLayoutState* WorkspaceUiState::FindPanel(WorkspacePanelId panelId) const {
    auto it = std::find_if(panels.begin(), panels.end(), [&](const WorkspacePanelLayoutState& panel) {
        return panel.panelId == panelId;
    });
    return it == panels.end() ? nullptr : &*it;
}

bool WorkspaceUiState::IsPanelVisible(WorkspacePanelId panelId) const {
    const auto* panel = FindPanel(panelId);
    return panel ? panel->visible : false;
}

void WorkspaceUiState::SetPanelVisible(WorkspacePanelId panelId, bool visible) {
    if (auto* panel = FindPanel(panelId)) {
        panel->visible = visible;
    }
}

void WorkspaceUiState::TogglePanel(WorkspacePanelId panelId) {
    if (auto* panel = FindPanel(panelId)) {
        panel->visible = !panel->visible;
    }
}

void WorkspaceUiState::NormalizeDockOrder(WorkspaceDockSide side) {
    auto dockPanels = PanelsForDock(side);
    std::sort(dockPanels.begin(), dockPanels.end(), [](const auto* a, const auto* b) {
        return a->dockOrder < b->dockOrder;
    });
    for (size_t i = 0; i < dockPanels.size(); ++i) {
        dockPanels[i]->dockOrder = static_cast<int>(i);
    }
}

void WorkspaceUiState::NormalizeAllDockOrders() {
    NormalizeDockOrder(WorkspaceDockSide::Left);
    NormalizeDockOrder(WorkspaceDockSide::Right);
}

void WorkspaceUiState::BringFloatingPanelToFront(WorkspacePanelId panelId) {
    int maxZ = 1;
    for (const auto& panel : panels) {
        maxZ = std::max(maxZ, panel.floatingZ);
    }
    if (auto* panel = FindPanel(panelId)) {
        panel->floatingZ = maxZ + 1;
    }
}

std::vector<WorkspacePanelLayoutState*> WorkspaceUiState::PanelsForDock(WorkspaceDockSide side) {
    std::vector<WorkspacePanelLayoutState*> result;
    for (auto& panel : panels) {
        if (panel.panelId == WorkspacePanelId::StatusBar) {
            continue;
        }
        if (panel.dockSide == side && panel.visible) {
            result.push_back(&panel);
        }
    }
    std::sort(result.begin(), result.end(), [](const auto* a, const auto* b) {
        if (a->dockOrder != b->dockOrder) return a->dockOrder < b->dockOrder;
        return static_cast<int>(a->panelId) < static_cast<int>(b->panelId);
    });
    return result;
}

std::vector<const WorkspacePanelLayoutState*> WorkspaceUiState::PanelsForDock(WorkspaceDockSide side) const {
    std::vector<const WorkspacePanelLayoutState*> result;
    for (const auto& panel : panels) {
        if (panel.panelId == WorkspacePanelId::StatusBar) {
            continue;
        }
        if (panel.dockSide == side && panel.visible) {
            result.push_back(&panel);
        }
    }
    std::sort(result.begin(), result.end(), [](const auto* a, const auto* b) {
        if (a->dockOrder != b->dockOrder) return a->dockOrder < b->dockOrder;
        return static_cast<int>(a->panelId) < static_cast<int>(b->panelId);
    });
    return result;
}

} // namespace CloverPic::Core
