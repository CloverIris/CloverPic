#include "UI/Widgets/ComboBox.h"

namespace VividPic {
namespace UI {

ComboBox::ComboBox() = default;

void ComboBox::AddItem(const String& item) {
    m_items.push_back(item);
    if (m_items.size() == 1) {
        m_selectedIndex = 0;
    }
    Invalidate();
}

void ComboBox::SetSelectedIndex(int index) {
    if (index >= 0 && index < static_cast<int>(m_items.size())) {
        m_selectedIndex = index;
        Invalidate();
    }
}

String ComboBox::GetSelectedItem() const {
    if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_items.size())) {
        return m_items[m_selectedIndex];
    }
    return String();
}

void ComboBox::OnPaint(HDC hdc, const Rect& clip) {
    Rect client = GetClientBounds();
    
    // Background
    uint32_t bgColor = m_hovered ? 0x454545 : 0x3C3C3C;
    HBRUSH brush = Theme::SolidBrush(bgColor);
    RECT rc = client.ToWin32Rect();
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);
    
    // Border
    HPEN borderPen = CreatePen(PS_SOLID, 1, Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
    HBRUSH nullBrush = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, nullBrush));
    Rectangle(hdc, client.left, client.top, client.right, client.bottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(borderPen);
    
    // Selected text
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::TextPrimary);
    
    HFONT font = CreateFontW(Theme::GetFontSize(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
    
    String displayText = GetSelectedItem();
    RECT textRect = { client.left + 6, client.top + 2, client.right - 24, client.bottom - 2 };
    DrawTextW(hdc, displayText.c_str(), -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    
    // Dropdown arrow
    HPEN arrowPen = CreatePen(PS_SOLID, 1, Theme::TextSecondary);
    SelectObject(hdc, arrowPen);
    int arrowX = client.right - 14;
    int arrowY = client.top + client.Height() / 2;
    MoveToEx(hdc, arrowX, arrowY - 2, nullptr);
    LineTo(hdc, arrowX + 6, arrowY - 2);
    LineTo(hdc, arrowX + 3, arrowY + 2);
    LineTo(hdc, arrowX, arrowY - 2);
    SelectObject(hdc, oldPen);
    DeleteObject(arrowPen);
    
    SelectObject(hdc, oldFont);
    DeleteObject(font);
    
    // Dropdown list
    if (m_dropdownOpen) {
        DrawDropdown(hdc);
    }
}

void ComboBox::DrawDropdown(HDC hdc) {
    Rect client = GetClientBounds();
    int itemHeight = 24;
    int listHeight = static_cast<int>(m_items.size()) * itemHeight;
    
    Rect listRect(client.left, client.bottom, client.right, client.bottom + listHeight);
    
    // Dropdown background
    HBRUSH bgBrush = Theme::SolidBrush(0x3C3C3C);
    RECT rc = listRect.ToWin32Rect();
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);
    
    // Border
    HPEN borderPen = CreatePen(PS_SOLID, 1, Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
    HBRUSH nullBrush = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, nullBrush));
    Rectangle(hdc, listRect.left, listRect.top, listRect.right, listRect.bottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(borderPen);
    
    // Items
    SetBkMode(hdc, TRANSPARENT);
    HFONT font = CreateFontW(Theme::GetFontSize(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
    
    for (size_t i = 0; i < m_items.size(); ++i) {
        int itemTop = listRect.top + static_cast<int>(i) * itemHeight;
        
        if (static_cast<int>(i) == m_hoverIndex) {
            HBRUSH hoverBrush = Theme::SolidBrush(Theme::HighlightBlue);
            RECT itemRc = { listRect.left + 1, itemTop, listRect.right - 1, itemTop + itemHeight };
            FillRect(hdc, &itemRc, hoverBrush);
            DeleteObject(hoverBrush);
        }
        
        SetTextColor(hdc, static_cast<int>(i) == m_selectedIndex ? Theme::HighlightBlue : Theme::TextPrimary);
        RECT textRect = { listRect.left + 6, itemTop, listRect.right - 6, itemTop + itemHeight };
        DrawTextW(hdc, m_items[i].c_str(), -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    }
    
    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

void ComboBox::OnMouseDown(const Point& pos, MouseButton button) {
    if (button == MouseButton::Left) {
        if (m_dropdownOpen) {
            int item = HitTestItem(pos);
            if (item >= 0) {
                m_selectedIndex = item;
                if (m_onChanged) {
                    m_onChanged(m_selectedIndex);
                }
            }
            CloseDropdown();
        } else {
            OpenDropdown();
        }
        Invalidate();
    }
}

void ComboBox::OnMouseUp(const Point& pos, MouseButton button) {
    if (button == MouseButton::Left && m_dropdownOpen) {
        int item = HitTestItem(pos);
        if (item >= 0) {
            m_selectedIndex = item;
            if (m_onChanged) {
                m_onChanged(m_selectedIndex);
            }
        }
        CloseDropdown();
        Invalidate();
    }
}

void ComboBox::OpenDropdown() {
    m_dropdownOpen = true;
    Rect bounds = GetBounds();
    m_originalHeight = bounds.Height();
    int itemHeight = 24;
    int listHeight = static_cast<int>(m_items.size()) * itemHeight;
    SetWindowPos(m_hwnd, HWND_TOP, bounds.left, bounds.top, bounds.Width(), m_originalHeight + listHeight, SWP_SHOWWINDOW);
    SetCapture(m_hwnd);
}

void ComboBox::CloseDropdown() {
    m_dropdownOpen = false;
    m_hoverIndex = -1;
    ReleaseCapture();
    if (m_originalHeight > 0) {
        Rect bounds = GetBounds();
        SetWindowPos(m_hwnd, nullptr, bounds.left, bounds.top, bounds.Width(), m_originalHeight, SWP_NOZORDER | SWP_SHOWWINDOW);
        m_originalHeight = 0;
    }
}

void ComboBox::OnMouseMove(const Point& pos) {
    if (m_dropdownOpen) {
        m_hoverIndex = HitTestItem(pos);
        Invalidate();
    }
    m_hovered = true;
}

void ComboBox::OnMouseLeave() {
    m_hovered = false;
    if (!m_dropdownOpen) {
        m_hoverIndex = -1;
        Invalidate();
    }
}

int ComboBox::HitTestItem(const Point& pos) const {
    Rect client = GetClientBounds();
    int itemHeight = 24;
    int relativeY = pos.y - client.bottom;
    
    if (relativeY < 0) return -1;
    
    int index = relativeY / itemHeight;
    if (index >= 0 && index < static_cast<int>(m_items.size())) {
        return index;
    }
    return -1;
}

LRESULT ComboBox::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_CAPTURECHANGED) {
        if (m_dropdownOpen && reinterpret_cast<HWND>(lParam) != m_hwnd) {
            CloseDropdown();
            Invalidate();
        }
        return 0;
    }
    return Window::HandleMessage(msg, wParam, lParam);
}

} // namespace UI
} // namespace VividPic
