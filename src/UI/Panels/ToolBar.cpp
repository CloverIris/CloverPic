#include "UI/Panels/ToolBar.h"
#include "UI/Core/Theme.h"

namespace VividPic {
namespace UI {

ToolBar::ToolBar() = default;

void ToolBar::SetCurrentTool(ToolType tool) {
    if (m_currentTool != tool) {
        m_currentTool = tool;
        Invalidate();
        if (m_onToolChanged) m_onToolChanged(tool);
    }
}

void ToolBar::OnPaint(HDC hdc, const Rect& clip) {
    Rect client = GetClientBounds();
    
    // Background
    HBRUSH bg = Theme::SolidBrush(Theme::BackgroundDark);
    RECT rc = client.ToWin32Rect();
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);
    
    // Right border
    HPEN pen = Theme::Pen(Theme::BorderDark);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    MoveToEx(hdc, client.Width() - 1, 0, nullptr);
    LineTo(hdc, client.Width() - 1, client.Height());
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
    
    // Draw tools
    int y = Theme::GetSize(4);
    for (int i = 0; i < ToolCount; ++i) {
        int itemH = Theme::GetSize(IconSize);
        int itemW = client.Width() - Theme::GetSize(4);
        int x = Theme::GetSize(2);
        Rect itemRc(x, y, x + itemW, y + itemH);
        bool active = (Tools[i].type == m_currentTool);
        bool hovered = (i == m_hoverIndex);
        DrawToolIcon(hdc, i, itemRc, active, hovered);
        y += itemH + Theme::GetSize(2);
    }
}

void ToolBar::DrawToolIcon(HDC hdc, int index, const Rect& rc, bool active, bool hovered) {
    // Background
    uint32_t bgColor = active ? Theme::HighlightBlue : (hovered ? Theme::ButtonHover : Theme::BackgroundDark);
    HBRUSH brush = Theme::SolidBrush(bgColor);
    RECT fillRc = rc.ToWin32Rect();
    FillRect(hdc, &fillRc, brush);
    DeleteObject(brush);
    
    // Border
    if (active || hovered) {
        HPEN pen = Theme::Pen(active ? Theme::HighlightHover : Theme::BorderLight);
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
        HBRUSH nullBr = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
        HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
        Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBr);
        DeleteObject(pen);
    }
    
    // Icon text
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, active ? Theme::TextInverse : Theme::TextPrimary);
    HFONT font = Theme::GetFont(Theme::FontID::Toolbar);
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
    RECT textRc = rc.ToWin32Rect();
    DrawTextW(hdc, Tools[index].icon, -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

void ToolBar::OnMouseDown(const Point& pos, MouseButton button) {
    if (button != MouseButton::Left) return;
    int idx = HitTest(pos);
    if (idx >= 0) {
        SetCurrentTool(Tools[idx].type);
    }
}

void ToolBar::OnMouseMove(const Point& pos) {
    int idx = HitTest(pos);
    if (idx != m_hoverIndex) {
        m_hoverIndex = idx;
        Invalidate();
    }
}

void ToolBar::OnMouseLeave() {
    if (m_hoverIndex >= 0) {
        m_hoverIndex = -1;
        Invalidate();
    }
}

int ToolBar::HitTest(const Point& pos) const {
    Rect client = GetClientBounds();
    int y = Theme::GetSize(4);
    for (int i = 0; i < ToolCount; ++i) {
        int itemH = Theme::GetSize(IconSize);
        int itemW = client.Width() - Theme::GetSize(4);
        int x = Theme::GetSize(2);
        if (pos.x >= x && pos.x < x + itemW && pos.y >= y && pos.y < y + itemH) {
            return i;
        }
        y += itemH + Theme::GetSize(2);
    }
    return -1;
}

} // namespace UI
} // namespace VividPic
