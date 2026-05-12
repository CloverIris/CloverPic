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
    RgbToHsv(m_foregroundColor, m_hue, m_saturation, m_value);
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

// ------------------------------------------------------------------
// Layout helpers
// ------------------------------------------------------------------

Rect ColorsPanel::GetSVRect() const {
    int pad = Theme::GetSize(8);
    int titleH = Theme::GetSize(22);
    int svSize = Theme::GetSize(SVSquareSize);
    return Rect(pad, titleH + Theme::GetSize(6), pad + svSize, titleH + Theme::GetSize(6) + svSize);
}

Rect ColorsPanel::GetHueRect() const {
    int pad = Theme::GetSize(8);
    int titleH = Theme::GetSize(22);
    int svSize = Theme::GetSize(SVSquareSize);
    int hueW = Theme::GetSize(HueBarWidth);
    int gap = Theme::GetSize(4);
    int hueX = pad + svSize + gap;
    return Rect(hueX, titleH + Theme::GetSize(6), hueX + hueW, titleH + Theme::GetSize(6) + svSize);
}

Rect ColorsPanel::GetFgSwatchRect() const {
    int pad = Theme::GetSize(8);
    int svSize = Theme::GetSize(SVSquareSize);
    int titleH = Theme::GetSize(22);
    int swatchSize = Theme::GetSize(SwatchSize);
    int y = titleH + Theme::GetSize(6) + svSize + Theme::GetSize(8);
    return Rect(pad, y, pad + swatchSize, y + swatchSize);
}

Rect ColorsPanel::GetBgSwatchRect() const {
    Rect fg = GetFgSwatchRect();
    int smallGap = Theme::GetSize(4);
    int swatchSize = Theme::GetSize(SwatchSize);
    return Rect(fg.right + smallGap, fg.top, fg.right + smallGap + swatchSize, fg.bottom);
}

Rect ColorsPanel::GetSwapButtonRect() const {
    Rect bg = GetBgSwatchRect();
    int gap = Theme::GetSize(4);
    int btnSize = Theme::GetSize(20);
    int y = bg.top + (bg.Height() - btnSize) / 2;
    return Rect(bg.right + gap, y, bg.right + gap + btnSize, y + btnSize);
}

Rect ColorsPanel::GetHexRect() const {
    Rect swap = GetSwapButtonRect();
    int gap = Theme::GetSize(8);
    int pad = Theme::GetSize(8);
    int clientW = GetClientBounds().Width();
    int y = swap.top;
    int h = swap.Height();
    return Rect(swap.right + gap, y, clientW - pad, y + h);
}

Rect ColorsPanel::GetHistoryRect(int index) const {
    int pad = Theme::GetSize(8);
    int histSize = Theme::GetSize(HistoryItemSize);
    int histGap = Theme::GetSize(3);
    int swatchSize = Theme::GetSize(SwatchSize);
    Rect fg = GetFgSwatchRect();
    int startY = fg.bottom + Theme::GetSize(8);
    int row = index / 8;
    int col = index % 8;
    int x = pad + col * (histSize + histGap);
    int y = startY + row * (histSize + histGap);
    return Rect(x, y, x + histSize, y + histSize);
}

// ------------------------------------------------------------------
// Color state
// ------------------------------------------------------------------

void ColorsPanel::SetCurrentColor(const Color& color) {
    SetActiveColor(color);
    if (m_foregroundActive) m_foregroundColor = color;
    else m_backgroundColor = color;
}

void ColorsPanel::SetForegroundColor(const Color& color) {
    m_foregroundColor = color;
    if (m_foregroundActive) SetActiveColor(color);
}

void ColorsPanel::SetBackgroundColor(const Color& color) {
    m_backgroundColor = color;
    if (!m_foregroundActive) SetActiveColor(color);
}

void ColorsPanel::SetActiveColor(const Color& color) {
    float prevHue = m_hue;
    RgbToHsv(color, m_hue, m_saturation, m_value);
    if (std::abs(m_hue - prevHue) > 0.5f) {
        RebuildSVBitmap();
    }
    Invalidate();
}

void ColorsPanel::SwapColors() {
    std::swap(m_foregroundColor, m_backgroundColor);
    SetActiveColor(m_foregroundActive ? m_foregroundColor : m_backgroundColor);
    if (m_onColorChanged) m_onColorChanged(GetCurrentColor());
    Invalidate();
}

