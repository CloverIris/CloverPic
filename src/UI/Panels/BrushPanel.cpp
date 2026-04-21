#include "UI/Panels/BrushPanel.h"
#include "UI/Core/Theme.h"
#include <algorithm>
#include <sstream>

namespace VividPic {
namespace UI {

BrushPanel::BrushPanel() = default;

void BrushPanel::OnPaint(HDC hdc, const Rect& clip) {
    Rect client = GetClientBounds();
    
    HBRUSH bgBrush = Theme::SolidBrush(Theme::PanelBackground);
    RECT rc = client.ToWin32Rect();
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);
    
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::TextSecondary);
    HFONT font = CreateFontW(12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
    
    RECT titleRc = { 8, 4, client.Width() - 8, 20 };
    DrawTextW(hdc, L"笔刷控制", -1, &titleRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    SelectObject(hdc, oldFont);
    DeleteObject(font);
    
    HPEN sepPen = CreatePen(PS_SOLID, 1, Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, sepPen));
    MoveToEx(hdc, 8, 22, nullptr);
    LineTo(hdc, client.Width() - 8, 22);
    SelectObject(hdc, oldPen);
    DeleteObject(sepPen);
    
    // Size slider
    DrawSlider(hdc, 8, 32, SliderWidth, m_brushSize / 100.0f, L"大小");
    
    // Opacity slider
    DrawSlider(hdc, 8, 70, SliderWidth, m_opacity, L"不透明度");
    
    // Preview
    int previewY = 110;
    SetTextColor(hdc, Theme::TextPrimary);
    HFONT valFont = CreateFontW(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    oldFont = static_cast<HFONT>(SelectObject(hdc, valFont));
    
    std::wostringstream oss;
    oss << L"大小: " << static_cast<int>(m_brushSize) << L"px";
    RECT sizeRc = { 8, previewY, client.Width() - 8, previewY + 18 };
    DrawTextW(hdc, oss.str().c_str(), -1, &sizeRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    
    oss.str(L"");
    oss << L"不透明度: " << static_cast<int>(m_opacity * 100) << L"%";
    RECT opRc = { 8, previewY + 18, client.Width() - 8, previewY + 36 };
    DrawTextW(hdc, oss.str().c_str(), -1, &opRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    
    SelectObject(hdc, oldFont);
    DeleteObject(valFont);
}

void BrushPanel::DrawSlider(HDC hdc, int x, int y, int width, float value, const String& label) {
    SetTextColor(hdc, Theme::TextSecondary);
    HFONT labelFont = CreateFontW(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, labelFont));
    RECT labelRc = { x, y - 2, x + 60, y + 14 };
    DrawTextW(hdc, label.c_str(), -1, &labelRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    SelectObject(hdc, oldFont);
    DeleteObject(labelFont);
    
    // Track
    HBRUSH trackBrush = Theme::SolidBrush(Theme::BorderDark);
    RECT trackRc = { x, y + 16, x + width, y + 16 + SliderHeight };
    FillRect(hdc, &trackRc, trackBrush);
    DeleteObject(trackBrush);
    
    // Fill
    uint32_t fillColor = Theme::HighlightBlue;
    int fillWidth = static_cast<int>(width * value);
    if (fillWidth > 0) {
        HBRUSH fillBrush = Theme::SolidBrush(fillColor);
        RECT fillRc = { x, y + 16, x + fillWidth, y + 16 + SliderHeight };
        FillRect(hdc, &fillRc, fillBrush);
        DeleteObject(fillBrush);
    }
    
    // Border
    HPEN borderPen = CreatePen(PS_SOLID, 1, Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
    HBRUSH nullBrush = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, nullBrush));
    Rectangle(hdc, x, y + 16, x + width, y + 16 + SliderHeight);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(borderPen);
}

void BrushPanel::OnMouseDown(const Point& pos, MouseButton button) {
    if (button != MouseButton::Left) return;
    
    int sliderY = 0;
    int slider = HitTestSlider(pos, sliderY);
    if (slider == 0) {
        m_draggingSize = true;
        m_brushSize = std::clamp(static_cast<float>(pos.x - 8) / SliderWidth * 100.0f, 1.0f, 100.0f);
        if (m_onSizeChanged) m_onSizeChanged(m_brushSize);
        Invalidate();
    } else if (slider == 1) {
        m_draggingOpacity = true;
        m_opacity = std::clamp(static_cast<float>(pos.x - 8) / SliderWidth, 0.0f, 1.0f);
        if (m_onOpacityChanged) m_onOpacityChanged(m_opacity);
        Invalidate();
    }
}

void BrushPanel::OnMouseMove(const Point& pos) {
    if (m_draggingSize) {
        m_brushSize = std::clamp(static_cast<float>(pos.x - 8) / SliderWidth * 100.0f, 1.0f, 100.0f);
        if (m_onSizeChanged) m_onSizeChanged(m_brushSize);
        Invalidate();
    } else if (m_draggingOpacity) {
        m_opacity = std::clamp(static_cast<float>(pos.x - 8) / SliderWidth, 0.0f, 1.0f);
        if (m_onOpacityChanged) m_onOpacityChanged(m_opacity);
        Invalidate();
    }
}

void BrushPanel::OnMouseUp(const Point& pos, MouseButton button) {
    m_draggingSize = false;
    m_draggingOpacity = false;
}

int BrushPanel::HitTestSlider(const Point& pos, int& outY) const {
    int sizeY = 32 + 16;
    int opY = 70 + 16;
    
    if (pos.y >= sizeY && pos.y < sizeY + SliderHeight && pos.x >= 8 && pos.x < 8 + SliderWidth) {
        outY = sizeY;
        return 0;
    }
    if (pos.y >= opY && pos.y < opY + SliderHeight && pos.x >= 8 && pos.x < 8 + SliderWidth) {
        outY = opY;
        return 1;
    }
    return -1;
}

} // namespace UI
} // namespace VividPic
