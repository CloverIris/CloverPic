#include "UI/Panels/ColorsPanel.h"
#include "UI/Core/Theme.h"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace VividPic {
namespace UI {

ColorsPanel::ColorsPanel() {
    for (int i = 0; i < HistoryCount; ++i) {
        m_history[i] = Color(0, 0, 0, 0);
    }
}

ColorsPanel::~ColorsPanel() {
    CleanupBitmaps();
}

void ColorsPanel::CleanupBitmaps() {
    if (m_svDC) {
        if (m_svOldBitmap) SelectObject(m_svDC, m_svOldBitmap);
        if (m_svBitmap) DeleteObject(m_svBitmap);
        DeleteDC(m_svDC);
        m_svDC = nullptr;
        m_svBitmap = nullptr;
        m_svOldBitmap = nullptr;
        m_svPixels = nullptr;
    }
    if (m_hueDC) {
        if (m_hueOldBitmap) SelectObject(m_hueDC, m_hueOldBitmap);
        if (m_hueBitmap) DeleteObject(m_hueBitmap);
        DeleteDC(m_hueDC);
        m_hueDC = nullptr;
        m_hueBitmap = nullptr;
        m_hueOldBitmap = nullptr;
        m_huePixels = nullptr;
    }
}

void ColorsPanel::RebuildSVBitmap() {
    int size = Theme::GetSize(SVSquareSize);
    if (m_svBitmapSize == size && m_svDC && m_lastHue == m_hue) return;
    m_svBitmapSize = size;
    m_lastHue = m_hue;

    if (m_svDC) {
        if (m_svOldBitmap) SelectObject(m_svDC, m_svOldBitmap);
        if (m_svBitmap) DeleteObject(m_svBitmap);
        DeleteDC(m_svDC);
    }

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = size;
    bmi.bmiHeader.biHeight = -size; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    m_svDC = CreateCompatibleDC(nullptr);
    m_svBitmap = CreateDIBSection(m_svDC, &bmi, DIB_RGB_COLORS, reinterpret_cast<void**>(&m_svPixels), nullptr, 0);
    m_svOldBitmap = static_cast<HBITMAP>(SelectObject(m_svDC, m_svBitmap));

    Color baseColor = HsvToRgb(m_hue, 1.0f, 1.0f);
    for (int py = 0; py < size; ++py) {
        for (int px = 0; px < size; ++px) {
            float s = static_cast<float>(px) / size;
            float v = 1.0f - static_cast<float>(py) / size;
            Color c = HsvToRgb(m_hue, s, v);
            m_svPixels[py * size + px] = RGB(c.b, c.g, c.r); // BGR for DIB
        }
    }
}

void ColorsPanel::RebuildHueBitmap() {
    int width = Theme::GetSize(HueBarWidth);
    int height = Theme::GetSize(SVSquareSize);
    if (m_hueBitmapWidth == width && m_hueBitmapHeight == height && m_hueDC) return;
    m_hueBitmapWidth = width;
    m_hueBitmapHeight = height;

    if (m_hueDC) {
        if (m_hueOldBitmap) SelectObject(m_hueDC, m_hueOldBitmap);
        if (m_hueBitmap) DeleteObject(m_hueBitmap);
        DeleteDC(m_hueDC);
        m_hueDC = nullptr;
        m_hueBitmap = nullptr;
        m_hueOldBitmap = nullptr;
        m_huePixels = nullptr;
    }

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    m_hueDC = CreateCompatibleDC(nullptr);
    m_hueBitmap = CreateDIBSection(m_hueDC, &bmi, DIB_RGB_COLORS, reinterpret_cast<void**>(&m_huePixels), nullptr, 0);
    m_hueOldBitmap = static_cast<HBITMAP>(SelectObject(m_hueDC, m_hueBitmap));

    for (int py = 0; py < height; ++py) {
        float h = static_cast<float>(py) / height * 360.0f;
        Color c = HsvToRgb(h, 1.0f, 1.0f);
        uint32_t color = RGB(c.b, c.g, c.r);
        for (int px = 0; px < width; ++px) {
            m_huePixels[py * width + px] = color;
        }
    }
}

void ColorsPanel::SetCurrentColor(const Color& color) {
    float prevHue = m_hue;
    m_currentColor = color;
    RgbToHsv(color, m_hue, m_saturation, m_value);
    if (std::abs(m_hue - prevHue) > 0.5f) {
        RebuildSVBitmap();
    }
    Invalidate();
}

void ColorsPanel::OnPaint(HDC hdc, const Rect& clip) {
    Rect client = GetClientBounds();
    int pad = Theme::GetSize(8);
    int titleH = Theme::GetSize(22);
    int svSize = Theme::GetSize(SVSquareSize);
    int hueW = Theme::GetSize(HueBarWidth);
    int gap = Theme::GetSize(4);
    int colorSize = Theme::GetSize(CurrentColorSize);
    int histSize = Theme::GetSize(HistoryItemSize);

    HBRUSH bgBrush = Theme::SolidBrush(Theme::PanelBackground);
    RECT rc = client.ToWin32Rect();
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::TextSecondary);
    HFONT font = CreateFontW(Theme::GetFontSize(12), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));

    RECT titleRc = { pad, 4, client.Width() - pad, titleH };
    DrawTextW(hdc, L"颜色", -1, &titleRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    SelectObject(hdc, oldFont);
    DeleteObject(font);

    // Separator
    HPEN sepPen = CreatePen(PS_SOLID, 1, Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, sepPen));
    MoveToEx(hdc, pad, titleH, nullptr);
    LineTo(hdc, client.Width() - pad, titleH);
    SelectObject(hdc, oldPen);
    DeleteObject(sepPen);

    // Rebuild bitmaps if needed
    RebuildSVBitmap();
    RebuildHueBitmap();

    // Layout: SV square left, vertical hue bar right
    int svX = pad;
    int svY = titleH + 6;
    int hueX = svX + svSize + gap;
    int hueY = svY;

    // Blit SV square
    BitBlt(hdc, svX, svY, svSize, svSize, m_svDC, 0, 0, SRCCOPY);

    // SV cursor
    int cx = svX + static_cast<int>(m_saturation * svSize);
    int cy = svY + static_cast<int>((1.0f - m_value) * svSize);
    HPEN cursorPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
    oldPen = static_cast<HPEN>(SelectObject(hdc, cursorPen));
    MoveToEx(hdc, cx - 4, cy, nullptr);
    LineTo(hdc, cx + 5, cy);
    MoveToEx(hdc, cx, cy - 4, nullptr);
    LineTo(hdc, cx, cy + 5);
    SelectObject(hdc, oldPen);
    DeleteObject(cursorPen);

    // Blit vertical hue bar
    BitBlt(hdc, hueX, hueY, hueW, svSize, m_hueDC, 0, 0, SRCCOPY);

    // Hue cursor (horizontal line on vertical bar)
    int hcy = hueY + static_cast<int>(m_hue / 360.0f * svSize);
    HPEN hcursorPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
    oldPen = static_cast<HPEN>(SelectObject(hdc, hcursorPen));
    MoveToEx(hdc, hueX - 2, hcy, nullptr);
    LineTo(hdc, hueX + hueW + 2, hcy);
    SelectObject(hdc, oldPen);
    DeleteObject(hcursorPen);

    // Current color + history
    DrawCurrentColor(hdc);
    DrawColorHistory(hdc);

    // HSV / Hex / RGB info
    DrawColorInfo(hdc);
}