void ColorsPanel::SetForegroundActive(bool active) {
    if (m_foregroundActive == active) return;
    // Save current HSV to old active
    Color oldColor = HsvToRgb(m_hue, m_saturation, m_value);
    if (m_foregroundActive) m_foregroundColor = oldColor;
    else m_backgroundColor = oldColor;
    m_foregroundActive = active;
    // Load HSV from new active
    Color newColor = active ? m_foregroundColor : m_backgroundColor;
    RgbToHsv(newColor, m_hue, m_saturation, m_value);
    RebuildSVBitmap();
    Invalidate();
}

// ------------------------------------------------------------------
// Bitmap rebuild
// ------------------------------------------------------------------

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
    bmi.bmiHeader.biHeight = -size;
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
            m_svPixels[py * size + px] = RGB(c.b, c.g, c.r);
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
    bmi.bmiHeader.biHeight = -height;
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

// ------------------------------------------------------------------
// HSV / RGB helpers
// ------------------------------------------------------------------

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

// ------------------------------------------------------------------
// HEX inline edit
// ------------------------------------------------------------------

void ColorsPanel::StartHexEdit() {
    Color c = GetCurrentColor();
    std::wostringstream oss;
    oss << std::uppercase << std::hex << std::setfill(L'0')
        << std::setw(2) << static_cast<int>(c.r)
        << std::setw(2) << static_cast<int>(c.g)
        << std::setw(2) << static_cast<int>(c.b);
    m_hexText = oss.str();
    m_hexCursorPos = static_cast<int>(m_hexText.length());
    m_hexEditing = true;
    SetFocus();
    Invalidate();
}

void ColorsPanel::EndHexEdit(bool apply) {
    if (!m_hexEditing) return;
    if (apply) {
        // Parse hex
        uint32_t hexVal = 0;
        for (wchar_t ch : m_hexText) {
            hexVal <<= 4;
            if (ch >= L'0' && ch <= L'9') hexVal |= (ch - L'0');
            else if (ch >= L'A' && ch <= L'F') hexVal |= (ch - L'A' + 10);
            else if (ch >= L'a' && ch <= L'f') hexVal |= (ch - L'a' + 10);
        }
        Color newColor(
            static_cast<uint8_t>((hexVal >> 16) & 0xFF),
            static_cast<uint8_t>((hexVal >> 8) & 0xFF),
            static_cast<uint8_t>(hexVal & 0xFF)
        );
        SetActiveColor(newColor);
        if (m_foregroundActive) m_foregroundColor = newColor;
        else m_backgroundColor = newColor;
        AddToHistory(newColor);
        if (m_onColorChanged) m_onColorChanged(newColor);
    }
    m_hexEditing = false;
    m_hexText.clear();
    m_hexCursorPos = 0;
    Invalidate();
}

void ColorsPanel::HexInsertChar(wchar_t ch) {
    if (m_hexText.length() >= 6) return;
    if (!((ch >= L'0' && ch <= L'9') || (ch >= L'A' && ch <= L'F') || (ch >= L'a' && ch <= L'f'))) return;
    m_hexText.insert(m_hexCursorPos, 1, ch);
    m_hexCursorPos++;
    Invalidate();
}

void ColorsPanel::HexDeleteChar() {
    if (m_hexCursorPos > 0) {
        m_hexText.erase(m_hexCursorPos - 1, 1);
        m_hexCursorPos--;
        Invalidate();
    }
}

void ColorsPanel::HexMoveCursor(int delta) {
    m_hexCursorPos += delta;
    if (m_hexCursorPos < 0) m_hexCursorPos = 0;
    if (m_hexCursorPos > static_cast<int>(m_hexText.length())) m_hexCursorPos = static_cast<int>(m_hexText.length());
    Invalidate();
}


// ------------------------------------------------------------------
// OnPaint
// ------------------------------------------------------------------

