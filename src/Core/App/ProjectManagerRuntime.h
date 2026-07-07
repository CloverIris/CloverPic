#pragma once

#include "Core/App/CommandDispatcher.h"
#include "Core/App/RuntimeSurface.h"
#include "Core/Document/EditorSession.h"
#include "Core/Presentation/FrameScheduler.h"
#include "Core/Services/CoreServices.h"
#include "Core/UI/ProjectManager/State/ProjectManagerUiState.h"
#include "Core/UI/Scene/UiScene.h"

namespace CloverPic::Core {

class ProjectManagerRuntime final : public RuntimeSurface, private CommandDispatcher::Handler {
public:
    bool Initialize() override;
    void Resize(uint32_t width, uint32_t height, float dpiScale) override;
    const RgbaFrame& Render(uint64_t nowMs, std::vector<Rect>& outDirtyRects) override;
    void HandlePointer(const Input::PointerEvent& event) override;
    void HandleKey(const Input::KeyEvent& event) override;
    void HandleWheel(int delta, const Point& position) override;
    bool NeedsFrame(uint64_t nowMs) const override;
    bool WantsQuit() const override { return m_wantsQuit; }
    bool IsWindowDragRegion(const Point& position) override;
    void ClearQuitRequest() { m_wantsQuit = false; }

    bool HasLaunchRequest() const { return m_launchRequest.kind != WorkspaceLaunchKind::None; }
    WorkspaceLaunchRequest ConsumeLaunchRequest();
    void RefreshRecentFiles();
    void ShowSettingsPage();

private:
    void BuildScene();
    void MarkDirty(const Rect& rect);
    Rect FullRect() const { return Rect(0, 0, static_cast<int32_t>(m_frame.width), static_cast<int32_t>(m_frame.height)); }
    void DispatchCommand(AppCommand command, uint64_t userData = 0, const String& payload = L"");
    void StartNewCanvas(uint32_t width, uint32_t height);
    void OpenProjectPath(const String& path);
    void LoadColorProfiles();
    void SaveSettingsDraft(bool persist);
    String ProfileLabel(size_t index) const;
    String ActiveFieldPayload(ProjectManagerActiveField field) const;

    void OnGoHome() override {}
    void OnShowNewCanvasModal() override;
    void OnShowSettingsModal() override;
    void OnCreateCanvasFromForm() override;
    void OnSwapCanvasOrientation() override;
    void OnToggleCanvasTransparency() override;
    void OnSelectRgbProfile(size_t index) override;
    void OnSelectCmykProfile(size_t index) override;
    void OnSetSettingsCategory(size_t index) override;
    void OnApplySettings() override;
    void OnSaveSettings() override;
    void OnCreateCanvasPreset(uint32_t width, uint32_t height) override;
    void OnCloseModal() override;
    void OnOpenProject() override;
    void OnOpenRecentProject(const String& path) override;
    void OnFocusHomeSearch() override;
    void OnSaveProject(bool) override {}
    void OnExportPng() override {}
    void OnUndo() override {}
    void OnRedo() override {}
    void OnQuit() override;
    void OnSelectTool(ToolType) override {}
    void OnSetColor(const Color&) override {}
    void OnAddLayer() override {}
    void OnDeleteLayer() override {}
    void OnToggleActiveLayerVisibility() override {}
    void OnSelectLayer(size_t) override {}

    RgbaFrame m_frame;
    Presentation::FrameScheduler m_scheduler;
    CoreUI::UiScene m_scene;
    CommandDispatcher m_commands;
    EditorSession m_session;
    WorkspaceLaunchRequest m_launchRequest;
    Size m_viewport;
    float m_dpiScale = 1.0f;
    bool m_wantsQuit = false;
    ProjectManagerUiState m_uiState;
};

} // namespace CloverPic::Core