void ColorsPanel::DrawCurrentColor(HDC hdc) {
    int pad = Theme::GetSize(8);
    int svSize = Theme::GetSize(SVSquareSize);
    int colorSize = Theme::GetSize(CurrentColorSize);
    int titleH = Theme::GetSize(22);
    int svY = titleH + 6;

    int x = pad;
    int y = svY + svSize + Theme::GetSize(8);

    HBRUSH colorBrush = CreateSolidBrush(RGB(m_currentColor.r, m_currentColor.g, m_currentColor.b));
    RECT rc = { x, y, x + colorSize, y + colorSize };
    FillRect(hdc, &rc, colorBrush);
    DeleteObject(colorBrush);

    HPEN borderPen = CreatePen(PS_SOLID, 1, Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
    HBRUSH nullBrush = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, nullBrush));
    Rectangle(hdc, x, y, x + colorSize, y + colorSize);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(borderPen);
}

void ColorsPanel::DrawColorHistory(HDC hdc) {
    int pad = Theme::GetSize(8);
    int svSize = Theme::GetSize(SVSquareSize);
    int colorSize = Theme::GetSize(CurrentColorSize);
    int histSize = Theme::GetSize(HistoryItemSize);
    int titleH = Theme::GetSize(22);
    int svY = titleH + 6;

    int x = pad + colorSize + pad;
    int y = svY + svSize + Theme::GetSize(8);

    for (int i = 0; i < m_historyCount && i < HistoryCount; ++i) {
        int hx = x + (i % 8) * (histSize + Theme::GetSize(2));
        int hy = y + (i / 8) * (histSize + Theme::GetSize(2));

        HBRUSH histBrush = CreateSolidBrush(RGB(m_history[i].r, m_history[i].g, m_history[i].b));
        RECT rc = { hx, hy, hx + histSize, hy + histSize };
        FillRect(hdc, &rc, histBrush);
        DeleteObject(histBrush);

        HPEN borderPen = CreatePen(PS_SOLID, 1, Theme::BorderLight);
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
        HBRUSH nullBrush = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, nullBrush));
        Rectangle(hdc, hx, hy, hx + histSize, hy + histSize);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(borderPen);
    }
}

