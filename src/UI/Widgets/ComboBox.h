#pragma once

#include "UI/Core/Window.h"
#include "UI/Core/Theme.h"
#include <vector>

namespace VividPic {
namespace UI {

class ComboBox : public Window {
public:
    ComboBox();
    
    void AddItem(const String& item);
    void SetSelectedIndex(int index);
    int GetSelectedIndex() const { return m_selectedIndex; }
    String GetSelectedItem() const;
    
    void SetOnSelectionChanged(CallbackInt callback) { m_onChanged = callback; }
    
    bool IsDropdownOpen() const { return m_dropdownOpen; }
    
protected:
    void OnPaint(HDC hdc, const Rect& clip) override;
    void OnMouseDown(const Point& pos, MouseButton button) override;
    void OnMouseMove(const Point& pos) override;
    void OnMouseLeave() override;
    
    DWORD GetDefaultStyle() const override { return WS_CHILD | WS_VISIBLE; }
    
private:
    std::vector<String> m_items;
    int m_selectedIndex = 0;
    bool m_dropdownOpen = false;
    bool m_hovered = false;
    int m_hoverIndex = -1;
    CallbackInt m_onChanged;
    
    void DrawDropdown(HDC hdc);
    int HitTestItem(const Point& pos) const;
};

} // namespace UI
} // namespace VividPic
