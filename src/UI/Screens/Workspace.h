#pragma once

#include "UI/Core/Window.h"
#include "UI/Widgets/CanvasView.h"
#include "UI/Panels/LayersPanel.h"
#include "UI/Panels/ColorsPanel.h"
#include "UI/Panels/BrushPanel.h"
#include "UI/Panels/NavigatorPanel.h"
#include "UI/Panels/ToolBar.h"
#include "UI/Panels/BrushSizePanel.h"
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
    void OnMouseDown(const Point& pos, MouseButton button) override;
    void OnKeyDown(uint32_t keyCode) override;
    bool OnCreate() override;
    
    DWORD GetDefaultStyle() const override { return WS_OVERLAPPEDWINDOW; }
    
private:
    void LayoutPanels();
    void DrawMenuBar(HDC hdc);
    void DrawToolbar(HDC hdc);
    void DrawStatusBar(HDC hdc);
    void SyncPanels();
    
    Ref<Project> m_project;
    String m_currentFilePath;
    Scope<CanvasView> m_canvasView;
    Scope<LayersPanel> m_layersPanel;
    Scope<ColorsPanel> m_colorsPanel;
    Scope<BrushPanel> m_brushPanel;
    Scope<NavigatorPanel> m_navigatorPanel;
    Scope<ToolBar> m_toolBar;
    Scope<BrushSizePanel> m_brushSizePanel;
    
    // Layout metrics
    static constexpr int MenuBarHeight = 26;
    static constexpr int ToolbarHeight = 36;
    static constexpr int ToolBarWidth = 40;
    static constexpr int StatusBarHeight = 26;
    static constexpr int LeftPanelWidth = 240;
    static constexpr int RightPanelWidth = 240;
    
    struct MenuItem {
        String name;
        std::vector<String> items;
    };
    std::vector<MenuItem> m_menus;
    std::vector<Rect> m_menuItemRects; // Cached bounds for alignment
    
    void BuildMenus();
    void OnMenuItemClicked(int menuIndex, int itemIndex);
    void CaptureLayerSnapshot(class Layer* layer);
    
    int HitTestMenuItem(const Point& pos) const;
    int HitTestToolbarButton(const Point& pos) const;
    void DrawToolbarButtons(HDC hdc);
    void DrawMenuDropdown(HDC hdc);
    
    int m_openMenuIndex = -1;
    int m_menuHoverItem = -1;
    
    // Popup dropdown window to avoid child-window occlusion
    class MenuDropdownWindow : public Window {
    public:
        void SetItems(const std::vector<String>& items, int menuIndex) {
            m_items = items;
            m_menuIndex = menuIndex;
        }
        void SetCallback(std::function<void(int, int)> cb) { m_callback = cb; }
        void SetHoverIndex(int idx) { m_hoverIndex = idx; Invalidate(); }
    protected:
        void OnPaint(HDC hdc, const Rect& clip) override;
        void OnMouseDown(const Point& pos, MouseButton button) override;
        void OnMouseMove(const Point& pos) override;
        void OnMouseLeave() override;
        LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) override;
        DWORD GetDefaultStyle() const override { return WS_POPUP | WS_VISIBLE; }
        DWORD GetDefaultExStyle() const override { return WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE; }
    private:
        std::vector<String> m_items;
        std::function<void(int, int)> m_callback;
        int m_menuIndex = -1;
        int m_hoverIndex = -1;
    };
    Scope<MenuDropdownWindow> m_dropdownWindow;
    void ShowMenuDropdown(int menuIndex, int x, int y, int width);
    void HideMenuDropdown();
    
    void DoSave(bool saveAs);
    void RefreshStatusBar();
    String m_saveStatusText;
    uint64_t m_saveStatusTime = 0;
};

} // namespace UI
} // namespace VividPic
