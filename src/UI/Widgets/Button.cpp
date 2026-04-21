#include "UI/Widgets/Button.h"
#include <shellapi.h>

namespace VividPic {
namespace UI {

Button::Button() = default;

void Button::SetText(const String& text) {
    m_text = text;
    Invalidate();
}

void Button::OnPaint(HDC hdc, const Rect& clip) {
    Rect client = GetClientBounds();
    
    // Background
    uint32_t bgColor = m_defaultColor;
    if (m_pressed) {
        bgColor = m_pressedColor;
    } else if (m_hovered) {
        bgColor = m_hoverColor;
    }
    
    HBRUSH brush = Theme::SolidBrush(bgColor);
    RECT rc = client.ToWin32Rect();
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);
    
    // Border
    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(0x22, 0x22, 0x22));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
    HBRUSH nullBrush = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, nullBrush));
    Rectangle(hdc, client.left, client.top, client.right, client.bottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(borderPen);
    
    // Color tag (for brush list)
    int leftOffset = 8;
    if (m_hasColorTag) {
        RECT tagRect = { client.left + 4, client.top + 8, client.left + 8, client.bottom - 8 };
        HBRUSH tagBrush = Theme::SolidBrush(m_colorTag);
        FillRect(hdc, &tagRect, tagBrush);
        DeleteObject(tagBrush);
        leftOffset = 16;
    }
    
    // Size preset (for brush list)
    if (m_hasSizePreset) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, Theme::TextSecondary);
        HFONT font = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
        
        String presetText = std::to_wstring(m_sizePreset);
        RECT presetRect = { client.left + leftOffset, client.top, client.left + leftOffset + 30, client.bottom };
        DrawTextW(hdc, presetText.c_str(), -1, &presetRect, 
                  DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        SelectObject(hdc, oldFont);
        DeleteObject(font);
        leftOffset += 32;
    }
    
    // Text
    if (!m_text.empty()) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, Theme::TextPrimary);
        
        HFONT font = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
        HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
        
        RECT textRect = { client.left + leftOffset, client.top, client.right - 8, client.bottom };
        DrawTextW(hdc, m_text.c_str(), -1, &textRect, 
                  DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        
        SelectObject(hdc, oldFont);
        DeleteObject(font);
    }
}

void Button::OnMouseDown(const Point& pos, MouseButton button) {
    if (button == MouseButton::Left) {
        m_pressed = true;
        Invalidate();
    }
}

void Button::OnMouseUp(const Point& pos, MouseButton button) {
    if (button == MouseButton::Left && m_pressed) {
        m_pressed = false;
        Invalidate();
        if (m_callback) {
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

} // namespace UI
} // namespace VividPic
