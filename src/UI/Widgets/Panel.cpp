#include "UI/Widgets/Panel.h"

namespace VividPic {
namespace UI {

Panel::Panel() = default;

void Panel::OnPaint(HDC hdc, const Rect& clip) {
    Rect client = GetClientBounds();
    
    // Background
    HBRUSH brush = Theme::CachedBrush(m_bgColor);
    RECT rc = client.ToWin32Rect();
    FillRect(hdc, &rc, brush);
    
    // Title
    if (!m_title.empty()) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, Theme::TextSecondary);
        
        HFONT font = Theme::GetCachedFont(Theme::FontID::PanelTitle);
        HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
        
        RECT titleRect = { client.left + 8, client.top + 4, client.right - 8, client.top + 24 };
        DrawTextW(hdc, m_title.c_str(), -1, &titleRect, 
                  DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        
        SelectObject(hdc, oldFont);
        
        // Separator line
        HPEN linePen = CreatePen(PS_SOLID, 1, RGB(0x55, 0x55, 0x55));
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, linePen));
        MoveToEx(hdc, client.left + 8, client.top + 26, nullptr);
        LineTo(hdc, client.right - 8, client.top + 26);
        SelectObject(hdc, oldPen);
        DeleteObject(linePen);
    }
}

} // namespace UI
} // namespace VividPic
