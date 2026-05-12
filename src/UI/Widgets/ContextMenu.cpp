#include "UI/Widgets/ContextMenu.h"
#include "UI/Core/Theme.h"
#include <algorithm>

namespace VividPic {
namespace UI {

ContextMenu::ContextMenu() = default;

void ContextMenu::SetItems(const std::vector<Item>& items) {
    m_items = items;
    UpdateSize();
    Invalidate();
}

void ContextMenu::SetCallback(std::function<void(int)> callback) {
    m_callback = callback;
}

void ContextMenu::UpdateSize() {
    m_itemHeight = Theme::GetSize(24);
    m_maxWidth = Theme::GetSize(120);
    
    // Measure widest item
    HDC hdc = GetDC(m_hwnd ? m_hwnd : GetDesktopWindow());
    HFONT font = Theme::GetCachedFont(Theme::FontID::Label);
    HFONT old = static_cast<HFONT>(SelectObject(hdc, font));
    for (const auto& item : m_items) {
        SIZE ts;
        GetTextExtentPoint32W(hdc, item.label.c_str(), static_cast<int>(item.label.length()), &ts);
        if (ts.cx + Theme::GetSize(24) > m_maxWidth) {
            m_maxWidth = ts.cx + Theme::GetSize(24);
        }
    }
    SelectObject(hdc, old);
    ReleaseDC(m_hwnd ? m_hwnd : GetDesktopWindow(), hdc);
}

void ContextMenu::ShowAt(int screenX, int screenY, Window* parent) {
    if (!m_hwnd) {
        Create(L"", Rect(screenX, screenY, screenX + 100, screenY + 100), parent);
    }
    
    int h = static_cast<int>(m_items.size()) * m_itemHeight + 4;
    
    // Ensure stays on screen
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    if (screenX + m_maxWidth > screenW) screenX = screenW - m_maxWidth;
    if (screenY + h > screenH) screenY = screenH - h;
    
    SetWindowPos(m_hwnd, HWND_TOPMOST, screenX, screenY, m_maxWidth, h, SWP_SHOWWINDOW | SWP_NOACTIVATE);
    SetCapture(m_hwnd);
    m_hoverIndex = -1;
    Invalidate();
}

void ContextMenu::OnPaint(HDC hdc, const Rect& clip) {
    (void)clip;
    Rect client = GetClientBounds();
    
    // Background
    HBRUSH bg = Theme::CachedBrush(Theme::PanelBackground);
    RECT rc = client.ToWin32Rect();
    FillRect(hdc, &rc, bg);
    
    // Border
    HPEN pen = Theme::Pen(Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    HBRUSH nullBr = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
    Rectangle(hdc, 0, 0, client.Width(), client.Height());
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(pen);
    
    // Items
    HFONT font = Theme::GetCachedFont(Theme::FontID::Label);
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
    SetBkMode(hdc, TRANSPARENT);
    
    for (size_t i = 0; i < m_items.size(); ++i) {
        int iy = static_cast<int>(i) * m_itemHeight + 2;
        
        if (static_cast<int>(i) == m_hoverIndex && m_items[i].enabled) {
            HBRUSH hover = Theme::CachedBrush(Theme::HighlightBlue);
            RECT hoverRc = { 2, iy, client.Width() - 2, iy + m_itemHeight };
            FillRect(hdc, &hoverRc, hover);
            SetTextColor(hdc, Theme::TextInverse);
        } else {
            SetTextColor(hdc, m_items[i].enabled ? Theme::TextPrimary : Theme::TextDisabled);
        }
        
        RECT textRc = { Theme::GetSize(8), iy, client.Width() - Theme::GetSize(8), iy + m_itemHeight };
        DrawTextW(hdc, m_items[i].label.c_str(), -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    }
    
    SelectObject(hdc, oldFont);
}

int ContextMenu::HitTestItem(const Point& pos) const {
    if (pos.x < 0 || pos.x >= m_maxWidth || pos.y < 0) return -1;
    int idx = (pos.y - 2) / m_itemHeight;
    if (idx < 0 || idx >= static_cast<int>(m_items.size())) return -1;
    return idx;
}

void ContextMenu::OnMouseDown(const Point& pos, MouseButton button) {
    if (button == MouseButton::Left) {
        int idx = HitTestItem(pos);
        if (idx >= 0 && m_items[idx].enabled) {
            if (m_callback) {
                m_callback(m_items[idx].id);
            }
        }
        ReleaseCapture();
        SetVisible(false);
    }
}

void ContextMenu::OnMouseMove(const Point& pos) {
    int idx = HitTestItem(pos);
    if (idx != m_hoverIndex) {
        m_hoverIndex = idx;
        Invalidate();
    }
}

void ContextMenu::OnMouseLeave() {
    if (m_hoverIndex >= 0) {
        m_hoverIndex = -1;
        Invalidate();
    }
}

LRESULT ContextMenu::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CAPTURECHANGED:
            SetVisible(false);
            return 0;
    }
    return Window::HandleMessage(msg, wParam, lParam);
}

} // namespace UI
} // namespace VividPic
