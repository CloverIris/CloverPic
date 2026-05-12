#include "UI/Core/Theme.h"
#include <unordered_map>
#include <array>
#include <algorithm>

namespace VividPic {
namespace UI {

float Theme::Scale = 1.25f;

// -----------------------------------------------------------------
// Font cache
// -----------------------------------------------------------------
static std::array<HFONT, 8> s_fontCache{};
static float s_fontCacheScale = 0.0f;

HFONT Theme::GetCachedFont(FontID id) {
    int idx = static_cast<int>(id);
    if (idx < 0 || idx >= 8) return nullptr;
    
    if (s_fontCacheScale != Scale || s_fontCache[idx] == nullptr) {
        // Rebuild all cached fonts when scale changes or first use
        if (s_fontCacheScale != Scale) {
            for (auto& f : s_fontCache) {
                if (f) { DeleteObject(f); f = nullptr; }
            }
            s_fontCacheScale = Scale;
        }
        
        switch (id) {
            case FontID::Title:
                s_fontCache[idx] = CreateFontW(static_cast<int>(GetFontSize(28)), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
                break;
            case FontID::Menu:
                s_fontCache[idx] = CreateFontW(static_cast<int>(GetFontSize(13)), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
                break;
            case FontID::Toolbar:
                s_fontCache[idx] = CreateFontW(static_cast<int>(GetFontSize(16)), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe MDL2 Assets");
                break;
            case FontID::PanelTitle:
                s_fontCache[idx] = CreateFontW(static_cast<int>(GetFontSize(13)), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
                break;
            case FontID::Label:
                s_fontCache[idx] = CreateFontW(static_cast<int>(GetFontSize(12)), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
                break;
            case FontID::Value:
                s_fontCache[idx] = CreateFontW(static_cast<int>(GetFontSize(12)), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
                break;
            case FontID::Small:
                s_fontCache[idx] = CreateFontW(static_cast<int>(GetFontSize(11)), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
                break;
            case FontID::Button:
                s_fontCache[idx] = CreateFontW(static_cast<int>(GetFontSize(13)), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
                break;
        }
    }
    return s_fontCache[idx];
}

HFONT Theme::GetFont(FontID id) {
    // Legacy: create a new font each call (for callers that manage their own lifetime)
    switch (id) {
        case FontID::Title:
            return CreateFontW(static_cast<int>(GetFontSize(28)), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
        case FontID::Menu:
            return CreateFontW(static_cast<int>(GetFontSize(13)), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
        case FontID::Toolbar:
            return CreateFontW(static_cast<int>(GetFontSize(16)), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe MDL2 Assets");
        case FontID::PanelTitle:
            return CreateFontW(static_cast<int>(GetFontSize(13)), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
        case FontID::Label:
            return CreateFontW(static_cast<int>(GetFontSize(12)), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
        case FontID::Value:
            return CreateFontW(static_cast<int>(GetFontSize(12)), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        case FontID::Small:
            return CreateFontW(static_cast<int>(GetFontSize(11)), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        case FontID::Button:
            return CreateFontW(static_cast<int>(GetFontSize(13)), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    }
    return nullptr;
}

// -----------------------------------------------------------------
// Brush cache
// -----------------------------------------------------------------
static std::unordered_map<uint32_t, HBRUSH> s_brushCache;

HBRUSH Theme::CachedBrush(uint32_t hex) {
    auto it = s_brushCache.find(hex);
    if (it != s_brushCache.end()) {
        return it->second;
    }
    HBRUSH brush = CreateSolidBrush(RGB(
        (hex >> 16) & 0xFF,
        (hex >> 8) & 0xFF,
        hex & 0xFF
    ));
    s_brushCache[hex] = brush;
    return brush;
}

void Theme::ShutdownCache() {
    for (auto& f : s_fontCache) {
        if (f) { DeleteObject(f); f = nullptr; }
    }
    s_fontCacheScale = 0.0f;
    for (auto& pair : s_brushCache) {
        if (pair.second) DeleteObject(pair.second);
    }
    s_brushCache.clear();
}

// -----------------------------------------------------------------
// Drawing helpers
// -----------------------------------------------------------------
void Theme::DrawGradientRect(HDC hdc, const Rect& rc, uint32_t topColor, uint32_t bottomColor) {
    int width = rc.Width();
    int height = rc.Height();
    if (width <= 0 || height <= 0) return;
    
    int r1 = (topColor >> 16) & 0xFF, g1 = (topColor >> 8) & 0xFF, b1 = topColor & 0xFF;
    int r2 = (bottomColor >> 16) & 0xFF, g2 = (bottomColor >> 8) & 0xFF, b2 = bottomColor & 0xFF;
    
    // Use a memory DIB for efficient gradient fill
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    void* bits = nullptr;
    HBITMAP hbm = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hbm || !bits) return;
    
    uint32_t* pixels = static_cast<uint32_t*>(bits);
    for (int y = 0; y < height; ++y) {
        float t = (height == 1) ? 0.0f : static_cast<float>(y) / (height - 1);
        int r = static_cast<int>(r1 + (r2 - r1) * t);
        int g = static_cast<int>(g1 + (g2 - g1) * t);
        int b = static_cast<int>(b1 + (b2 - b1) * t);
        uint32_t color = (0xFF000000) | (r << 16) | (g << 8) | b;
        for (int x = 0; x < width; ++x) {
            pixels[y * width + x] = color;
        }
    }
    
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(memDC, hbm));
    BitBlt(hdc, rc.left, rc.top, width, height, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteDC(memDC);
    DeleteObject(hbm);
}

void Theme::DrawBevelRect(HDC hdc, const Rect& rc, uint32_t highlight, uint32_t shadow, bool raised) {
    uint32_t topLeft = raised ? highlight : shadow;
    uint32_t bottomRight = raised ? shadow : highlight;
    
    HPEN penTop = Pen(topLeft);
    HPEN penBot = Pen(bottomRight);
    HPEN old = static_cast<HPEN>(SelectObject(hdc, penTop));
    
    MoveToEx(hdc, rc.left, rc.bottom - 1, nullptr);
    LineTo(hdc, rc.left, rc.top);
    LineTo(hdc, rc.right - 1, rc.top);
    
    SelectObject(hdc, penBot);
    LineTo(hdc, rc.right - 1, rc.bottom - 1);
    LineTo(hdc, rc.left, rc.bottom - 1);
    
    SelectObject(hdc, old);
    DeleteObject(penTop);
    DeleteObject(penBot);
}

void Theme::DrawRoundRect(HDC hdc, const Rect& rc, int radius, uint32_t fillColor, uint32_t borderColor) {
    HBRUSH brush = SolidBrush(fillColor);
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, brush));
    HPEN pen = (borderColor != 0) ? Pen(borderColor) : static_cast<HPEN>(GetStockObject(NULL_PEN));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius * 2, radius * 2);
    
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    if (borderColor != 0) DeleteObject(pen);
}

void Theme::DrawCheckBox(HDC hdc, const Rect& rc, bool checked, bool hovered, const wchar_t* label) {
    int boxSize = rc.Height();
    int boxX = rc.left;
    int boxY = rc.top;
    
    // Box background
    HBRUSH bg = SolidBrush(CheckBoxBg);
    RECT boxRc = { boxX, boxY, boxX + boxSize, boxY + boxSize };
    FillRect(hdc, &boxRc, bg);
    DeleteObject(bg);
    
    // Box border
    uint32_t border = hovered ? HighlightBlue : CheckBoxBorder;
    HPEN pen = Pen(border);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    HBRUSH nullBr = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
    Rectangle(hdc, boxX, boxY, boxX + boxSize, boxY + boxSize);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(pen);
    
    // Check mark
    if (checked) {
        HPEN checkPen = Pen(CheckBoxCheck, 2);
        oldPen = static_cast<HPEN>(SelectObject(hdc, checkPen));
        int margin = boxSize / 4;
        MoveToEx(hdc, boxX + margin, boxY + boxSize / 2, nullptr);
        LineTo(hdc, boxX + boxSize / 2 - 2, boxY + boxSize - margin - 2);
        LineTo(hdc, boxX + boxSize - margin, boxY + margin);
        SelectObject(hdc, oldPen);
        DeleteObject(checkPen);
    }
    
    // Label
    if (label) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, TextLabel);
        HFONT font = GetCachedFont(FontID::Label);
        HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
        RECT textRc = { boxX + boxSize + 4, boxY, rc.right, boxY + boxSize };
        DrawTextW(hdc, label, -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        SelectObject(hdc, oldFont);
    }
}

void Theme::DrawSlider(HDC hdc, const Rect& rc, float value01, bool hovered) {
    int trackY = rc.top + rc.Height() / 2 - 2;
    int trackHeight = 4;
    
    // Track background
    HBRUSH trackBrush = SolidBrush(SliderTrack);
    RECT trackRc = { rc.left, trackY, rc.right, trackY + trackHeight };
    FillRect(hdc, &trackRc, trackBrush);
    DeleteObject(trackBrush);
    
    // Filled portion
    int fillX = rc.left + static_cast<int>((rc.right - rc.left) * value01);
    if (fillX > rc.left) {
        HBRUSH fillBrush = SolidBrush(SliderFill);
        RECT fillRc = { rc.left, trackY, fillX, trackY + trackHeight };
        FillRect(hdc, &fillRc, fillBrush);
        DeleteObject(fillBrush);
    }
    
    // Thumb
    int thumbRadius = hovered ? 6 : 5;
    uint32_t thumbColor = hovered ? SliderThumbHover : SliderThumb;
    int thumbX = fillX;
    int thumbY = rc.top + rc.Height() / 2;
    
    HBRUSH thumbBrush = SolidBrush(thumbColor);
    HPEN thumbPen = Pen(BorderLight);
    HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, thumbBrush));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, thumbPen));
    
    Ellipse(hdc, thumbX - thumbRadius, thumbY - thumbRadius, thumbX + thumbRadius, thumbY + thumbRadius);
    
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(thumbBrush);
    DeleteObject(thumbPen);
}

void Theme::DrawTooltip(HDC hdc, const Rect& rc) {
    HBRUSH bg = SolidBrush(TooltipBackground);
    RECT rcWin = rc.ToWin32Rect();
    FillRect(hdc, &rcWin, bg);
    DeleteObject(bg);
    
    HPEN pen = Pen(BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    HBRUSH nullBr = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(pen);
}

} // namespace UI
} // namespace VividPic
