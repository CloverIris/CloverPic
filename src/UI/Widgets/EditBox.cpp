#include "UI/Widgets/EditBox.h"

namespace VividPic {
namespace UI {

EditBox::EditBox() = default;

bool EditBox::OnCreate() {
    return true;
}

void EditBox::SetText(const String& text) {
    m_text = text;
    Invalidate();
}

void EditBox::OnPaint(HDC hdc, const Rect& clip) {
    Rect client = GetClientBounds();
    
    // Background
    uint32_t bgColor = m_focused ? 0x2A2A2A : 0x333333;
    HBRUSH brush = Theme::SolidBrush(bgColor);
    RECT rc = client.ToWin32Rect();
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);
    
    // Border
    HPEN borderPen = CreatePen(PS_SOLID, 1, m_focused ? Theme::HighlightBlue : Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
    HBRUSH nullBrush = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, nullBrush));
    Rectangle(hdc, client.left, client.top, client.right, client.bottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(borderPen);
    
    // Text
    if (!m_text.empty()) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, Theme::TextPrimary);
        
        HFONT font = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
        
        RECT textRect = { client.left + 6, client.top + 2, client.right - 6, client.bottom - 2 };
        DrawTextW(hdc, m_text.c_str(), -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        
        SelectObject(hdc, oldFont);
        DeleteObject(font);
    }
}

void EditBox::OnMouseDown(const Point& pos, MouseButton button) {
    if (button == MouseButton::Left) {
        SetFocus();
        m_focused = true;
        Invalidate();
    }
}

void EditBox::OnKeyDown(uint32_t keyCode) {
    if (m_readOnly) return;
    
    switch (keyCode) {
        case VK_BACK:
            DeleteChar();
            break;
        case VK_RETURN:
            m_focused = false;
            Invalidate();
            break;
    }
}

void EditBox::OnKeyUp(uint32_t keyCode) {
    // Modifier keys handled here if needed
}

void EditBox::OnChar(wchar_t ch) {
    if (ch == L'\b') {
        DeleteChar();
    } else if (ch == L'\r' || ch == L'\n') {
        m_focused = false;
        Invalidate();
    } else if (ch >= 32) {
        InsertChar(ch);
    }
}

void EditBox::InsertChar(wchar_t ch) {
    if (m_readOnly) return;
    
    if (m_numericOnly) {
        if (!((ch >= L'0' && ch <= L'9') || ch == L'.' || ch == L'-')) {
            return;
        }
    }
    
    m_text += ch;
    Invalidate();
    NotifyChanged();
}

void EditBox::DeleteChar() {
    if (!m_text.empty()) {
        m_text.pop_back();
        Invalidate();
        NotifyChanged();
    }
}

void EditBox::NotifyChanged() {
    if (m_onChanged) {
        m_onChanged();
    }
}

} // namespace UI
} // namespace VividPic
