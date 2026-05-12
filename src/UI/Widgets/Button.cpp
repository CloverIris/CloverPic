#include "UI/Widgets/Button.h"
#include <shellapi.h>

namespace VividPic {
namespace UI {

Button::Button() {
    SetTabStop(true);
}

void Button::SetText(const String& text) {
    m_text = text;
    Invalidate();
}

void Button::OnPaint(HDC hdc, const Rect& clip) {
    (void)clip;
    Rect client = GetClientBounds();
    
    // Background
    uint32_t bgColor = m_defaultColor;
    if (m_disabled) {
        bgColor = Theme::PanelBackground;
    } else if (m_pressed) {
        bgColor = m_pressedColor;
    } else if (m_hovered) {
        bgColor = m_hoverColor;
    }
    
    HBRUSH brush = Theme::CachedBrush(bgColor);
    RECT rc = client.ToWin32Rect();
    FillRect(hdc, &rc, brush);
    
    // Border
    uint32_t borderColor = m_disabled ? Theme::BorderDark : (m_focused ? Theme::HighlightBlue : 0x222222);
    if (borderColor != 0) {
        HPEN borderPen = CreatePen(PS_SOLID, 1, borderColor);
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
        HBRUSH nullBrush = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, nullBrush));
        Rectangle(hdc, client.left, client.top, client.right, client.bottom);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(borderPen);
    }
    
    // Focus dotted rect (inside, 2px padding)
    if (m_focused && !m_disabled) {
        HPEN dotPen = CreatePen(PS_DOT, 1, Theme::HighlightBlue);
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, dotPen));
        HBRUSH nullBrush = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, nullBrush));
        Rectangle(hdc, client.left + 2, client.top + 2, client.right - 2, client.bottom - 2);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(dotPen);
    }
    
    // Color tag (for brush list)
    int leftOffset = Theme::GetSize(8);
    if (m_hasColorTag) {
        RECT tagRect = { client.left + Theme::GetSize(4), client.top + Theme::GetSize(8),
                         client.left + Theme::GetSize(8), client.bottom - Theme::GetSize(8) };
        HBRUSH tagBrush = Theme::CachedBrush(m_colorTag);
        FillRect(hdc, &tagRect, tagBrush);
        leftOffset = Theme::GetSize(16);
    }
    
    // Size preset (for brush list)
    if (m_hasSizePreset) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, m_disabled ? Theme::TextDisabled : Theme::TextSecondary);
        HFONT font = Theme::GetCachedFont(Theme::FontID::Value);
        HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
        
        String presetText = std::to_wstring(m_sizePreset);
        RECT presetRect = { client.left + leftOffset, client.top,
                            client.left + leftOffset + Theme::GetSize(30), client.bottom };
        DrawTextW(hdc, presetText.c_str(), -1, &presetRect, 
                  DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        SelectObject(hdc, oldFont);
        leftOffset += Theme::GetSize(32);
    }
    
    // Icon
    if (m_hasIcon && m_iconChar != L'\0') {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, m_disabled ? Theme::TextDisabled : Theme::TextPrimary);
        HFONT font = Theme::GetCachedFont(Theme::FontID::Toolbar);
        HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
        
        wchar_t iconStr[2] = { m_iconChar, L'\0' };
        SIZE iconSize;
        GetTextExtentPoint32W(hdc, iconStr, 1, &iconSize);
        int iconY = client.top + (client.Height() - iconSize.cy) / 2;
        TextOutW(hdc, client.left + leftOffset, iconY, iconStr, 1);
        
        SelectObject(hdc, oldFont);
        leftOffset += iconSize.cx + Theme::GetSize(4);
    }
    
    // Text
    if (!m_text.empty()) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, m_disabled ? Theme::TextDisabled : Theme::TextPrimary);
        
        HFONT font = Theme::GetCachedFont(Theme::FontID::Button);
        HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
        
        RECT textRect = { client.left + leftOffset, client.top, client.right - Theme::GetSize(8), client.bottom };
        DrawTextW(hdc, m_text.c_str(), -1, &textRect, 
                  DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        
        SelectObject(hdc, oldFont);
    }
}

void Button::OnMouseDown(const Point& pos, MouseButton button) {
    (void)pos;
    if (button == MouseButton::Left && !m_disabled) {
        m_pressed = true;
        Invalidate();
    }
}

void Button::OnMouseUp(const Point& pos, MouseButton button) {
    (void)pos;
    if (button == MouseButton::Left && m_pressed) {
        m_pressed = false;
        Invalidate();
        if (!m_disabled && m_callback) {
            m_callback();
        }
    }
}

void Button::OnMouseEnter() {
    m_hovered = true;
    Invalidate();
}

void Button::OnMouseLeave() {
    m_hovered = false;
    m_pressed = false;
    Invalidate();
}

void Button::OnKeyDown(uint32_t keyCode) {
    if (m_disabled) return;
    if (keyCode == VK_SPACE || keyCode == VK_RETURN) {
        m_pressed = true;
        Invalidate();
    }
}

void Button::OnKeyUp(uint32_t keyCode) {
    if (m_disabled) return;
    if ((keyCode == VK_SPACE || keyCode == VK_RETURN) && m_pressed) {
        m_pressed = false;
        Invalidate();
        if (m_callback) {
            m_callback();
        }
    }
}

LRESULT Button::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SETFOCUS:
            m_focused = true;
            Invalidate();
            return 0;
        case WM_KILLFOCUS:
            m_focused = false;
            m_pressed = false;
            Invalidate();
            return 0;
    }
    return Window::HandleMessage(msg, wParam, lParam);
}

} // namespace UI
} // namespace VividPic