void ColorsPanel::OnPaint(HDC hdc, const Rect& clip) {
    (void)clip;
    Rect client = GetClientBounds();
    int pad = Theme::GetSize(8);
    int titleH = Theme::GetSize(22);

    // Background
    HBRUSH bgBrush = Theme::CachedBrush(Theme::PanelBackground);
    RECT rc = client.ToWin32Rect();
    FillRect(hdc, &rc, bgBrush);

    // Modern panel header
    Rect headerRc(0, 0, client.Width(), titleH);
    Theme::DrawPanelHeaderModern(hdc, headerRc, L"颜色", IsCollapsed());

    if (IsCollapsed()) return;

    RebuildSVBitmap();
    RebuildHueBitmap();

    // SV square
    Rect svRc = GetSVRect();
    BitBlt(hdc, svRc.left, svRc.top, svRc.Width(), svRc.Height(), m_svDC, 0, 0, SRCCOPY);
    DrawSVCursor(hdc);

    // Hue bar
    Rect hueRc = GetHueRect();
    BitBlt(hdc, hueRc.left, hueRc.top, hueRc.Width(), hueRc.Height(), m_hueDC, 0, 0, SRCCOPY);
    DrawHueCursor(hdc);

    // Foreground / background swatches
    Rect fgRc = GetFgSwatchRect();
    Rect bgRc = GetBgSwatchRect();
    Rect swapRc = GetSwapButtonRect();

    // Draw checkerboard pattern under swatches for transparency awareness
    int check = fgRc.Width() / 4;
    for (int cy = 0; cy < 4; ++cy) {
        for (int cx = 0; cx < 4; ++cx) {
            uint32_t c = ((cx + cy) & 1) ? 0x666666 : 0x999999;
            HBRUSH br = Theme::CachedBrush(c);
            RECT r = { fgRc.left + cx * check, fgRc.top + cy * check,
                       fgRc.left + (cx + 1) * check, fgRc.top + (cy + 1) * check };
            FillRect(hdc, &r, br);
        }
    }
    for (int cy = 0; cy < 4; ++cy) {
        for (int cx = 0; cx < 4; ++cx) {
            uint32_t c = ((cx + cy) & 1) ? 0x666666 : 0x999999;
            HBRUSH br = Theme::CachedBrush(c);
            RECT r = { bgRc.left + cx * check, bgRc.top + cy * check,
                       bgRc.left + (cx + 1) * check, bgRc.top + (cy + 1) * check };
            FillRect(hdc, &r, br);
        }
    }

    // Swatches with rounded rect
    uint32_t fgColor = RGB(m_foregroundColor.r, m_foregroundColor.g, m_foregroundColor.b);
    uint32_t bgColor = RGB(m_backgroundColor.r, m_backgroundColor.g, m_backgroundColor.b);
    Theme::DrawColorSwatch(hdc, fgRc, fgColor, m_foregroundActive, m_hoverFgSwatch);
    Theme::DrawColorSwatch(hdc, bgRc, bgColor, !m_foregroundActive, m_hoverBgSwatch);

    // Swap button (icon: \uE8AB = Swap)
    Theme::DrawIconButton(hdc, swapRc, L'\uE8AB', false, m_hoverSwapBtn);

    // HEX field
    DrawHexField(hdc);

    // Color info
    DrawColorInfo(hdc);

    // History
    DrawColorHistory(hdc);
}

void ColorsPanel::DrawSVCursor(HDC hdc) {
    Rect svRc = GetSVRect();
    int cx = svRc.left + static_cast<int>(m_saturation * svRc.Width());
    int cy = svRc.top + static_cast<int>((1.0f - m_value) * svRc.Height());

    // White outer ring
    HPEN whitePen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, whitePen));
    HBRUSH nullBr = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
    Ellipse(hdc, cx - 6, cy - 6, cx + 7, cy + 7);
    SelectObject(hdc, oldPen);
    DeleteObject(whitePen);

    // Black inner ring
    HPEN blackPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
    oldPen = static_cast<HPEN>(SelectObject(hdc, blackPen));
    Ellipse(hdc, cx - 4, cy - 4, cx + 5, cy + 5);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(blackPen);
}

void ColorsPanel::DrawHueCursor(HDC hdc) {
    Rect hueRc = GetHueRect();
    int hcy = hueRc.top + static_cast<int>(m_hue / 360.0f * hueRc.Height());

    // White horizontal line extending beyond bar
    HPEN whitePen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, whitePen));
    MoveToEx(hdc, hueRc.left - 3, hcy, nullptr);
    LineTo(hdc, hueRc.right + 3, hcy);
    SelectObject(hdc, oldPen);
    DeleteObject(whitePen);

    // Black shadow line
    HPEN blackPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
    oldPen = static_cast<HPEN>(SelectObject(hdc, blackPen));
    MoveToEx(hdc, hueRc.left - 2, hcy - 1, nullptr);
    LineTo(hdc, hueRc.right + 2, hcy - 1);
    MoveToEx(hdc, hueRc.left - 2, hcy + 1, nullptr);
    LineTo(hdc, hueRc.right + 2, hcy + 1);
    SelectObject(hdc, oldPen);
    DeleteObject(blackPen);
}

