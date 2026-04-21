#include "UI/Panels/BrushSizePanel.h"
#include "UI/Core/Theme.h"
#include <sstream>

namespace VividPic {
namespace UI {

constexpr float BrushSizePanel::Presets[PresetCount] = {
    1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 5.0f, 7.0f, 10.0f,
    12.0f, 15.0f, 20.0f, 25.0f, 30.0f, 40.0f, 50.0f, 70.0f,
    100.0f, 150.0f, 200.0f, 300.0f, 400.0f, 500.0f, 700.0f, 1000.0f
};

BrushSizePanel::BrushSizePanel() = default;

void BrushSizePanel::SetCurrentSize(float size) {
    m_currentSize = size;
    Invalidate();
}

void BrushSizePanel::Refresh() {
    Invalidate();
}

void BrushSizePanel::OnPaint(HDC hdc, const Rect& clip) {
    Rect client = GetClientBounds();
    
    // Background
    HBRUSH bg = Theme::SolidBrush(Theme::PanelBackground);
    RECT rc = client.ToWin32Rect();
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);
    
    // Title
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::TextSecondary);
    HFONT titleFont = Theme::GetFont(Theme::FontID::PanelTitle);
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, titleFont));
    RECT titleRc = { Theme::GetSize(8), Theme::GetSize(4), client.Width() - Theme::GetSize(8), Theme::GetSize(22) };
    DrawTextW(hdc, L"笔刷尺寸", -1, &titleRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    SelectObject(hdc, oldFont);
    DeleteObject(titleFont);
    
    // Separator
    HPEN sepPen = Theme::Pen(Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, sepPen));
    MoveToEx(hdc, Theme::GetSize(8), Theme::GetSize(24), nullptr);
    LineTo(hdc, client.Width() - Theme::GetSize(8), Theme::GetSize(24));
    SelectObject(hdc, oldPen);
    DeleteObject(sepPen);
    
    // Grid buttons
    int startY = Theme::GetSize(28);
    int btnSize = Theme::GetSize(ButtonSize);
    int spacing = Theme::GetSize(2);
    int startX = Theme::GetSize(4);
    
    HFONT valFont = Theme::GetFont(Theme::FontID::Small);
    oldFont = static_cast<HFONT>(SelectObject(hdc, valFont));
    
    for (int i = 0; i < PresetCount; ++i) {
        int row = i / ButtonsPerRow;
        int col = i % ButtonsPerRow;
        int x = startX + col * (btnSize + spacing);
        int y = startY + row * (btnSize + spacing);
        Rect btnRc(x, y, x + btnSize, y + btnSize);
        bool active = (m_currentSize == Presets[i]);
        bool hovered = (i == m_hoverIndex);
        DrawButton(hdc, i, btnRc, active, hovered);
    }
    
    SelectObject(hdc, oldFont);
    DeleteObject(valFont);
}

void BrushSizePanel::DrawButton(HDC hdc, int index, const Rect& rc, bool active, bool hovered) {
    uint32_t bgColor = active ? Theme::HighlightBlue : (hovered ? Theme::ButtonHover : Theme::ButtonDefault);
    HBRUSH brush = Theme::SolidBrush(bgColor);
    RECT fillRc = rc.ToWin32Rect();
    FillRect(hdc, &fillRc, brush);
    DeleteObject(brush);
    
    HPEN border = Theme::Pen(active ? Theme::HighlightHover : Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, border));
    HBRUSH nullBr = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(border);
    
    // Label
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, active ? Theme::TextInverse : Theme::TextPrimary);
    RECT textRc = rc.ToWin32Rect();
    
    std::wostringstream oss;
    if (Presets[index] >= 100.0f) {
        oss << static_cast<int>(Presets[index]);
    } else if (Presets[index] == static_cast<int>(Presets[index])) {
        oss << static_cast<int>(Presets[index]);
    } else {
        oss << Presets[index];
    }
    DrawTextW(hdc, oss.str().c_str(), -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
}

void BrushSizePanel::OnMouseDown(const Point& pos, MouseButton button) {
    if (button != MouseButton::Left) return;
    int idx = HitTest(pos);
    if (idx >= 0) {
        m_currentSize = Presets[idx];
        Invalidate();
        if (m_onSizeChanged) m_onSizeChanged(Presets[idx]);
    }
}

void BrushSizePanel::OnMouseMove(const Point& pos) {
    int idx = HitTest(pos);
    if (idx != m_hoverIndex) {
        m_hoverIndex = idx;
        Invalidate();
    }
}

void BrushSizePanel::OnMouseLeave() {
    if (m_hoverIndex >= 0) {
        m_hoverIndex = -1;
        Invalidate();
    }
}

int BrushSizePanel::HitTest(const Point& pos) const {
    int startY = Theme::GetSize(28);
    int btnSize = Theme::GetSize(ButtonSize);
    int spacing = Theme::GetSize(2);
    int startX = Theme::GetSize(4);
    
    for (int i = 0; i < PresetCount; ++i) {
        int row = i / ButtonsPerRow;
        int col = i % ButtonsPerRow;
        int x = startX + col * (btnSize + spacing);
        int y = startY + row * (btnSize + spacing);
        if (pos.x >= x && pos.x < x + btnSize && pos.y >= y && pos.y < y + btnSize) {
            return i;
        }
    }
    return -1;
}

} // namespace UI
} // namespace VividPic
