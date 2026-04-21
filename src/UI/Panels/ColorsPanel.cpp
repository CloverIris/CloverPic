#include "UI/Panels/ColorsPanel.h"
#include "UI/Core/Theme.h"
#include <cmath>
#include <algorithm>
#include <sstream>

namespace VividPic {
namespace UI {

ColorsPanel::ColorsPanel() {
    for (int i = 0; i < HistoryCount; ++i) {
        m_history[i] = Color(0, 0, 0, 0);
    }
}

void ColorsPanel::SetCurrentColor(const Color& color) {
    m_currentColor = color;
    RgbToHsv(color, m_hue, m_saturation, m_value);
    Invalidate();
}

void ColorsPanel::OnPaint(HDC hdc, const Rect& clip) {
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
    DrawTextW(hdc, L"颜色", -1, &titleRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    SelectObject(hdc, oldFont);
    DeleteObject(font);
    
    // Separator
    HPEN sepPen = CreatePen(PS_SOLID, 1, Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, sepPen));
    MoveToEx(hdc, 8, 22, nullptr);
    LineTo(hdc, client.Width() - 8, 22);
    SelectObject(hdc, oldPen);
    DeleteObject(sepPen);
    
    DrawSaturationValueSquare(hdc);
    DrawHueBar(hdc);
    DrawCurrentColor(hdc);
    DrawColorHistory(hdc);
    
    // RGB values
    SetTextColor(hdc, Theme::TextPrimary);
    HFONT valFont = CreateFontW(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    oldFont = static_cast<HFONT>(SelectObject(hdc, valFont));
    
    std::wostringstream oss;
    oss << L"R: " << static_cast<int>(m_currentColor.r)
        << L"  G: " << static_cast<int>(m_currentColor.g)
        << L"  B: " << static_cast<int>(m_currentColor.b);
    RECT valRc = { 8, 24 + SVSquareSize + HueBarHeight + CurrentColorSize + HistoryItemSize + 16,
                   client.Width() - 8, client.Height() - 4 };
    DrawTextW(hdc, oss.str().c_str(), -1, &valRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    
    SelectObject(hdc, oldFont);
    DeleteObject(valFont);
}

void ColorsPanel::DrawSaturationValueSquare(HDC hdc) {
    int x = 8;
    int y = 28;
    
    Color baseColor = HsvToRgb(m_hue, 1.0f, 1.0f);
    
    for (int py = 0; py < SVSquareSize; ++py) {
        for (int px = 0; px < SVSquareSize; ++px) {
            float s = static_cast<float>(px) / SVSquareSize;
            float v = 1.0f - static_cast<float>(py) / SVSquareSize;
            
            Color c = HsvToRgb(m_hue, s, v);
            SetPixel(hdc, x + px, y + py, RGB(c.r, c.g, c.b));
        }
    }
    
    // Border
    HPEN borderPen = CreatePen(PS_SOLID, 1, Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
    HBRUSH nullBrush = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, nullBrush));
    Rectangle(hdc, x, y, x + SVSquareSize, y + SVSquareSize);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(borderPen);
    
    // Cursor
    int cx = x + static_cast<int>(m_saturation * SVSquareSize);
    int cy = y + static_cast<int>((1.0f - m_value) * SVSquareSize);
    HPEN cursorPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
    oldPen = static_cast<HPEN>(SelectObject(hdc, cursorPen));
    MoveToEx(hdc, cx - 4, cy, nullptr);
    LineTo(hdc, cx + 5, cy);
    MoveToEx(hdc, cx, cy - 4, nullptr);
    LineTo(hdc, cx, cy + 5);
    SelectObject(hdc, oldPen);
    DeleteObject(cursorPen);
}

void ColorsPanel::DrawHueBar(HDC hdc) {
    int x = 8;
    int y = 28 + SVSquareSize + 4;
    int width = SVSquareSize;
    
    for (int px = 0; px < width; ++px) {
        float h = static_cast<float>(px) / width * 360.0f;
        Color c = HsvToRgb(h, 1.0f, 1.0f);
        for (int py = 0; py < HueBarHeight; ++py) {
            SetPixel(hdc, x + px, y + py, RGB(c.r, c.g, c.b));
        }
    }
    
    // Border
    HPEN borderPen = CreatePen(PS_SOLID, 1, Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
    HBRUSH nullBrush = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, nullBrush));
    Rectangle(hdc, x, y, x + width, y + HueBarHeight);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(borderPen);
    
    // Cursor
    int cx = x + static_cast<int>(m_hue / 360.0f * width);
    HPEN cursorPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
    oldPen = static_cast<HPEN>(SelectObject(hdc, cursorPen));
    MoveToEx(hdc, cx, y - 2, nullptr);
    LineTo(hdc, cx, y + HueBarHeight + 2);
    SelectObject(hdc, oldPen);
    DeleteObject(cursorPen);
}

void ColorsPanel::DrawCurrentColor(HDC hdc) {
    int x = 8;
    int y = 28 + SVSquareSize + HueBarHeight + 8;
    
    HBRUSH colorBrush = CreateSolidBrush(RGB(m_currentColor.r, m_currentColor.g, m_currentColor.b));
    RECT rc = { x, y, x + CurrentColorSize, y + CurrentColorSize };
    FillRect(hdc, &rc, colorBrush);
    DeleteObject(colorBrush);
    
    HPEN borderPen = CreatePen(PS_SOLID, 1, Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
    HBRUSH nullBrush = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, nullBrush));
    Rectangle(hdc, x, y, x + CurrentColorSize, y + CurrentColorSize);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(borderPen);
}

void ColorsPanel::DrawColorHistory(HDC hdc) {
    int x = 8 + CurrentColorSize + 8;
    int y = 28 + SVSquareSize + HueBarHeight + 8;
    
    for (int i = 0; i < m_historyCount && i < HistoryCount; ++i) {
        int hx = x + (i % 8) * (HistoryItemSize + 2);
        int hy = y + (i / 8) * (HistoryItemSize + 2);
        
        HBRUSH histBrush = CreateSolidBrush(RGB(m_history[i].r, m_history[i].g, m_history[i].b));
        RECT rc = { hx, hy, hx + HistoryItemSize, hy + HistoryItemSize };
        FillRect(hdc, &rc, histBrush);
        DeleteObject(histBrush);
        
        HPEN borderPen = CreatePen(PS_SOLID, 1, Theme::BorderLight);
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
        HBRUSH nullBrush = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, nullBrush));
        Rectangle(hdc, hx, hy, hx + HistoryItemSize, hy + HistoryItemSize);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(borderPen);
    }
}

