#pragma once

#include "UI/Core/Window.h"
#include "UI/Widgets/CanvasView.h"
#include "UI/Panels/LayersPanel.h"
#include "UI/Panels/ColorsPanel.h"
#include "UI/Panels/BrushPanel.h"
#include "UI/Panels/NavigatorPanel.h"
#include "Core/Project.h"
#include <memory>

namespace VividPic {
namespace UI {

class Workspace : public Window {
public:
    Workspace();
    ~Workspace() override = default;
    
    bool Initialize(Ref<Project> project);
    void SetProject(Ref<Project> project);
    
    CanvasView* GetCanvasView() const { return m_canvasView.get(); }
    
protected:
    void OnPaint(HDC hdc, const Rect& clip) override;
    void OnSize(const Size& newSize) override;
    void OnKeyDown(uint32_t keyCode) override;
    bool OnCreate() override;
    
    DWORD GetDefaultStyle() const override { return WS_OVERLAPPEDWINDOW; }
    
private:
    void LayoutPanels();
    void DrawMenuBar(HDC hdc);
    void DrawToolbar(HDC hdc);
    void SyncPanels();
    
    Ref<Project> m_project;
    Scope<CanvasView> m_canvasView;
    Scope<LayersPanel> m_layersPanel;
    Scope<ColorsPanel> m_colorsPanel;
    Scope<BrushPanel> m_brushPanel;
    Scope<NavigatorPanel> m_navigatorPanel;
    
    // Layout metrics
    static constexpr int MenuBarHeight = 24;
    static constexpr int ToolbarHeight = 32;
    static constexpr int LeftPanelWidth = 240;
    static constexpr int RightPanelWidth = 240;
    
    struct MenuItem {
        String name;
        std::vector<String> items;
    };
    std::vector<MenuItem> m_menus;
    
    void BuildMenus();
    void OnMenuItemClicked(int menuIndex, int itemIndex);
};

} // namespace UI
} // namespace VividPic
