#pragma once

#include "UI/Core/Window.h"
#include <vector>
#include <functional>

namespace VividPic {
namespace UI {

// -------------------------------------------------------------------------
// ContextMenu — lightweight popup menu for right-click context
// Similar to MenuDropdownWindow but standalone and reusable.
// -------------------------------------------------------------------------
class ContextMenu : public Window {
public:
    ContextMenu();
    
    struct Item {
        String label;
        bool enabled = true;
        int id = 0;
    };
    
    void SetItems(const std::vector<Item>& items);
    void ShowAt(int screenX, int screenY, Window* parent);
    void SetCallback(std::function<void(int)> callback);

protected:
    void OnPaint(HDC hdc, const Rect& clip) override;
    void OnMouseDown(const Point& pos, MouseButton button) override;
    void OnMouseMove(const Point& pos) override;
    void OnMouseLeave() override;
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) override;
    
    DWORD GetDefaultStyle() const override { return WS_POPUP; }
    DWORD GetDefaultExStyle() const override { return WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST; }

private:
    std::vector<Item> m_items;
    std::function<void(int)> m_callback;
    int m_hoverIndex = -1;
    int m_itemHeight = 24;
    int m_maxWidth = 120;
    
    int HitTestItem(const Point& pos) const;
    void UpdateSize();
};

} // namespace UI
} // namespace VividPic