void ColorsPanel::OnMouseDown(const Point& pos, MouseButton button) {
    if (button != MouseButton::Left) return;
    
    int svX = 8;
    int svY = 28;
    if (pos.x >= svX && pos.x < svX + SVSquareSize && pos.y >= svY && pos.y < svY + SVSquareSize) {
        m_draggingSV = true;
        UpdateColorFromPosition(pos);
        return;
    }
    
    int hueX = 8;
    int hueY = 28 + SVSquareSize + 4;
    if (pos.x >= hueX && pos.x < hueX + SVSquareSize && pos.y >= hueY && pos.y < hueY + HueBarHeight) {
        m_draggingHue = true;
        m_hue = static_cast<float>(pos.x - hueX) / SVSquareSize * 360.0f;
        m_hue = std::clamp(m_hue, 0.0f, 360.0f);
        m_currentColor = HsvToRgb(m_hue, m_saturation, m_value);
        AddToHistory(m_currentColor);
        if (m_onColorChanged) m_onColorChanged(m_currentColor);
        Invalidate();
        return;
    }
    
    // History click
    int histX = 8 + CurrentColorSize + 8;
    int histY = 28 + SVSquareSize + HueBarHeight + 8;
    for (int i = 0; i < m_historyCount && i < HistoryCount; ++i) {
        int hx = histX + (i % 8) * (HistoryItemSize + 2);
        int hy = histY + (i / 8) * (HistoryItemSize + 2);
        if (pos.x >= hx && pos.x < hx + HistoryItemSize && pos.y >= hy && pos.y < hy + HistoryItemSize) {
            m_currentColor = m_history[i];
            RgbToHsv(m_currentColor, m_hue, m_saturation, m_value);
            if (m_onColorChanged) m_onColorChanged(m_currentColor);
            Invalidate();
            return;
        }
    }
}

