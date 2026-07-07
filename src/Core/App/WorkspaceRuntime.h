#pragma once

#include "Core/App/CommandDispatcher.h"
#include "Core/App/WorkspaceEditorFacade.h"
#include "Core/App/WorkspaceActionController.h"
#include "Core/UI/Workspace/Layout/WorkspaceDockLayout.h"
#include "Core/UI/Workspace/Interaction/WorkspaceInteractionController.h"
#include "Core/UI/Workspace/Modal/WorkspaceModalState.h"
#include "Core/UI/Workspace/WorkspaceUiState.h"
#include "Core/App/ModalManager.h"
#include "Core/App/RuntimeSurface.h"
#include "Core/App/CanvasController.h"
#include "Core/Document/EditorSession.h"
#include "Core/Presentation/FrameScheduler.h"
#include "Core/UI/Scene/UiScene.h"

namespace CloverPic::Core {

class WorkspaceRuntime final : public RuntimeSurface, private CommandDispatcher::Handler {
public:
    explicit WorkspaceRuntime(WorkspaceLaunchRequest launchRequest = {});

    bool Initialize() override;
    void Resize(uint32_t width, uint32_t height, float dpiScale) override;
    const RgbaFrame& Render(uint64_t nowMs, std::vector<Rect>& outDirtyRects) override;
    void HandlePointer(const Input::PointerEvent& event) override;
    void HandleKey(const Input::KeyEvent& event) override;
    void HandleWheel(int delta, const Point& position) override;
    bool NeedsFrame(uint64_t nowMs) const override;
    bool WantsQuit() const override { return m_wantsClose; }
    bool HasProject() const { return m_canvas.HasProject(); }
    bool HasUnsavedChanges() const { return m_session.HasUnsavedChanges(); }
    void ClearQuitRequest() { m_wantsClose = false; }
    bool SaveCurrentProjectInteractive(bool saveAs = false);
    void AcceptWorkspaceLaunchRequest(WorkspaceLaunchRequest launchRequest);
    bool ConsumeProgramManagerRequest();
    bool ConsumeProgramManagerSettingsRequest();

private:
    bool OpenLaunchRequest(const WorkspaceLaunchRequest& launchRequest);
    void BuildScene();
    void LoadWorkspaceUiSettings();
    void SaveWorkspaceUiSettings();
    void RecomputeLayout();
    void MarkDirty(const Rect& rect);
    Rect FullRect() const { return Rect(0, 0, static_cast<int32_t>(m_frame.width), static_cast<int32_t>(m_frame.height)); }
    Rect CanvasRect() const;
    void DispatchCommand(AppCommand command, uint64_t userData = 0, const String& payload = L"");
    WorkspaceActionContext BuildActionContext();
    WorkspaceActionCallbacks BuildActionCallbacks();

    void RenderWorkspace(Presentation::SoftRenderer& renderer);
    void RenderModal(Presentation::SoftRenderer& renderer);
    void RenderWorkspacePanels(Presentation::SoftRenderer& renderer);

    bool SaveProject(bool saveAs);
    void ExportPng();
    void SwitchTool(ToolType tool);
    void SetStatus(const String& status);
    void TouchWorkspace();
    void MarkProjectDirty();
    bool ConfirmAbandonUnsavedChanges(const String& actionLabel);

    void OnGoHome() override;
    void OnShowNewCanvasModal() override;
    void OnShowSettingsModal() override;
    void OnCreateCanvasFromForm() override;
    void OnSwapCanvasOrientation() override;
    void OnCreateCanvasPreset(uint32_t width, uint32_t height) override;
    void OnSetCanvasAnchor(uint32_t anchor) override;
    void OnCloseModal() override;
    void OnOpenProject() override;
    void OnOpenRecentProject(const String& path) override;
    void OnFocusHomeSearch() override {}
    void OnShowFiltersModal() override;
    void OnShowTextLayerModal() override;
    void OnSaveProject(bool saveAs) override;
    void OnExportPng() override;
    void OnUndo() override;
    void OnRedo() override;
    void OnShowCanvasSizeModal() override;
    void OnFlipLayerHorizontal() override;
    void OnFlipLayerVertical() override;
    void OnRotateLayerLeft() override;
    void OnRotateLayerRight() override;
    void OnSelectAll() override;
    void OnClearSelection() override;
    void OnInvertSelection() override;
    void OnQuit() override;
    void OnSelectTool(ToolType tool) override;
    void OnSetColor(const Color& color) override;
    void OnAddLayer() override;
    void OnDeleteLayer() override;
    void OnToggleActiveLayerVisibility() override;
    void OnToggleActiveLayerLock() override;
    void OnToggleActiveLayerProtectAlpha() override;
    void OnToggleActiveLayerSolo() override;
    void OnDuplicateLayer() override;
    void OnMergeLayerDown() override;
    void OnSetLayerOpacity(uint8_t opacity) override;
    void OnToggleLayerBlendDropdown() override;
    void OnSetBlendMode(uint32_t mode) override;
    void OnSelectLayer(size_t index) override;
    void OnSetBrushParam(BrushParamId param, uint16_t value) override;
    void OnSetBrushTip(uint32_t tip) override;
    void OnSetBrushPreset(uint16_t size, uint16_t tip) override;
    void OnApplyFilter(FilterCommandId filter, uint16_t value) override;
    void OnCreateTextLayer(const String& text) override;
    void OnSetZoomPreset(uint16_t zoomPercent) override;
    void OnZoomIn() override;
    void OnZoomOut() override;
    void OnFitCanvas() override;
    void OnResetView() override;
    void OnToggleViewOption(ViewOptionId option) override;
    void OnSetSnapMode(SnapModeId mode) override;
    void OnTogglePanel(WorkspacePanelId panel) override;
    void OnToggleLeftSidebar() override;
    void OnToggleRightSidebar() override;
    void OnInitializeLayout() override;
    void OnShowUnavailable() override;
    void OnCloseWorkspace() override;

    WorkspaceLaunchRequest m_launchRequest;
    RgbaFrame m_frame;
    Presentation::FrameScheduler m_scheduler;
    CoreUI::UiScene m_scene;
    CommandDispatcher m_commands;
    ModalManager m_modals;
    WorkspaceUiState m_uiState;
    WorkspaceDockLayoutResult m_layout;
    WorkspaceInteractionController m_interaction;
    EditorSession m_session;
    CanvasController m_canvas;
    WorkspaceEditorFacade m_editor;
    Size m_viewport;
    float m_dpiScale = 1.0f;
    bool m_wantsClose = false;
    bool m_wantsProgramManager = false;
    bool m_wantsProgramManagerSettings = false;
    String m_status = L"READY";
    String m_openMenu;
    WorkspaceCanvasSizeState m_canvasSizeState;
};

} // namespace CloverPic::Core
