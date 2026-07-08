#pragma once

#include "Core/App/AppCommand.h"
#include "Core/App/WorkspaceEditorFacade.h"
#include "Core/UI/Workspace/Layout/WorkspaceDockLayout.h"
#include "Core/UI/Workspace/WorkspaceUiState.h"
#include "Core/UI/Scene/UiScene.h"
#include <functional>

namespace CloverPic::Core {

enum class ActiveWorkspaceInteractionKind : uint8_t {
    None,
    Slider,
    ColorField,
    HueStrip,
    PanelDrag,
    PanelResize,
    LayerDrag
};

struct ActiveWorkspaceInteraction {
    ActiveWorkspaceInteractionKind kind = ActiveWorkspaceInteractionKind::None;
    Presentation::UiNodeId nodeId = 0;
    WorkspacePanelId panelId = WorkspacePanelId::Color;
    CoreUI::UiBindingKey bindingKey = CoreUI::UiBindingKey::None;
    Point dragStart;
    Point lastPoint;
    Point panelGrabOffset;
    Size panelStartSize;
    String resizeAxis;
    WorkspacePanelLayoutState originalPanelState;
    size_t layerIndex = 0;
};

struct WorkspaceInteractionCallbacks {
    std::function<void(AppCommand, uint64_t, const String&)> dispatch;
    std::function<void(const Rect&)> markDirty;
    std::function<void()> markProjectDirty;
    std::function<void()> rebuildScene;
    std::function<void(const String&)> setStatus;
};

class WorkspaceInteractionController {
public:
    void Reset();
    const ActiveWorkspaceInteraction& Active() const { return m_active; }

    bool Begin(const Input::PointerEvent& event,
               const CoreUI::UiNode& node,
               CoreUI::UiScene& scene,
               WorkspaceUiState& uiState,
               const WorkspaceDockLayoutResult& layout,
               WorkspaceEditorFacade& editor,
               const WorkspaceInteractionCallbacks& callbacks);

    bool Update(const Input::PointerEvent& event,
                CoreUI::UiScene& scene,
                WorkspaceUiState& uiState,
                const WorkspaceDockLayoutResult& layout,
                WorkspaceEditorFacade& editor,
                const WorkspaceInteractionCallbacks& callbacks);

    bool End(const Input::PointerEvent& event,
             CoreUI::UiScene& scene,
             WorkspaceUiState& uiState,
             const WorkspaceDockLayoutResult& layout,
             WorkspaceEditorFacade& editor,
             const WorkspaceInteractionCallbacks& callbacks);

private:
    void ApplySliderAtPoint(const CoreUI::UiNode& node, const Point& position, WorkspaceEditorFacade& editor, const WorkspaceInteractionCallbacks& callbacks) const;
    void ApplyColorFieldAtPoint(const CoreUI::UiNode& node, const Point& position, WorkspaceEditorFacade& editor, const WorkspaceInteractionCallbacks& callbacks) const;
    void ApplyHueStripAtPoint(const CoreUI::UiNode& node, const Point& position, WorkspaceEditorFacade& editor, const WorkspaceInteractionCallbacks& callbacks) const;
    void UpdatePanelDrag(const Point& position,
                         WorkspaceUiState& uiState,
                         const WorkspaceDockLayoutResult& layout);
    void CommitPanelDrag(const Point& position,
                         WorkspaceUiState& uiState,
                         const WorkspaceDockLayoutResult& layout);

    ActiveWorkspaceInteraction m_active;
};

} // namespace CloverPic::Core