void ColorsPanel::DrawHexField(HDC hdc) {
    Rect hexRc = GetHexRect();
    SetBkMode(hdc, TRANSPARENT);

    if (m_hexEditing) {
        // Edit box background
        HBRUSH editBg = Theme::CachedBrush(Theme::InputBackground);
        RECT editRc = hexRc.ToWin32Rect();
        FillRect(hdc, &editRc, editBg);

        // Border
        HPEN borderPen = CreatePen(PS_SOLID, 1, Theme::HighlightBlue);
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
        HBRUSH nullBr = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
        HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
        Rectangle(hdc, hexRc.left, hexRc.top, hexRc.right, hexRc.bottom);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBr);
        DeleteObject(borderPen);

        // Text
        SetTextColor(hdc, Theme::TextPrimary);
        HFONT font = Theme::GetCachedFont(Theme::FontID::Value);
        HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
        std::wstring text = L"#" + m_hexText;
        RECT textRc = { hexRc.left + Theme::GetSize(4), hexRc.top, hexRc.right - Theme::GetSize(4), hexRc.bottom };
        DrawTextW(hdc, text.c_str(), -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

        // Caret
        if (HasFocus()) {
            HDC memDC = CreateCompatibleDC(hdc);
            HFONT oldF = static_cast<HFONT>(SelectObject(memDC, font));
            SIZE sz;
            GetTextExtentPoint32W(memDC, text.c_str(), m_hexCursorPos + 1, &sz);
            SelectObject(memDC, oldF);
            DeleteDC(memDC);
            int caretX = hexRc.left + Theme::GetSize(4) + sz.cx;
            int caretY1 = hexRc.top + Theme::GetSize(3);
            int caretY2 = hexRc.bottom - Theme::GetSize(3);
            HPEN caretPen = CreatePen(PS_SOLID, 1, Theme::HighlightBlue);
            oldPen = static_cast<HPEN>(SelectObject(hdc, caretPen));
            MoveToEx(hdc, caretX, caretY1, nullptr);
            LineTo(hdc, caretX, caretY2);
            SelectObject(hdc, oldPen);
            DeleteObject(caretPen);
        }
        SelectObject(hdc, oldFont);
    } else {
        // Static display
        Color c = GetCurrentColor();
        std::wostringstream oss;
        oss << L"#" << std::uppercase << std::hex << std::setfill(L'0')
            << std::setw(2) << static_cast<int>(c.r)
            << std::setw(2) << static_cast<int>(c.g)
            << std::setw(2) << static_cast<int>(c.b);
        SetTextColor(hdc, Theme::TextSecondary);
        HFONT font = Theme::GetCachedFont(Theme::FontID::Value);
        HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
        RECT textRc = hexRc.ToWin32Rect();
        DrawTextW(hdc, oss.str().c_str(), -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        SelectObject(hdc, oldFont);
    }
}

void ColorsPanel::DrawColorInfo(HDC hdc) {
    Rect fgRc = GetFgSwatchRect();
    int infoY = fgRc.bottom + Theme::GetSize(6);
    int pad = Theme::GetSize(8);
    int clientW = GetClientBounds().Width();

    SetBkMode(hdc, TRANSPARENT);
    HFONT valFont = Theme::GetCachedFont(Theme::FontID::Small);
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, valFont));

    // HSV line
    SetTextColor(hdc, Theme::TextSecondary);
    std::wostringstream hsvOss;
    hsvOss << L"H:" << static_cast<int>(m_hue) << L" S:" << static_cast<int>(m_saturation * 100)
           << L" V:" << static_cast<int>(m_value * 100);
    RECT hsvRc = { pad, infoY, clientW - pad, infoY + Theme::GetSize(16) };
    DrawTextW(hdc, hsvOss.str().c_str(), -1, &hsvRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

    // RGB line
    Color c = GetCurrentColor();
    infoY += Theme::GetSize(16);
    std::wostringstream rgbOss;
    rgbOss << L"R:" << static_cast<int>(c.r) << L" G:" << static_cast<int>(c.g)
           << L" B:" << static_cast<int>(c.b);
    RECT rgbRc = { pad, infoY, clientW - pad, infoY + Theme::GetSize(16) };
    DrawTextW(hdc, rgbOss.str().c_str(), -1, &rgbRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

    SelectObject(hdc, oldFont);
}

void ColorsPanel::DrawColorHistory(HDC hdc) {
    int histSize = Theme::GetSize(HistoryItemSize);
    int radius = Theme::GetSize(3);

    for (int i = 0; i < m_historyCount; ++i) {
        Rect rc = GetHistoryRect(i);
        bool hovered = (i == m_hoverHistoryIndex);

        // Checkerboard under transparent colors
        int check = histSize / 4;
        for (int cy = 0; cy < 4; ++cy) {
            for (int cx = 0; cx < 4; ++cx) {
                uint32_t cb = ((cx + cy) & 1) ? 0x666666 : 0x999999;
                HBRUSH br = Theme::CachedBrush(cb);
                RECT r = { rc.left + cx * check, rc.top + cy * check,
                           rc.left + (cx + 1) * check, rc.top + (cy + 1) * check };
                FillRect(hdc, &r, br);
            }
        }

        // Color fill
        HBRUSH fill = CreateSolidBrush(RGB(m_history[i].r, m_history[i].g, m_history[i].b));
        HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, fill));
        HPEN pen = CreatePen(PS_SOLID, hovered ? 2 : 1, hovered ? Theme::HighlightBlue : Theme::BorderLight);
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius * 2, radius * 2);
        SelectObject(hdc, oldBr);
        SelectObject(hdc, oldPen);
        DeleteObject(fill);
        DeleteObject(pen);

        // Hover: subtle white overlay
        if (hovered) {
            HBRUSH hoverBr = CreateSolidBrush(RGB(255, 255, 255));
            RECT hr = { rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1 };
            // Skip alpha blend for simplicity
            DeleteObject(hoverBr);
        }
    }
}

