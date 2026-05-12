#include "UI/Panels/BrushPanel.h"
#include "UI/Core/Theme.h"
#include "Render/BrushEngine.h"
#include <algorithm>
#include <sstream>
#include <cmath>

namespace VividPic {
namespace UI {

BrushPanel::BrushPanel() = default;

std::wstring BrushPanel::SliderInfo::FormatValue() const {
    std::wostringstream oss;
    if (max >= 100.0f) {
        oss << static_cast<int>(*value) << L"px";
    } else {
        oss << static_cast<int>(*value * 100.0f + 0.5f) << L"%";
    }
    return oss.str();
}

void BrushPanel::OnPaint(HDC hdc, const Rect& clip) {
    Rect client = GetClientBounds();

    // Background
    HBRUSH bgBrush = Theme::CachedBrush(Theme::PanelBackground);
    RECT rc = client.ToWin32Rect();
    FillRect(hdc, &rc, bgBrush);

    int pad = Theme::GetSize(Pad);
    int titleH = Theme::GetSize(TitleH);

    // Header
    DrawPanelHeader(hdc, client);

    int y = titleH + Theme::GetSize(SepMargin);

    // Brush preview row
    int previewSize = Theme::GetSize(PreviewSize);
    int strokeW = Theme::GetSize(StrokePreviewW);
    int strokeH = Theme::GetSize(StrokePreviewH);
    DrawBrushPreview(hdc, pad, y);
    DrawStrokePreview(hdc, pad + previewSize + pad, y + (previewSize - strokeH) / 2);
    y += previewSize + pad;

    // Tip type buttons
    DrawTipButtons(hdc, pad, y, client.Width() - pad * 2);
    y += Theme::GetSize(TipBtnH) + pad;

    // Sliders
    DrawSliders(hdc, pad, y, client.Width() - pad * 2);
    y += Theme::GetSize(SliderH) * 5 + pad;

    // Preset buttons
    DrawPresetButtons(hdc, pad, y, client.Width() - pad * 2);
}

void BrushPanel::DrawPanelHeader(HDC hdc, const Rect& client) {
    int titleH = Theme::GetSize(TitleH);
    Rect headerRc(0, 0, client.Width(), titleH);
    Theme::DrawPanelHeaderModern(hdc, headerRc, L"笔刷控制", IsCollapsed());
}

void BrushPanel::DrawBrushPreview(HDC hdc, int x, int y) {
    int size = Theme::GetSize(PreviewSize);
    Rect rc(x, y, x + size, y + size);

    // Checkerboard background
    int check = size / 8;
    for (int cy = 0; cy < 8; ++cy) {
        for (int cx = 0; cx < 8; ++cx) {
            uint32_t c = ((cx + cy) & 1) ? 0x666666 : 0x999999;
            HBRUSH br = Theme::CachedBrush(c);
            RECT r = { x + cx * check, y + cy * check, x + (cx + 1) * check, y + (cy + 1) * check };
            FillRect(hdc, &r, br);
        }
    }

    // Border
    HPEN pen = Theme::Pen(Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    HBRUSH nullBr = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(pen);

    // Draw tip shape centered
    DrawBrushTipShape(hdc, rc, m_tipType, m_brushSize);
}

void BrushPanel::DrawStrokePreview(HDC hdc, int x, int y) {
    int w = Theme::GetSize(StrokePreviewW);
    int h = Theme::GetSize(StrokePreviewH);
    Rect rc(x, y, x + w, y + h);

    // Dark background
    HBRUSH bg = Theme::CachedBrush(Theme::BackgroundDark);
    RECT r = rc.ToWin32Rect();
    FillRect(hdc, &r, bg);

    // Border
    HPEN pen = Theme::Pen(Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    HBRUSH nullBr = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(pen);

    // Draw a stroke curve
    DrawBrushStroke(hdc, rc, m_tipType, m_brushSize, m_opacity);
}

void BrushPanel::DrawBrushTipShape(HDC hdc, const Rect& rc, Render::BrushTipType type, float size) {
    int cx = rc.left + rc.Width() / 2;
    int cy = rc.top + rc.Height() / 2;
    float displaySize = std::min(size * 0.8f, static_cast<float>(rc.Width()) * 0.7f);
    int r = static_cast<int>(displaySize / 2);
    if (r < 1) r = 1;

    switch (type) {
        case Render::BrushTipType::RoundHard: {
            HBRUSH br = Theme::SolidBrush(Theme::TextPrimary);
            HPEN pen = Theme::Pen(Theme::TextPrimary);
            HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, br));
            HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
            Ellipse(hdc, cx - r, cy - r, cx + r, cy + r);
            SelectObject(hdc, oldBr);
            SelectObject(hdc, oldPen);
            DeleteObject(br);
            DeleteObject(pen);
            break;
        }
        case Render::BrushTipType::RoundSoft: {
            // Simulated soft edge with concentric circles of decreasing alpha
            for (int i = r; i >= 0; i -= std::max(1, r / 8)) {
                uint8_t alpha = static_cast<uint8_t>(255 * (1.0f - static_cast<float>(i) / r));
                uint32_t color = (alpha << 16) | (alpha << 8) | alpha;
                HPEN pen = Theme::Pen(color);
                HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
                HBRUSH nullBr = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
                HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
                Ellipse(hdc, cx - i, cy - i, cx + i, cy + i);
                SelectObject(hdc, oldPen);
                SelectObject(hdc, oldBr);
                DeleteObject(pen);
            }
            break;
        }
        case Render::BrushTipType::Flat: {
            int hw = static_cast<int>(r * 1.4f);
            int hh = static_cast<int>(r * 0.5f);
            HBRUSH br = Theme::SolidBrush(Theme::TextPrimary);
            HPEN pen = Theme::Pen(Theme::TextPrimary);
            HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, br));
            HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
            Ellipse(hdc, cx - hw, cy - hh, cx + hw, cy + hh);
            SelectObject(hdc, oldBr);
            SelectObject(hdc, oldPen);
            DeleteObject(br);
            DeleteObject(pen);
            break;
        }
        case Render::BrushTipType::Bristle: {
            // Draw several lines radiating from center
            HPEN pen = Theme::Pen(Theme::TextPrimary, std::max(1, r / 8));
            HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
            for (int i = 0; i < 8; ++i) {
                float angle = i * 3.14159265f / 4.0f;
                int x2 = cx + static_cast<int>(r * std::cos(angle));
                int y2 = cy + static_cast<int>(r * std::sin(angle));
                MoveToEx(hdc, cx, cy, nullptr);
                LineTo(hdc, x2, y2);
            }
            SelectObject(hdc, oldPen);
            DeleteObject(pen);
            break;
        }
        case Render::BrushTipType::Texture: {
            // Rough circle with noise dots
            HPEN pen = Theme::Pen(Theme::TextPrimary);
            HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
            HBRUSH nullBr = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
            HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
            Ellipse(hdc, cx - r, cy - r, cx + r, cy + r);
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBr);
            DeleteObject(pen);
            // Random dots inside
            for (int i = 0; i < r * 2; ++i) {
                float dx = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * r * 1.5f;
                float dy = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * r * 1.5f;
                if (dx * dx + dy * dy <= r * r) {
                    SetPixel(hdc, cx + static_cast<int>(dx), cy + static_cast<int>(dy), RGB(0xE0, 0xE0, 0xE0));
                }
            }
            break;
        }
    }
}

