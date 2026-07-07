#include "Core/UI/Workspace/Interaction/WorkspaceInteractionController.h"
#include <algorithm>
#include <cmath>

namespace CloverPic::Core {

namespace {

struct Hsv {
    float h = 0.0f;
    float s = 0.0f;
    float v = 0.0f;
};

Hsv RgbToHsv(const Color& color) {
    const float r = color.r / 255.0f;
    const float g = color.g / 255.0f;
    const float b = color.b / 255.0f;
    const float maxV = std::max({ r, g, b });
    const float minV = std::min({ r, g, b });
    const float d = maxV - minV;

    Hsv hsv;
    hsv.v = maxV;
    hsv.s = maxV <= 0.0f ? 0.0f : d / maxV;
    if (d <= 0.0001f) {
        hsv.h = 0.0f;
    } else if (maxV == r) {
        hsv.h = 60.0f * std::fmod(((g - b) / d), 6.0f);
    } else if (maxV == g) {
        hsv.h = 60.0f * (((b - r) / d) + 2.0f);
    } else {
        hsv.h = 60.0f * (((r - g) / d) + 4.0f);
    }
    if (hsv.h < 0.0f) hsv.h += 360.0f;
    return hsv;
}

Color HsvToRgb(float hueDegrees, float saturation, float value, uint8_t alpha = 255) {
    float h = std::fmod(hueDegrees, 360.0f);
    if (h < 0.0f) h += 360.0f;
    saturation = std::clamp(saturation, 0.0f, 1.0f);
    value = std::clamp(value, 0.0f, 1.0f);

    const float c = value * saturation;
    const float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    const float m = value - c;
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    if (h < 60.0f) {
        r = c; g = x;
    } else if (h < 120.0f) {
        r = x; g = c;
    } else if (h < 180.0f) {
        g = c; b = x;
    } else if (h < 240.0f) {
        g = x; b = c;
    } else if (h < 300.0f) {
        r = x; b = c;
    } else {
        r = c; b = x;
    }
    return Color(static_cast<uint8_t>(std::clamp((r + m) * 255.0f, 0.0f, 255.0f)),
                 static_cast<uint8_t>(std::clamp((g + m) * 255.0f, 0.0f, 255.0f)),
                 static_cast<uint8_t>(std::clamp((b + m) * 255.0f, 0.0f, 255.0f)),
                 alpha);
}

} // namespace

void WorkspaceInteractionController::Reset() {
    m_active = {};
}

bool WorkspaceInteractionController::Begin(const Input::PointerEvent& event,
                                           const CoreUI::UiNode& node,
                                           CoreUI::UiScene& scene,
                                           WorkspaceUiState& uiState,
                                           const WorkspaceDockLayoutResult& layout,
                                           WorkspaceEditorFacade& editor,
                                           const WorkspaceInteractionCallbacks& callbacks) {
    scene.SetCapture(node.id);
    scene.SetFocus(node.id);
    m_active = {};
    m_active.nodeId = node.id;
    m_active.bindingKey = node.bindingKey;
    m_active.dragStart = event.position;
    m_active.lastPoint = event.position;
    m_active.panelId = static_cast<WorkspacePanelId>(node.panelId);

    if (node.type == CoreUI::UiNodeType::Slider) {
        m_active.kind = ActiveWorkspaceInteractionKind::Slider;
        ApplySliderAtPoint(node, event.position, editor, callbacks);
        callbacks.markDirty(layout.canvasRect);
        return true;
    }
    if (node.type == CoreUI::UiNodeType::ColorField) {
        m_active.kind = ActiveWorkspaceInteractionKind::ColorField;
        ApplyColorFieldAtPoint(node, event.position, editor, callbacks);
        callbacks.markDirty(layout.canvasRect);
        return true;
    }
    if (node.type == CoreUI::UiNodeType::HueStrip) {
        m_active.kind = ActiveWorkspaceInteractionKind::HueStrip;
        ApplyHueStripAtPoint(node, event.position, editor, callbacks);
        callbacks.markDirty(layout.canvasRect);
        return true;
    }
    if (node.type == CoreUI::UiNodeType::PanelHeader) {
        m_active.kind = ActiveWorkspaceInteractionKind::PanelDrag;
        if (auto* panel = uiState.FindPanel(m_active.panelId)) {
            m_active.originalPanelState = *panel;
            const auto* computed = layout.FindPanel(m_active.panelId);
            if (computed) {
                m_active.panelGrabOffset = Point(event.position.x - computed->rect.left, event.position.y - computed->rect.top);
                uiState.draggingPanel = true;
                uiState.draggedPanelId = m_active.panelId;
                uiState.draggedPreviewRect = computed->rect;
                uiState.BringFloatingPanelToFront(m_active.panelId);
            }
        }
        callbacks.markDirty(layout.canvasRect);
        return true;
    }

    return false;
}

bool WorkspaceInteractionController::Update(const Input::PointerEvent& event,
                                            CoreUI::UiScene& scene,
                                            WorkspaceUiState& uiState,
                                            const WorkspaceDockLayoutResult& layout,
                                            WorkspaceEditorFacade& editor,
                                            const WorkspaceInteractionCallbacks& callbacks) {
    if (m_active.kind == ActiveWorkspaceInteractionKind::None) {
        return false;
    }
    m_active.lastPoint = event.position;
    auto* node = scene.Find(m_active.nodeId);
    if (!node) {
        return false;
    }

    switch (m_active.kind) {
        case ActiveWorkspaceInteractionKind::Slider:
            ApplySliderAtPoint(*node, event.position, editor, callbacks);
            callbacks.markDirty(layout.canvasRect);
            return true;
        case ActiveWorkspaceInteractionKind::ColorField:
            ApplyColorFieldAtPoint(*node, event.position, editor, callbacks);
            callbacks.markDirty(layout.canvasRect);
            return true;
        case ActiveWorkspaceInteractionKind::HueStrip:
            ApplyHueStripAtPoint(*node, event.position, editor, callbacks);
            callbacks.markDirty(layout.canvasRect);
            return true;
        case ActiveWorkspaceInteractionKind::PanelDrag:
            UpdatePanelDrag(event.position, uiState, layout);
            callbacks.rebuildScene();
            callbacks.markDirty(Rect(0, 0, layout.statusBarRect.right, layout.statusBarRect.bottom));
            return true;
        default:
            return false;
    }
}

bool WorkspaceInteractionController::End(const Input::PointerEvent& event,
                                         CoreUI::UiScene& scene,
                                         WorkspaceUiState& uiState,
                                         const WorkspaceDockLayoutResult& layout,
                                         WorkspaceEditorFacade& editor,
                                         const WorkspaceInteractionCallbacks& callbacks) {
    if (m_active.kind == ActiveWorkspaceInteractionKind::None) {
        scene.SetCapture(0);
        return false;
    }

    auto* node = scene.Find(m_active.nodeId);
    if (node && m_active.kind == ActiveWorkspaceInteractionKind::Slider) {
            ApplySliderAtPoint(*node, event.position, editor, callbacks);
    } else if (node && m_active.kind == ActiveWorkspaceInteractionKind::ColorField) {
            ApplyColorFieldAtPoint(*node, event.position, editor, callbacks);
    } else if (node && m_active.kind == ActiveWorkspaceInteractionKind::HueStrip) {
            ApplyHueStripAtPoint(*node, event.position, editor, callbacks);
    } else if (m_active.kind == ActiveWorkspaceInteractionKind::PanelDrag) {
        CommitPanelDrag(event.position, uiState, layout);
        callbacks.rebuildScene();
    }

    scene.SetCapture(0);
    m_active = {};
    uiState.draggingPanel = false;
    uiState.dockInsertion = {};
    callbacks.markDirty(Rect(0, 0, layout.statusBarRect.right, layout.statusBarRect.bottom));
    return true;
}

void WorkspaceInteractionController::ApplySliderAtPoint(const CoreUI::UiNode& node, const Point& position, WorkspaceEditorFacade& editor, const WorkspaceInteractionCallbacks& callbacks) const {
    const int trackLeft = node.bounds.left + 86;
    const int trackRight = node.bounds.right - 46;
    const int width = std::max(1, trackRight - trackLeft);
    const int clampedX = std::clamp(position.x, trackLeft, trackRight);
    const uint16_t value = static_cast<uint16_t>(((clampedX - trackLeft) * 100) / width);
    const AppCommand command = static_cast<AppCommand>(node.command);
    if (command == AppCommand::SetBrushParam) {
        editor.SetBrushParam(BrushParamFromPacked(node.userData), value);
        callbacks.setStatus(L"BRUSH PARAM");
    } else if (command == AppCommand::SetLayerOpacity) {
        editor.SetActiveLayerOpacity(static_cast<uint8_t>((value * 255) / 100));
        callbacks.markProjectDirty();
        callbacks.setStatus(L"OPACITY");
    }
}

void WorkspaceInteractionController::ApplyColorFieldAtPoint(const CoreUI::UiNode& node, const Point& position, WorkspaceEditorFacade& editor, const WorkspaceInteractionCallbacks& callbacks) const {
    const Hsv current = RgbToHsv(editor.GetColor());
    const float s = std::clamp((position.x - node.bounds.left) / static_cast<float>(std::max(1, node.bounds.Width() - 1)), 0.0f, 1.0f);
    const float v = 1.0f - std::clamp((position.y - node.bounds.top) / static_cast<float>(std::max(1, node.bounds.Height() - 1)), 0.0f, 1.0f);
    editor.SetColor(HsvToRgb(current.h, s, v, editor.GetColor().a));
    callbacks.setStatus(L"COLOR");
}

void WorkspaceInteractionController::ApplyHueStripAtPoint(const CoreUI::UiNode& node, const Point& position, WorkspaceEditorFacade& editor, const WorkspaceInteractionCallbacks& callbacks) const {
    Hsv current = RgbToHsv(editor.GetColor());
    current.h = std::clamp((position.y - node.bounds.top) / static_cast<float>(std::max(1, node.bounds.Height() - 1)), 0.0f, 1.0f) * 360.0f;
    if (current.s <= 0.01f) current.s = 1.0f;
    if (current.v <= 0.01f) current.v = 1.0f;
    editor.SetColor(HsvToRgb(current.h, current.s, current.v, editor.GetColor().a));
    callbacks.setStatus(L"COLOR");
}

void WorkspaceInteractionController::UpdatePanelDrag(const Point& position,
                                                     WorkspaceUiState& uiState,
                                                     const WorkspaceDockLayoutResult& layout) {
    auto* panel = uiState.FindPanel(m_active.panelId);
    if (!panel) {
        return;
    }

    const int width = m_active.originalPanelState.dockSide == WorkspaceDockSide::Floating
        ? m_active.originalPanelState.floatingRect.Width()
        : std::max(220, uiState.draggedPreviewRect.Width());
    const int height = m_active.originalPanelState.dockSide == WorkspaceDockSide::Floating
        ? m_active.originalPanelState.floatingRect.Height()
        : std::max(120, uiState.draggedPreviewRect.Height());
    uiState.draggedPreviewRect = Rect(position.x - m_active.panelGrabOffset.x,
                                      position.y - m_active.panelGrabOffset.y,
                                      position.x - m_active.panelGrabOffset.x + width,
                                      position.y - m_active.panelGrabOffset.y + height);
    uiState.dockInsertion = {};

    if (uiState.leftSidebarExpanded && layout.leftDockHotZone.Contains(position)) {
        uiState.dockInsertion.visible = true;
        uiState.dockInsertion.side = WorkspaceDockSide::Left;
        uiState.dockInsertion.insertIndex = layout.InsertIndexForDock(WorkspaceDockSide::Left, position.y);
        uiState.dockInsertion.rect = layout.BuildInsertionRect(WorkspaceDockSide::Left, uiState.dockInsertion.insertIndex);
    } else if (uiState.rightSidebarExpanded && layout.rightDockHotZone.Contains(position)) {
        uiState.dockInsertion.visible = true;
        uiState.dockInsertion.side = WorkspaceDockSide::Right;
        uiState.dockInsertion.insertIndex = layout.InsertIndexForDock(WorkspaceDockSide::Right, position.y);
        uiState.dockInsertion.rect = layout.BuildInsertionRect(WorkspaceDockSide::Right, uiState.dockInsertion.insertIndex);
    }
}

void WorkspaceInteractionController::CommitPanelDrag(const Point&,
                                                     WorkspaceUiState& uiState,
                                                     const WorkspaceDockLayoutResult&) {
    auto* panel = uiState.FindPanel(m_active.panelId);
    if (!panel) {
        return;
    }

    const bool targetDockExpanded = uiState.dockInsertion.side == WorkspaceDockSide::Left
        ? uiState.leftSidebarExpanded
        : uiState.rightSidebarExpanded;
    if (uiState.dockInsertion.visible && targetDockExpanded) {
        panel->dockSide = uiState.dockInsertion.side;
        auto dockPanels = uiState.PanelsForDock(uiState.dockInsertion.side);
        for (auto* dockPanel : dockPanels) {
            if (dockPanel->panelId == panel->panelId) {
                continue;
            }
            if (dockPanel->dockOrder >= uiState.dockInsertion.insertIndex) {
                dockPanel->dockOrder += 1;
            }
        }
        panel->dockOrder = uiState.dockInsertion.insertIndex;
        uiState.NormalizeAllDockOrders();
        return;
    }

    panel->dockSide = WorkspaceDockSide::Floating;
    panel->floatingRect = uiState.draggedPreviewRect;
    panel->floatingZ = std::max(panel->floatingZ, 1);
    uiState.BringFloatingPanelToFront(panel->panelId);
}

} // namespace CloverPic::Core