// ------------------------------------------------------------------
// Mouse events
// ------------------------------------------------------------------

void ColorsPanel::OnMouseDown(const Point& pos, MouseButton button) {
    if (button != MouseButton::Left) return;

    if (IsCollapsible() && pos.y < Theme::GetSize(26)) {
        ToggleCollapsed();
        return;
    }

    if (IsCollapsed()) return;

    // End hex edit if clicking outside
    if (m_hexEditing) {
        Rect hexRc = GetHexRect();
        if (!hexRc.Contains(pos)) {
            EndHexEdit(true);
            return;
        }
    }

    // SV square
    Rect svRc = GetSVRect();
    if (svRc.Contains(pos)) {
        m_draggingSV = true;
        UpdateColorFromPosition(pos);
        return;
    }

    // Hue bar
    Rect hueRc = GetHueRect();
    if (hueRc.Contains(pos)) {
        m_draggingHue = true;
        m_hue = static_cast<float>(pos.y - hueRc.top) / hueRc.Height() * 360.0f;
        m_hue = std::clamp(m_hue, 0.0f, 360.0f);
        SetActiveColor(HsvToRgb(m_hue, m_saturation, m_value));
        Color newColor = HsvToRgb(m_hue, m_saturation, m_value);
        if (m_foregroundActive) m_foregroundColor = newColor;
        else m_backgroundColor = newColor;
        AddToHistory(newColor);
        if (m_onColorChanged) m_onColorChanged(newColor);
        return;
    }

    // Foreground swatch
    Rect fgRc = GetFgSwatchRect();
    if (fgRc.Contains(pos)) {
        SetForegroundActive(true);
        return;
    }

    // Background swatch
    Rect bgRc = GetBgSwatchRect();
    if (bgRc.Contains(pos)) {
        SetForegroundActive(false);
        return;
    }

    // Swap button
    Rect swapRc = GetSwapButtonRect();
    if (swapRc.Contains(pos)) {
        SwapColors();
        return;
    }

    // HEX field (double click starts edit, but single click also starts for simplicity)
    Rect hexRc = GetHexRect();
    if (hexRc.Contains(pos)) {
        if (!m_hexEditing) {
            StartHexEdit();
        }
        return;
    }

    // History
    for (int i = 0; i < m_historyCount; ++i) {
        if (GetHistoryRect(i).Contains(pos)) {
            SetActiveColor(m_history[i]);
            if (m_foregroundActive) m_foregroundColor = m_history[i];
            else m_backgroundColor = m_history[i];
            if (m_onColorChanged) m_onColorChanged(m_history[i]);
            Invalidate();
            return;
        }
    }
}