void ColorsPanel::DrawColorInfo(HDC hdc) {
    int pad = Theme::GetSize(8);
    int svSize = Theme::GetSize(SVSquareSize);
    int colorSize = Theme::GetSize(CurrentColorSize);
    int histSize = Theme::GetSize(HistoryItemSize);
    int titleH = Theme::GetSize(22);
    int svY = titleH + 6;

    int y = svY + svSize + colorSize + Theme::GetSize(12);
    int x = pad;
    int w = GetClientBounds().Width() - pad * 2;

    SetBkMode(hdc, TRANSPARENT);
    HFONT valFont = CreateFontW(Theme::GetFontSize(11), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, valFont));

    // HSV line
    SetTextColor(hdc, Theme::TextPrimary);
    std::wostringstream oss;
    oss << L"H:" << static_cast<int>(m_hue) << L"° "
        << L"S:" << static_cast<int>(m_saturation * 100) << L"% "
        << L"V:" << static_cast<int>(m_value * 100) << L"%";
    RECT hsvRc = { x, y, x + w, y + Theme::GetSize(16) };
    DrawTextW(hdc, oss.str().c_str(), -1, &hsvRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

    // Hex line
    y += Theme::GetSize(16);
    std::wostringstream hexOss;
    hexOss << L"#" << std::uppercase << std::hex << std::setfill(L'0')
           << std::setw(2) << static_cast<int>(m_currentColor.r)
           << std::setw(2) << static_cast<int>(m_currentColor.g)
           << std::setw(2) << static_cast<int>(m_currentColor.b);
    RECT hexRc = { x, y, x + w, y + Theme::GetSize(16) };
    DrawTextW(hdc, hexOss.str().c_str(), -1, &hexRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

    // RGB line
    y += Theme::GetSize(16);
    std::wostringstream rgbOss;
    rgbOss << L"R:" << static_cast<int>(m_currentColor.r) << L" "
           << L"G:" << static_cast<int>(m_currentColor.g) << L" "
           << L"B:" << static_cast<int>(m_currentColor.b);
    RECT rgbRc = { x, y, x + w, y + Theme::GetSize(16) };
    DrawTextW(hdc, rgbOss.str().c_str(), -1, &rgbRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

    SelectObject(hdc, oldFont);
    DeleteObject(valFont);
}

void ColorsPanel::OnMouseDown(const Point& pos, MouseButton button) {
    if (button != MouseButton::Left) return;

    int pad = Theme::GetSize(8);
    int titleH = Theme::GetSize(22);
    int svSize = Theme::GetSize(SVSquareSize);
    int hueW = Theme::GetSize(HueBarWidth);
    int gap = Theme::GetSize(4);
    int colorSize = Theme::GetSize(CurrentColorSize);
    int histSize = Theme::GetSize(HistoryItemSize);
    int svY = titleH + 6;
    int svX = pad;

    if (pos.x >= svX && pos.x < svX + svSize && pos.y >= svY && pos.y < svY + svSize) {
        m_draggingSV = true;
        UpdateColorFromPosition(pos);
        return;
    }

    int hueX = svX + svSize + gap;
    int hueY = svY;
    if (pos.x >= hueX && pos.x < hueX + hueW && pos.y >= hueY && pos.y < hueY + svSize) {
        m_draggingHue = true;
        m_hue = static_cast<float>(pos.y - hueY) / svSize * 360.0f;
        m_hue = std::clamp(m_hue, 0.0f, 360.0f);
        m_currentColor = HsvToRgb(m_hue, m_saturation, m_value);
        RebuildSVBitmap();
        AddToHistory(m_currentColor);
        if (m_onColorChanged) m_onColorChanged(m_currentColor);
        Invalidate();
        return;
    }

    // History click
    int histX = pad + colorSize + pad;
    int histY = svY + svSize + Theme::GetSize(8);
    for (int i = 0; i < m_historyCount && i < HistoryCount; ++i) {
        int hx = histX + (i % 8) * (histSize + Theme::GetSize(2));
        int hy = histY + (i / 8) * (histSize + Theme::GetSize(2));
        if (pos.x >= hx && pos.x < hx + histSize && pos.y >= hy && pos.y < hy + histSize) {
            m_currentColor = m_history[i];
            RgbToHsv(m_currentColor, m_hue, m_saturation, m_value);
            RebuildSVBitmap();
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
        int svSize = Theme::GetSize(SVSquareSize);
        int hueY = Theme::GetSize(22) + 6;
        m_hue = static_cast<float>(pos.y - hueY) / svSize * 360.0f;
        m_hue = std::clamp(m_hue, 0.0f, 360.0f);
        m_currentColor = HsvToRgb(m_hue, m_saturation, m_value);
        RebuildSVBitmap();
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
    int pad = Theme::GetSize(8);
    int titleH = Theme::GetSize(22);
    int svSize = Theme::GetSize(SVSquareSize);
    int svX = pad;
    int svY = titleH + 6;

    m_saturation = static_cast<float>(pos.x - svX) / svSize;
    m_value = 1.0f - static_cast<float>(pos.y - svY) / svSize;
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