void ColorsPanel::OnMouseMove(const Point& pos) {
    if (m_draggingSV) {
        UpdateColorFromPosition(pos);
    } else if (m_draggingHue) {
        int hueX = 8;
        m_hue = static_cast<float>(pos.x - hueX) / SVSquareSize * 360.0f;
        m_hue = std::clamp(m_hue, 0.0f, 360.0f);
        m_currentColor = HsvToRgb(m_hue, m_saturation, m_value);
        if (m_onColorChanged) m_onColorChanged(m_currentColor);
        Invalidate();
    }
}

void ColorsPanel::OnMouseUp(const Point& pos, MouseButton button) {
    if (m_draggingSV || m_draggingHue) {
        AddToHistory(m_currentColor);
    }
    m_draggingSV = false;
    m_draggingHue = false;
}

void ColorsPanel::UpdateColorFromPosition(const Point& pos) {
    int x = 8;
    int y = 28;
    
    m_saturation = static_cast<float>(pos.x - x) / SVSquareSize;
    m_value = 1.0f - static_cast<float>(pos.y - y) / SVSquareSize;
    m_saturation = std::clamp(m_saturation, 0.0f, 1.0f);
    m_value = std::clamp(m_value, 0.0f, 1.0f);
    
    m_currentColor = HsvToRgb(m_hue, m_saturation, m_value);
    if (m_onColorChanged) m_onColorChanged(m_currentColor);
    Invalidate();
}

Color ColorsPanel::HsvToRgb(float h, float s, float v) {
    h = std::fmod(h, 360.0f);
    if (h < 0) h += 360.0f;
    
    float c = v * s;
    float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    
    float r, g, b;
    if (h < 60.0f) { r = c; g = x; b = 0; }
    else if (h < 120.0f) { r = x; g = c; b = 0; }
    else if (h < 180.0f) { r = 0; g = c; b = x; }
    else if (h < 240.0f) { r = 0; g = x; b = c; }
    else if (h < 300.0f) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }
    
    return Color(
        static_cast<uint8_t>((r + m) * 255.0f),
        static_cast<uint8_t>((g + m) * 255.0f),
        static_cast<uint8_t>((b + m) * 255.0f)
    );
}

void ColorsPanel::RgbToHsv(const Color& rgb, float& h, float& s, float& v) {
    float r = rgb.r / 255.0f;
    float g = rgb.g / 255.0f;
    float b = rgb.b / 255.0f;
    
    float maxVal = std::max({r, g, b});
    float minVal = std::min({r, g, b});
    float diff = maxVal - minVal;
    
    v = maxVal;
    s = (maxVal == 0.0f) ? 0.0f : diff / maxVal;
    
    if (diff < 0.001f) {
        h = 0.0f;
    } else if (maxVal == r) {
        h = 60.0f * std::fmod((g - b) / diff, 6.0f);
    } else if (maxVal == g) {
        h = 60.0f * ((b - r) / diff + 2.0f);
    } else {
        h = 60.0f * ((r - g) / diff + 4.0f);
    }
    
    if (h < 0.0f) h += 360.0f;
}

void ColorsPanel::AddToHistory(const Color& color) {
    // Don't add duplicates at current position
    if (m_historyCount > 0 && m_history[m_historyIndex].r == color.r &&
        m_history[m_historyIndex].g == color.g && m_history[m_historyIndex].b == color.b) {
        return;
    }
    
    m_historyIndex = (m_historyIndex + 1) % HistoryCount;
    m_history[m_historyIndex] = color;
    if (m_historyCount < HistoryCount) m_historyCount++;
}

} // namespace UI
} // namespace VividPic
