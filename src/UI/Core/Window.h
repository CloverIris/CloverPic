#pragma once

#include "Utils/Types.h"
#include <vector>
#include <string>

namespace VividPic {
namespace UI {

class Window {
public:
    Window();
    virtual ~Window();
    
    // Creation
    bool Create(const String& title, const Rect& bounds, Window* parent = nullptr, DWORD style = 0, DWORD exStyle = 0);
    void Destroy();
    bool IsCreated() const { return m_hwnd != nullptr; }
    
    // Window handle
    HWND GetHandle() const { return m_hwnd; }
    Window* GetParent() const { return m_parent; }
    
    // Hierarchy
    void AddChild(Window* child);
    void RemoveChild(Window* child);
    const std::vector<Window*>& GetChildren() const { return m_children; }
    
    // Layout
    void SetBounds(const Rect& bounds);
    Rect GetBounds() const;
    Rect GetClientBounds() const;
    
    // DPI scaling
    float GetDpiScale() const { return m_dpiScale; }
    static int ScaleSize(int base);
    static int ScaleFont(int base);
    void SetVisible(bool visible);
    bool IsVisible() const;
    void Invalidate();
    void InvalidateRect(const Rect& rect);
    
    // Focus
    void SetFocus();
    bool HasFocus() const;
    
    // Tooltip
    void SetTooltip(const wchar_t* tooltip);
    
    // Collapsible panel support
    void SetCollapsible(bool collapsible);
    bool IsCollapsible() const;
    void SetCollapsed(bool collapsed);
    bool IsCollapsed() const;
    void ToggleCollapsed();
    void SetOnCollapsedChanged(Callback callback);
    
    // Tab navigation
    void SetTabStop(bool tabStop);
    bool IsTabStop() const;
    void SetTabOrder(int order);
    int GetTabOrder() const;
    bool NavigateTab(bool forward);
    
    // Text
    void SetText(const String& text);
    String GetText() const;
    
    // Event handlers (override in derived classes)
    virtual void OnPaint(HDC hdc, const Rect& clip);
    virtual void OnMouseMove(const Point& pos);
    virtual void OnMouseDown(const Point& pos, MouseButton button);
    virtual void OnMouseUp(const Point& pos, MouseButton button);
    virtual void OnMouseDoubleClick(const Point& pos, MouseButton button);
    virtual void OnMouseWheel(int delta);
    virtual void OnMouseEnter();
    virtual void OnMouseLeave();
    virtual void OnSize(const Size& newSize);
    virtual void OnKeyDown(uint32_t keyCode);
    virtual void OnKeyUp(uint32_t keyCode);
    virtual void OnChar(wchar_t ch);
    virtual void OnCommand(uint32_t id);
    virtual void OnNotify(NMHDR* nmhdr);
    virtual bool OnCreate();
    virtual void OnDestroy();
    
    // Static window procedure
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
protected:
    void RegisterWindowClass();
    virtual DWORD GetDefaultStyle() const { return WS_OVERLAPPEDWINDOW; }
    virtual DWORD GetDefaultExStyle() const { return 0; }
    
    virtual LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    
    HWND m_hwnd = nullptr;
    Window* m_parent = nullptr;
    std::vector<Window*> m_children;
    String m_className;
    bool m_mouseTracking = false;
    bool m_mouseInside = false;
    float m_dpiScale = 1.0f;
    String m_tooltip;
    bool m_tooltipPending = false;
    bool m_collapsible = false;
    bool m_collapsed = false;
    Callback m_onCollapsedChanged;
    bool m_tabStop = false;
    int m_tabOrder = 0;
    
    static constexpr wchar_t BaseClassName[] = L"VividPic_Window";
    static bool s_classRegistered;
    static uint32_t s_windowId;
    static class TooltipWindow* s_tooltipWindow;
    static constexpr uint32_t TooltipTimerId = 0x7F00;
};

} // namespace UI
} // namespace VividPic