void ColorsPanel::OnMouseMove(const Point& pos) {
    if (IsCollapsed()) return;

    // Update hover state
    int oldHoverHistory = m_hoverHistoryIndex;
    bool oldHoverFg = m_hoverFgSwatch;
    bool oldHoverBg = m_hoverBgSwatch;
    bool oldHoverSwap = m_hoverSwapBtn;

    m_hoverHistoryIndex = -1;
    m_hoverFgSwatch = false;
    m_hoverBgSwatch = false;
    m_hoverSwapBtn = false;

    if (GetFgSwatchRect().Contains(pos)) m_hoverFgSwatch = true;
    else if (GetBgSwatchRect().Contains(pos)) m_hoverBgSwatch = true;
    else if (GetSwapButtonRect().Contains(pos)) m_hoverSwapBtn = true;
    else {
        for (int i = 0; i < m_historyCount; ++i) {
            if (GetHistoryRect(i).Contains(pos)) {
                m_hoverHistoryIndex = i;
                break;
            }
        }
    }

    if (oldHoverHistory != m_hoverHistoryIndex || oldHoverFg != m_hoverFgSwatch ||
        oldHoverBg != m_hoverBgSwatch || oldHoverSwap != m_hoverSwapBtn) {
        Invalidate();
    }

    if (m_draggingSV) {
        UpdateColorFromPosition(pos);
    } else if (m_draggingHue) {
        Rect hueRc = GetHueRect();
        m_hue = static_cast<float>(pos.y - hueRc.top) / hueRc.Height() * 360.0f;
        m_hue = std::clamp(m_hue, 0.0f, 360.0f);
        Color newColor = HsvToRgb(m_hue, m_saturation, m_value);
        SetActiveColor(newColor);
        if (m_foregroundActive) m_foregroundColor = newColor;
        else m_backgroundColor = newColor;
        if (m_onColorChanged) m_onColorChanged(newColor);
    }
}

void ColorsPanel::OnMouseUp(const Point& pos, MouseButton button) {
    (void)pos;
    if (button == MouseButton::Left && (m_draggingSV || m_draggingHue)) {
        Color finalColor = HsvToRgb(m_hue, m_saturation, m_value);
        AddToHistory(finalColor);
    }
    m_draggingSV = false;
    m_draggingHue = false;
}

void ColorsPanel::OnMouseLeave() {
    if (m_hoverHistoryIndex >= 0 || m_hoverFgSwatch || m_hoverBgSwatch || m_hoverSwapBtn) {
        m_hoverHistoryIndex = -1;
        m_hoverFgSwatch = false;
        m_hoverBgSwatch = false;
        m_hoverSwapBtn = false;
        Invalidate();
    }
}

void ColorsPanel::UpdateColorFromPosition(const Point& pos) {
    Rect svRc = GetSVRect();
    m_saturation = static_cast<float>(pos.x - svRc.left) / svRc.Width();
    m_value = 1.0f - static_cast<float>(pos.y - svRc.top) / svRc.Height();
    m_saturation = std::clamp(m_saturation, 0.0f, 1.0f);
    m_value = std::clamp(m_value, 0.0f, 1.0f);
    Color newColor = HsvToRgb(m_hue, m_saturation, m_value);
    if (m_foregroundActive) m_foregroundColor = newColor;
    else m_backgroundColor = newColor;
    if (m_onColorChanged) m_onColorChanged(newColor);
    Invalidate();
}

// ------------------------------------------------------------------
// Keyboard (for HEX edit)
// ------------------------------------------------------------------

void ColorsPanel::OnKeyDown(uint32_t keyCode) {
    if (!m_hexEditing) return;
    switch (keyCode) {
        case VK_LEFT: HexMoveCursor(-1); break;
        case VK_RIGHT: HexMoveCursor(1); break;
        case VK_HOME: m_hexCursorPos = 0; Invalidate(); break;
        case VK_END: m_hexCursorPos = static_cast<int>(m_hexText.length()); Invalidate(); break;
        case VK_DELETE:
            if (m_hexCursorPos < static_cast<int>(m_hexText.length())) {
                m_hexText.erase(m_hexCursorPos, 1);
                Invalidate();
            }
            break;
        case VK_RETURN: EndHexEdit(true); break;
        case VK_ESCAPE: EndHexEdit(false); break;
    }
}

void ColorsPanel::OnChar(wchar_t ch) {
    if (!m_hexEditing) return;
    if (ch == L'\b') {
        HexDeleteChar();
    } else if (ch >= 32) {
        HexInsertChar(ch);
    }
}

} // namespace UI
} // namespace VividPic
