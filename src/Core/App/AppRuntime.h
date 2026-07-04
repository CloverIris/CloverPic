#pragma once

#include "Core/App/AppCommand.h"
#include "Core/App/AppScreen.h"
#include "Core/App/CanvasController.h"
#include "Core/App/CommandDispatcher.h"
#include "Core/App/ModalManager.h"
#include "Core/EditorSession.h"
#include "Core/Platform/PlatformInterfaces.h"
#include "Core/Presentation/FrameScheduler.h"
#include "Core/UI/UiScene.h"

namespace CloverPic::Core {

class AppRuntime : private CommandDispatcher::Handler {
public:
    bool Initialize();
    void Resize(uint32_t width, uint32_t height, float dpiScale);
    const RgbaFrame& Render(uint64_t nowMs, std::vector<Rect>& outDirtyRects);

    void HandlePointer(const Input::PointerEvent& event);
    void HandleKey(const Input::KeyEvent& event);
    void HandleWheel(int delta, const Point& position);

    bool NeedsFrame(uint64_t nowMs) const;
    void MarkDirty(const Rect& rect);
    bool WantsQuit() const { return m_wantsQuit; }

private:
    void BuildScene();
    void RenderHome(Presentation::SoftRenderer& renderer);
    void RenderWorkspace(Presentation::SoftRenderer& renderer);
    void RenderModal(Presentation::SoftRenderer& renderer);
    void RenderButton(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node, bool active = false);
    void RenderHomeAction(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node);
    void RenderHomeRecent(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node);
    void RenderHomeSearch(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node);
    void RenderHomeTile(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node);
    void RenderLayerItem(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node);
    void RenderSwatch(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node);
    void RenderPanel(Presentation::SoftRenderer& renderer, const Rect& rect, const String& title);

    void NewDefaultCanvas();
    void NewCanvas(uint32_t width, uint32_t height, float dpi, bool transparent);
    void OpenProject();
    void OpenProjectPath(const String& path);
    void DispatchCommand(AppCommand command, uint64_t userData = 0, const String& payload = L"");
    void SaveProject(bool saveAs);
    void ExportPng();
    void SwitchTool(ToolType tool);
    bool HandleHomeSearchKey(const Input::KeyEvent& event);
    void AddLayer();
    void DeleteLayer();
    void ToggleActiveLayerVisibility();

    void OnGoHome() override;
    void OnShowNewCanvasModal() override;
    void OnCreateCanvasPreset(uint32_t width, uint32_t height) override;
    void OnCloseModal() override;
    void OnOpenProject() override;
    void OnOpenRecentProject(const String& path) override;
    void OnFocusHomeSearch() override;
    void OnSaveProject(bool saveAs) override;
    void OnExportPng() override;
    void OnUndo() override;
    void OnRedo() override;
    void OnQuit() override;
    void OnSelectTool(ToolType tool) override;
    void OnSetColor(const Color& color) override;
    void OnAddLayer() override;
    void OnDeleteLayer() override;
    void OnToggleActiveLayerVisibility() override;
    void OnSelectLayer(size_t index) override;

    Rect CanvasRect() const;
    Rect FullRect() const { return Rect(0, 0, static_cast<int32_t>(m_frame.width), static_cast<int32_t>(m_frame.height)); }

    RgbaFrame m_frame;
    Presentation::FrameScheduler m_scheduler;
    CoreUI::UiScene m_scene;
    CommandDispatcher m_commands;
    ModalManager m_modals;
    EditorSession m_session;
    CanvasController m_canvas;
    AppScreen m_screen = AppScreen::Home;
    Size m_viewport;
    float m_dpiScale = 1.0f;
    bool m_wantsQuit = false;
    String m_status = L"READY";
    String m_homeSearchQuery;
};

} // namespace CloverPic::Core
