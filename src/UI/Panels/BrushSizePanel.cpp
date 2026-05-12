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
    HBRUSH bg = Theme::CachedBrush(Theme::PanelBackground);
    RECT rc = client.ToWin32Rect();
    FillRect(hdc, &rc, bg);
    
    // Modern panel header
    int titleH = Theme::GetSize(22);
    Rect headerRc(0, 0, client.Width(), titleH);
    Theme::DrawPanelHeaderModern(hdc, headerRc, L"笔刷尺寸", IsCollapsed());
    
    // Grid buttons
    int startY = Theme::GetSize(28);
    int btnSize = Theme::GetSize(ButtonSize);
    int spacing = Theme::GetSize(2);
    int startX = Theme::GetSize(4);
    
    HFONT valFont = Theme::GetCachedFont(Theme::FontID::Small);
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, valFont));
    
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
}

void BrushSizePanel::DrawButton(HDC hdc, int index, const Rect& rc, bool active, bool hovered) {
    int radius = Theme::GetSize(3);
    uint32_t bgColor = active ? Theme::HighlightBlue : (hovered ? Theme::ButtonHover : Theme::ButtonDefault);
    uint32_t borderColor = active ? Theme::HighlightHover : (hovered ? Theme::BorderLight : Theme::BorderDark);
    Theme::DrawRoundRect(hdc, rc, radius, bgColor, borderColor);
    
    // Hover: slight scale effect (simulate by drawing larger text)
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, active ? Theme::TextInverse : Theme::TextPrimary);
    RECT textRc = rc.ToWin32Rect();
    
    HFONT font = Theme::GetCachedFont(Theme::FontID::Small);
    if (hovered && !active) {
        // Slightly larger font for hover
        font = Theme::GetCachedFont(Theme::FontID::Label);
    }
    HFONT oldF = static_cast<HFONT>(SelectObject(hdc, font));
    
    std::wostringstream oss;
    if (Presets[index] >= 100.0f) {
        oss << static_cast<int>(Presets[index]);
    } else if (Presets[index] == static_cast<int>(Presets[index])) {
        oss << static_cast<int>(Presets[index]);
    } else {
        oss << Presets[index];
    }
    DrawTextW(hdc, oss.str().c_str(), -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
    SelectObject(hdc, oldF);
}

void BrushSizePanel::OnMouseDown(const Point& pos, MouseButton button) {
    if (button != MouseButton::Left) return;

    if (IsCollapsible() && pos.y < Theme::GetSize(26)) {
        ToggleCollapsed();
        return;
    }

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
