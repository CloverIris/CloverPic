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

    // Tip type buttons (row of 5 small buttons)
    DrawTipButtons(hdc, 8, 28);

    // Sliders
    int sliderY = 54;
    std::wostringstream oss;

    oss << static_cast<int>(m_brushSize) << L"px";
    DrawSlider(hdc, 8, sliderY, SliderWidth, std::min(m_brushSize / 500.0f, 1.0f), L"大小", oss.str());

    oss.str(L"");
    oss << static_cast<int>(m_opacity * 100) << L"%";
    DrawSlider(hdc, 8, sliderY + 34, SliderWidth, m_opacity, L"不透明度", oss.str());

    oss.str(L"");
    oss << static_cast<int>(m_flow * 100) << L"%";
    DrawSlider(hdc, 8, sliderY + 68, SliderWidth, m_flow, L"流量", oss.str());

    oss.str(L"");
    oss << static_cast<int>(m_spacing * 100) << L"%";
    DrawSlider(hdc, 8, sliderY + 102, SliderWidth, std::min(m_spacing / 3.0f, 1.0f), L"间距", oss.str());

    // Preset buttons
    int presetY = sliderY + 140;
    DrawPresetButtons(hdc, 8, presetY, client.Width() - 16);
}

void BrushPanel::DrawSlider(HDC hdc, int x, int y, int width, float value, const String& label, const String& valueText) {
    SetTextColor(hdc, Theme::TextSecondary);
    HFONT labelFont = CreateFontW(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, labelFont));
    RECT labelRc = { x, y - 2, x + 60, y + 14 };
    DrawTextW(hdc, label.c_str(), -1, &labelRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

    // Value text on the right
    RECT valRc = { x + width - 50, y - 2, x + width, y + 14 };
    DrawTextW(hdc, valueText.c_str(), -1, &valRc, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
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

void BrushPanel::DrawTipButtons(HDC hdc, int x, int y) {
    const wchar_t* labels[] = { L"硬圆", L"软圆", L"平头", L"毛笔", L"纹理" };
    int btnW = 32;
    int btnH = 20;
    int gap = 4;

    for (int i = 0; i < 5; ++i) {
        int bx = x + i * (btnW + gap);
        bool selected = (static_cast<int>(m_tipType) == i);

        HBRUSH brush = Theme::SolidBrush(selected ? Theme::HighlightBlue : Theme::ButtonDefault);
        RECT rc = { bx, y, bx + btnW, y + btnH };
        FillRect(hdc, &rc, brush);
        DeleteObject(brush);

        HPEN pen = CreatePen(PS_SOLID, 1, selected ? RGB(0x00, 0xA0, 0xFF) : Theme::BorderLight);
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
        HBRUSH nullBr = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
        HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
        Rectangle(hdc, bx, y, bx + btnW, y + btnH);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBr);
        DeleteObject(pen);

        SetTextColor(hdc, selected ? RGB(0xFF, 0xFF, 0xFF) : Theme::TextPrimary);
        HFONT fnt = CreateFontW(10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
        HFONT oldF = static_cast<HFONT>(SelectObject(hdc, fnt));
        RECT trc = { bx, y, bx + btnW, y + btnH };
        DrawTextW(hdc, labels[i], -1, &trc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
        SelectObject(hdc, oldF);
        DeleteObject(fnt);
    }
}

void BrushPanel::DrawPresetButtons(HDC hdc, int x, int y, int width) {
    SetTextColor(hdc, Theme::TextSecondary);
    HFONT font = CreateFontW(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
    RECT labelRc = { x, y, x + 60, y + 14 };
    DrawTextW(hdc, L"预设", -1, &labelRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    SelectObject(hdc, oldFont);
    DeleteObject(font);

    const wchar_t* presets[] = { L"钢笔", L"G笔", L"G软", L"水墨", L"喷枪" };
    int btnW = (width - 16) / 5;
    int btnH = 20;
    int gap = 4;
    int startY = y + 16;

    for (int i = 0; i < 5; ++i) {
        int bx = x + i * (btnW + gap);
        HBRUSH brush = Theme::SolidBrush(Theme::ButtonDefault);
        RECT rc = { bx, startY, bx + btnW, startY + btnH };
        FillRect(hdc, &rc, brush);
        DeleteObject(brush);

        HPEN pen = CreatePen(PS_SOLID, 1, Theme::BorderLight);
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
        HBRUSH nullBr = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
        HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
        Rectangle(hdc, bx, startY, bx + btnW, startY + btnH);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBr);
        DeleteObject(pen);

        SetTextColor(hdc, Theme::TextPrimary);
        HFONT fnt = CreateFontW(10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
        HFONT oldF = static_cast<HFONT>(SelectObject(hdc, fnt));
        RECT trc = { bx, startY, bx + btnW, startY + btnH };
        DrawTextW(hdc, presets[i], -1, &trc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
        SelectObject(hdc, oldF);
        DeleteObject(fnt);
    }
}

void BrushPanel::OnMouseDown(const Point& pos, MouseButton button) {
    if (button != MouseButton::Left) return;

    // Check tip buttons
    int tipBtn = HitTestTipButton(pos);
    if (tipBtn >= 0) {
        m_tipType = static_cast<Render::BrushTipType>(tipBtn);
        if (m_onTipTypeChanged) m_onTipTypeChanged(m_tipType);
        Invalidate();
        return;
    }

    // Check preset buttons
    int presetBtn = HitTestPresetButton(pos);
    if (presetBtn >= 0) {
        if (m_onPresetSelected) m_onPresetSelected(presetBtn);
        Invalidate();
        return;
    }

    // Check sliders
    int sliderIdx = 0;
    int slider = HitTestSlider(pos, sliderIdx);
    if (slider >= 0) {
        float t = std::clamp(static_cast<float>(pos.x - 8) / SliderWidth, 0.0f, 1.0f);
        switch (slider) {
            case 0:
                m_draggingSize = true;
                m_brushSize = std::clamp(t * 500.0f, 1.0f, 500.0f);
                if (m_onSizeChanged) m_onSizeChanged(m_brushSize);
                break;
            case 1:
                m_draggingOpacity = true;
                m_opacity = t;
                if (m_onOpacityChanged) m_onOpacityChanged(m_opacity);
                break;
            case 2:
                m_draggingFlow = true;
                m_flow = t;
                if (m_onFlowChanged) m_onFlowChanged(m_flow);
                break;
            case 3:
                m_draggingSpacing = true;
                m_spacing = std::clamp(t * 3.0f, 0.01f, 3.0f);
                if (m_onSpacingChanged) m_onSpacingChanged(m_spacing);
                break;
        }
        Invalidate();
    }
}

void BrushPanel::OnMouseMove(const Point& pos) {
    if (!m_draggingSize && !m_draggingOpacity && !m_draggingFlow && !m_draggingSpacing) return;

    float t = std::clamp(static_cast<float>(pos.x - 8) / SliderWidth, 0.0f, 1.0f);
    if (m_draggingSize) {
        m_brushSize = std::clamp(t * 500.0f, 1.0f, 500.0f);
        if (m_onSizeChanged) m_onSizeChanged(m_brushSize);
    } else if (m_draggingOpacity) {
        m_opacity = t;
        if (m_onOpacityChanged) m_onOpacityChanged(m_opacity);
    } else if (m_draggingFlow) {
        m_flow = t;
        if (m_onFlowChanged) m_onFlowChanged(m_flow);
    } else if (m_draggingSpacing) {
        m_spacing = std::clamp(t * 3.0f, 0.01f, 3.0f);
        if (m_onSpacingChanged) m_onSpacingChanged(m_spacing);
    }
    Invalidate();
}

void BrushPanel::OnMouseUp(const Point& pos, MouseButton button) {
    m_draggingSize = false;
    m_draggingOpacity = false;
    m_draggingFlow = false;
    m_draggingSpacing = false;
}

int BrushPanel::HitTestSlider(const Point& pos, int& outSliderIndex) const {
    int sliders[] = { 54 + 16, 88 + 16, 122 + 16, 156 + 16 };
    for (int i = 0; i < 4; ++i) {
        if (pos.y >= sliders[i] && pos.y < sliders[i] + SliderHeight && pos.x >= 8 && pos.x < 8 + SliderWidth) {
            outSliderIndex = i;
            return i;
        }
    }
    return -1;
}

int BrushPanel::HitTestTipButton(const Point& pos) const {
    int btnW = 32;
    int btnH = 20;
    int gap = 4;
    int x = 8;
    int y = 28;
    for (int i = 0; i < 5; ++i) {
        int bx = x + i * (btnW + gap);
        if (pos.x >= bx && pos.x < bx + btnW && pos.y >= y && pos.y < y + btnH) {
            return i;
        }
    }
    return -1;
}

int BrushPanel::HitTestPresetButton(const Point& pos) const {
    Rect client = GetClientBounds();
    int width = client.Width() - 16;
    int btnW = (width - 16) / 5;
    int btnH = 20;
    int gap = 4;
    int x = 8;
    int y = 54 + 140 + 16; // sliderY + 140 + 16
    for (int i = 0; i < 5; ++i) {
        int bx = x + i * (btnW + gap);
        if (pos.x >= bx && pos.x < bx + btnW && pos.y >= y && pos.y < y + btnH) {
            return i;
        }
    }
    return -1;
}

} // namespace UI
} // namespace VividPic