void BrushPanel::DrawBrushStroke(HDC hdc, const Rect& rc, Render::BrushTipType type, float size, float opacity) {
    int margin = Theme::GetSize(4);
    int x1 = rc.left + margin;
    int y1 = rc.top + rc.Height() / 2;
    int x2 = rc.right - margin;
    int y2 = y1;

    int thickness = std::max(1, static_cast<int>(std::min(size * 0.15f, static_cast<float>(rc.Height()) * 0.6f)));
    uint8_t a = static_cast<uint8_t>(opacity * 255);
    uint32_t color = (a << 16) | (a << 8) | a;

    HPEN pen = Theme::Pen(color, thickness);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    MoveToEx(hdc, x1, y1, nullptr);
    LineTo(hdc, x2, y2);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void BrushPanel::DrawTipButtons(HDC hdc, int x, int y, int width) {
    const wchar_t* labels[] = { L"硬圆", L"软圆", L"平头", L"毛笔", L"纹理" };
    int btnW = (width - Theme::GetSize(4) * 4) / 5;
    int btnH = Theme::GetSize(TipBtnH);
    int gap = Theme::GetSize(4);
    int radius = Theme::GetSize(3);

    for (int i = 0; i < 5; ++i) {
        int bx = x + i * (btnW + gap);
        bool selected = (static_cast<int>(m_tipType) == i);
        bool hovered = (m_hoverTipIndex == i);

        uint32_t bgColor = selected ? Theme::HighlightBlue : (hovered ? Theme::ButtonHover : Theme::ButtonDefault);
        uint32_t borderColor = selected ? Theme::HighlightHover : (hovered ? Theme::BorderLight : Theme::BorderDark);
        Theme::DrawRoundRect(hdc, Rect(bx, y, bx + btnW, y + btnH), radius, bgColor, borderColor);

        SetTextColor(hdc, selected ? Theme::TextInverse : Theme::TextPrimary);
        HFONT fnt = Theme::GetCachedFont(Theme::FontID::Small);
        HFONT oldF = static_cast<HFONT>(SelectObject(hdc, fnt));
        RECT trc = { bx, y, bx + btnW, y + btnH };
        DrawTextW(hdc, labels[i], -1, &trc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
        SelectObject(hdc, oldF);
    }
}

void BrushPanel::DrawSliders(HDC hdc, int x, int y, int width) {
    SliderInfo sliders[] = {
        { L"大小", &m_brushSize, 5000.0f, &m_draggingSize, [this]() { if (m_onSizeChanged) m_onSizeChanged(m_brushSize); } },
        { L"不透明度", &m_opacity, 1.0f, &m_draggingOpacity, [this]() { if (m_onOpacityChanged) m_onOpacityChanged(m_opacity); } },
        { L"流量", &m_flow, 1.0f, &m_draggingFlow, [this]() { if (m_onFlowChanged) m_onFlowChanged(m_flow); } },
        { L"间距", &m_spacing, 3.0f, &m_draggingSpacing, [this]() { if (m_onSpacingChanged) m_onSpacingChanged(m_spacing); } },
        { L"湿混", &m_wetMix, 1.0f, &m_draggingWetMix, [this]() { if (m_onWetMixChanged) m_onWetMixChanged(m_wetMix); } },
    };

    int sliderH = Theme::GetSize(SliderH);
    HFONT labelFont = Theme::GetCachedFont(Theme::FontID::Label);
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, labelFont));

    for (int i = 0; i < 5; ++i) {
        int sy = y + i * sliderH;
        auto& s = sliders[i];

        // Label
        SetTextColor(hdc, Theme::TextSecondary);
        RECT labelRc = { x, sy, x + Theme::GetSize(48), sy + Theme::GetSize(14) };
        DrawTextW(hdc, s.label, -1, &labelRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

        // Value text
        std::wstring valStr = s.FormatValue();
        RECT valRc = { x + width - Theme::GetSize(40), sy, x + width, sy + Theme::GetSize(14) };
        DrawTextW(hdc, valStr.c_str(), -1, &valRc, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);

        // Slider using Theme helper
        int trackY = sy + Theme::GetSize(16);
        int trackH = Theme::GetSize(8);
        float value01 = (s.max >= 100.0f) ? (s.value[0] / s.max) : s.value[0];
        Rect sliderRc(x, trackY, x + width - Theme::GetSize(40), trackY + trackH);
        Theme::DrawSlider(hdc, sliderRc, std::clamp(value01, 0.0f, 1.0f), m_hoverSliderIndex == i);
    }

    SelectObject(hdc, oldFont);
}

void BrushPanel::DrawPresetButtons(HDC hdc, int x, int y, int width) {
    const wchar_t* presets[] = { L"钢笔", L"G笔", L"G软", L"水墨", L"喷枪" };
    int btnW = (width - Theme::GetSize(4) * 4) / 5;
    int btnH = Theme::GetSize(PresetBtnH);
    int gap = Theme::GetSize(4);
    int radius = Theme::GetSize(3);

    // Section label
    SetTextColor(hdc, Theme::TextSecondary);
    HFONT labelFont = Theme::GetCachedFont(Theme::FontID::Label);
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, labelFont));
    RECT labelRc = { x, y, x + Theme::GetSize(60), y + Theme::GetSize(14) };
    DrawTextW(hdc, L"预设", -1, &labelRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    SelectObject(hdc, oldFont);

    int startY = y + Theme::GetSize(16);
    for (int i = 0; i < 5; ++i) {
        int bx = x + i * (btnW + gap);
        bool selected = (m_activePreset == i);
        bool hovered = (m_hoverPresetIndex == i);

        uint32_t bgColor = selected ? Theme::HighlightBlue : (hovered ? Theme::ButtonHover : Theme::ButtonDefault);
        uint32_t borderColor = selected ? Theme::HighlightHover : (hovered ? Theme::BorderLight : Theme::BorderDark);
        Theme::DrawRoundRect(hdc, Rect(bx, startY, bx + btnW, startY + btnH), radius, bgColor, borderColor);

        SetTextColor(hdc, selected ? Theme::TextInverse : Theme::TextPrimary);
        HFONT fnt = Theme::GetCachedFont(Theme::FontID::Small);
        HFONT oldF = static_cast<HFONT>(SelectObject(hdc, fnt));
        RECT trc = { bx, startY, bx + btnW, startY + btnH };
        DrawTextW(hdc, presets[i], -1, &trc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
        SelectObject(hdc, oldF);
    }
}

void BrushPanel::OnMouseDown(const Point& pos, MouseButton button) {
    if (button != MouseButton::Left) return;

    int tipBtn = HitTestTipButton(pos);
    if (tipBtn >= 0) {
        m_tipType = static_cast<Render::BrushTipType>(tipBtn);
        if (m_onTipTypeChanged) m_onTipTypeChanged(m_tipType);
        Invalidate();
        return;
    }

    int presetBtn = HitTestPresetButton(pos);
    if (presetBtn >= 0) {
        m_activePreset = presetBtn;
        if (m_onPresetSelected) m_onPresetSelected(presetBtn);
        Invalidate();
        return;
    }

    int slider = HitTestSlider(pos);
    if (slider >= 0) {
        int pad = Theme::GetSize(Pad);
        int width = GetClientBounds().Width() - pad * 2 - Theme::GetSize(40);
        float t = std::clamp(static_cast<float>(pos.x - pad) / width, 0.0f, 1.0f);
        switch (slider) {
            case 0:
                m_draggingSize = true;
                m_brushSize = std::clamp(t * 5000.0f, 1.0f, 5000.0f);
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
            case 4:
                m_draggingWetMix = true;
                m_wetMix = t;
                if (m_onWetMixChanged) m_onWetMixChanged(m_wetMix);
                break;
        }
        Invalidate();
    }
}

void BrushPanel::OnMouseMove(const Point& pos) {
    int oldHoverTip = m_hoverTipIndex;
    int oldHoverPreset = m_hoverPresetIndex;
    int oldHoverSlider = m_hoverSliderIndex;

    m_hoverTipIndex = HitTestTipButton(pos);
    m_hoverPresetIndex = HitTestPresetButton(pos);
    m_hoverSliderIndex = HitTestSlider(pos);

    if (oldHoverTip != m_hoverTipIndex || oldHoverPreset != m_hoverPresetIndex || oldHoverSlider != m_hoverSliderIndex) {
        Invalidate();
    }

    if (!m_draggingSize && !m_draggingOpacity && !m_draggingFlow && !m_draggingSpacing && !m_draggingWetMix) return;

    int pad = Theme::GetSize(Pad);
    int width = GetClientBounds().Width() - pad * 2 - Theme::GetSize(40);
    float t = std::clamp(static_cast<float>(pos.x - pad) / width, 0.0f, 1.0f);

    if (m_draggingSize) {
        m_brushSize = std::clamp(t * 5000.0f, 1.0f, 5000.0f);
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
    } else if (m_draggingWetMix) {
        m_wetMix = t;
        if (m_onWetMixChanged) m_onWetMixChanged(m_wetMix);
    }
    Invalidate();
}

void BrushPanel::OnMouseUp(const Point& pos, MouseButton button) {
    m_draggingSize = false;
    m_draggingOpacity = false;
    m_draggingFlow = false;
    m_draggingSpacing = false;
    m_draggingWetMix = false;
}

void BrushPanel::OnMouseLeave() {
    if (m_hoverTipIndex >= 0 || m_hoverPresetIndex >= 0 || m_hoverSliderIndex >= 0) {
        m_hoverTipIndex = -1;
        m_hoverPresetIndex = -1;
        m_hoverSliderIndex = -1;
        Invalidate();
    }
}

int BrushPanel::HitTestTipButton(const Point& pos) const {
    int pad = Theme::GetSize(Pad);
    int width = GetClientBounds().Width() - pad * 2;
    int btnW = (width - Theme::GetSize(4) * 4) / 5;
    int btnH = Theme::GetSize(TipBtnH);
    int gap = Theme::GetSize(4);
    int previewSize = Theme::GetSize(PreviewSize);
    int y = pad + previewSize + pad;
    for (int i = 0; i < 5; ++i) {
        int bx = pad + i * (btnW + gap);
        if (pos.x >= bx && pos.x < bx + btnW && pos.y >= y && pos.y < y + btnH) {
            return i;
        }
    }
    return -1;
}

int BrushPanel::HitTestPresetButton(const Point& pos) const {
    Rect client = GetClientBounds();
    int pad = Theme::GetSize(Pad);
    int width = client.Width() - pad * 2;
    int btnW = (width - Theme::GetSize(4) * 4) / 5;
    int btnH = Theme::GetSize(PresetBtnH);
    int gap = Theme::GetSize(4);
    int previewSize = Theme::GetSize(PreviewSize);
    int sliderH = Theme::GetSize(SliderH);
    int y = pad + previewSize + pad + Theme::GetSize(TipBtnH) + pad + sliderH * 5 + pad + Theme::GetSize(16);
    for (int i = 0; i < 5; ++i) {
        int bx = pad + i * (btnW + gap);
        if (pos.x >= bx && pos.x < bx + btnW && pos.y >= y && pos.y < y + btnH) {
            return i;
        }
    }
    return -1;
}

int BrushPanel::HitTestSlider(const Point& pos) const {
    int pad = Theme::GetSize(Pad);
    int width = GetClientBounds().Width() - pad * 2 - Theme::GetSize(40);
    int previewSize = Theme::GetSize(PreviewSize);
    int tipBtnH = Theme::GetSize(TipBtnH);
    int sliderH = Theme::GetSize(SliderH);
    int trackH = Theme::GetSize(8);
    int y = pad + previewSize + pad + tipBtnH + pad + Theme::GetSize(16);
    for (int i = 0; i < 5; ++i) {
        int sy = y + i * sliderH;
        if (pos.y >= sy && pos.y < sy + trackH && pos.x >= pad && pos.x < pad + width) {
            return i;
        }
    }
    return -1;
}

} // namespace UI
} // namespace VividPic
