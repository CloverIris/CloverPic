#include "UI/Widgets/TooltipWindow.h"

namespace VividPic {
namespace UI {

TooltipWindow::TooltipWindow() = default;

void TooltipWindow::ShowAt(int screenX, int screenY, const wchar_t* text) {
    m_text = text ? text : L"";
    if (m_text.empty()) {
        Hide();
        return;
    }
    
    // Measure text
    HDC hdc = GetDC(nullptr);
    HFONT font = Theme::GetCachedFont(Theme::FontID::Small);
    HFONT old = static_cast<HFONT>(SelectObject(hdc, font));
    SIZE ts;
    GetTextExtentPoint32W(hdc, m_text.c_str(), static_cast<int>(m_text.length()), &ts);
    SelectObject(hdc, old);
    ReleaseDC(nullptr, hdc);
    
    int pad = Theme::GetSize(6);
    int w = ts.cx + pad * 2;
    int h = ts.cy + pad * 2;
    
    int x = screenX + Theme::GetSize(12);
    int y = screenY + Theme::GetSize(20);
    
    // Keep on screen
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    if (x + w > sw) x = screenX - w - Theme::GetSize(4);
    if (y + h > sh) y = screenY - h - Theme::GetSize(4);
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    
    if (!m_hwnd) {
        Create(L"", Rect(x, y, x + w, y + h), nullptr);
    } else {
        SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
    m_showing = true;
    Invalidate();
}

void TooltipWindow::Hide() {
    if (m_showing && m_hwnd) {
        ShowWindow(m_hwnd, SW_HIDE);
    }
    m_showing = false;
}

void TooltipWindow::OnPaint(HDC hdc, const Rect& clip) {
    (void)clip;
    Rect client = GetClientBounds();
    
    // Background
    HBRUSH bg = Theme::CachedBrush(Theme::TooltipBackground);
    RECT rc = client.ToWin32Rect();
    FillRect(hdc, &rc, bg);
    
    // Border
    HPEN pen = CreatePen(PS_SOLID, 1, Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    HBRUSH nullBr = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
    Rectangle(hdc, 0, 0, client.Width(), client.Height());
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(pen);
    
    // Text
    if (!m_text.empty()) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, Theme::TextPrimary);
        HFONT font = Theme::GetCachedFont(Theme::FontID::Small);
        HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
        RECT tr = { Theme::GetSize(6), Theme::GetSize(4), client.Width() - Theme::GetSize(6), client.Height() - Theme::GetSize(4) };
        DrawTextW(hdc, m_text.c_str(), -1, &tr, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        SelectObject(hdc, oldFont);
    }
}

} // namespace UI
} // namespace VividPic
