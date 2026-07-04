#include "Core/Presentation/SoftRenderer.h"
#include "Core/Text/CoreTextEngine.h"
#include <algorithm>
#include <cmath>

namespace CloverPic::Presentation {

namespace {

Rect ClipRect(const Rect& rect, uint32_t width, uint32_t height) {
    return Rect(
        std::max<int32_t>(0, rect.left),
        std::max<int32_t>(0, rect.top),
        std::min<int32_t>(static_cast<int32_t>(width), rect.right),
        std::min<int32_t>(static_cast<int32_t>(height), rect.bottom));
}

uint8_t FontBits(wchar_t ch, int row) {
    // A compact 5x7 uppercase-ish debug font. Unknown glyphs draw as boxes.
    const wchar_t c = (ch >= L'a' && ch <= L'z') ? static_cast<wchar_t>(ch - 32) : ch;
    switch (c) {
        case L'0': { static const uint8_t r[] = {14,17,19,21,25,17,14}; return r[row]; }
        case L'1': { static const uint8_t r[] = {4,12,4,4,4,4,14}; return r[row]; }
        case L'2': { static const uint8_t r[] = {14,17,1,2,4,8,31}; return r[row]; }
        case L'3': { static const uint8_t r[] = {30,1,1,14,1,1,30}; return r[row]; }
        case L'4': { static const uint8_t r[] = {2,6,10,18,31,2,2}; return r[row]; }
        case L'5': { static const uint8_t r[] = {31,16,30,1,1,17,14}; return r[row]; }
        case L'6': { static const uint8_t r[] = {6,8,16,30,17,17,14}; return r[row]; }
        case L'7': { static const uint8_t r[] = {31,1,2,4,8,8,8}; return r[row]; }
        case L'8': { static const uint8_t r[] = {14,17,17,14,17,17,14}; return r[row]; }
        case L'9': { static const uint8_t r[] = {14,17,17,15,1,2,12}; return r[row]; }
        case L'A': { static const uint8_t r[] = {14,17,17,31,17,17,17}; return r[row]; }
        case L'B': { static const uint8_t r[] = {30,17,17,30,17,17,30}; return r[row]; }
        case L'C': { static const uint8_t r[] = {14,17,16,16,16,17,14}; return r[row]; }
        case L'D': { static const uint8_t r[] = {30,17,17,17,17,17,30}; return r[row]; }
        case L'E': { static const uint8_t r[] = {31,16,16,30,16,16,31}; return r[row]; }
        case L'F': { static const uint8_t r[] = {31,16,16,30,16,16,16}; return r[row]; }
        case L'G': { static const uint8_t r[] = {14,17,16,23,17,17,14}; return r[row]; }
        case L'H': { static const uint8_t r[] = {17,17,17,31,17,17,17}; return r[row]; }
        case L'I': { static const uint8_t r[] = {14,4,4,4,4,4,14}; return r[row]; }
        case L'J': { static const uint8_t r[] = {7,2,2,2,18,18,12}; return r[row]; }
        case L'K': { static const uint8_t r[] = {17,18,20,24,20,18,17}; return r[row]; }
        case L'L': { static const uint8_t r[] = {16,16,16,16,16,16,31}; return r[row]; }
        case L'M': { static const uint8_t r[] = {17,27,21,21,17,17,17}; return r[row]; }
        case L'N': { static const uint8_t r[] = {17,25,21,19,17,17,17}; return r[row]; }
        case L'O': { static const uint8_t r[] = {14,17,17,17,17,17,14}; return r[row]; }
        case L'P': { static const uint8_t r[] = {30,17,17,30,16,16,16}; return r[row]; }
        case L'Q': { static const uint8_t r[] = {14,17,17,17,21,18,13}; return r[row]; }
        case L'R': { static const uint8_t r[] = {30,17,17,30,20,18,17}; return r[row]; }
        case L'S': { static const uint8_t r[] = {15,16,16,14,1,1,30}; return r[row]; }
        case L'T': { static const uint8_t r[] = {31,4,4,4,4,4,4}; return r[row]; }
        case L'U': { static const uint8_t r[] = {17,17,17,17,17,17,14}; return r[row]; }
        case L'V': { static const uint8_t r[] = {17,17,17,17,17,10,4}; return r[row]; }
        case L'W': { static const uint8_t r[] = {17,17,17,21,21,21,10}; return r[row]; }
        case L'X': { static const uint8_t r[] = {17,17,10,4,10,17,17}; return r[row]; }
        case L'Y': { static const uint8_t r[] = {17,17,10,4,4,4,4}; return r[row]; }
        case L'Z': { static const uint8_t r[] = {31,1,2,4,8,16,31}; return r[row]; }
        case L'-': { static const uint8_t r[] = {0,0,0,31,0,0,0}; return r[row]; }
        case L'.': { static const uint8_t r[] = {0,0,0,0,0,12,12}; return r[row]; }
        case L'/': { static const uint8_t r[] = {1,1,2,4,8,16,16}; return r[row]; }
        case L'%': { static const uint8_t r[] = {24,25,2,4,8,19,3}; return r[row]; }
        case L':': { static const uint8_t r[] = {0,12,12,0,12,12,0}; return r[row]; }
        case L' ': return 0;
        default: { static const uint8_t r[] = {31,17,17,17,17,17,31}; return r[row]; }
    }
}

} // namespace

SoftRenderer::SoftRenderer(Core::RgbaFrame& frame) : m_frame(frame) {}

void SoftRenderer::Clear(const Color& color) {
    FillRect(Rect(0, 0, static_cast<int32_t>(m_frame.width), static_cast<int32_t>(m_frame.height)), color);
}

void SoftRenderer::FillRect(const Rect& rect, const Color& color) {
    const Rect clipped = ClipRect(rect, m_frame.width, m_frame.height);
    for (int y = clipped.top; y < clipped.bottom; ++y) {
        for (int x = clipped.left; x < clipped.right; ++x) {
            BlendPixel(x, y, color);
        }
    }
}

void SoftRenderer::StrokeRect(const Rect& rect, const Color& color, int thickness) {
    for (int i = 0; i < thickness; ++i) {
        FillRect(Rect(rect.left + i, rect.top + i, rect.right - i, rect.top + i + 1), color);
        FillRect(Rect(rect.left + i, rect.bottom - i - 1, rect.right - i, rect.bottom - i), color);
        FillRect(Rect(rect.left + i, rect.top + i, rect.left + i + 1, rect.bottom - i), color);
        FillRect(Rect(rect.right - i - 1, rect.top + i, rect.right - i, rect.bottom - i), color);
    }
}

void SoftRenderer::FillCircle(int cx, int cy, int radius, const Color& color) {
    const int r2 = radius * radius;
    for (int y = cy - radius; y <= cy + radius; ++y) {
        for (int x = cx - radius; x <= cx + radius; ++x) {
            const int dx = x - cx;
            const int dy = y - cy;
            if (dx * dx + dy * dy <= r2) {
                BlendPixel(x, y, color);
            }
        }
    }
}

void SoftRenderer::DrawLine(int x0, int y0, int x1, int y1, const Color& color, int thickness) {
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (true) {
        FillCircle(x0, y0, std::max(1, thickness), color);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void SoftRenderer::DrawText(int x, int y, const String& text, const Color& color, int scale) {
    TextRasterRequest request;
    request.text = text;
    request.fontFamily = CoreText::CoreTextEngine::Get().GetSystemUiFamily();
    request.fontSize = static_cast<float>(std::max(1, scale) * 7);
    request.color = color;
    request.maxWidth = m_frame.width > static_cast<uint32_t>(std::max(0, x)) ? m_frame.width - static_cast<uint32_t>(std::max(0, x)) : 0;
    request.maxHeight = m_frame.height > static_cast<uint32_t>(std::max(0, y)) ? m_frame.height - static_cast<uint32_t>(std::max(0, y)) : 0;

    RasterizedTextBitmap bitmap;
    if (CoreText::CoreTextEngine::Get().RasterizeText(request, bitmap) && !bitmap.IsEmpty()) {
        for (uint32_t py = 0; py < bitmap.height; ++py) {
            for (uint32_t px = 0; px < bitmap.width; ++px) {
                const size_t idx = (static_cast<size_t>(py) * bitmap.width + px) * 4;
                Color c(bitmap.rgbaPixels[idx], bitmap.rgbaPixels[idx + 1], bitmap.rgbaPixels[idx + 2], bitmap.rgbaPixels[idx + 3]);
                if (c.a > 0) {
                    BlendPixel(x + static_cast<int>(px), y + static_cast<int>(py), c);
                }
            }
        }
        return;
    }

    int cursor = x;
    for (wchar_t ch : text) {
        DrawGlyph(cursor, y, ch, color, scale);
        cursor += 6 * scale;
    }
}

void SoftRenderer::BlitBgra(const Rect& dstRect, const uint8_t* srcPixels, uint32_t srcWidth, uint32_t srcHeight) {
    if (!srcPixels || srcWidth == 0 || srcHeight == 0 || dstRect.Width() <= 0 || dstRect.Height() <= 0) {
        return;
    }

    const Rect clipped = ClipRect(dstRect, m_frame.width, m_frame.height);
    for (int y = clipped.top; y < clipped.bottom; ++y) {
        const uint32_t sy = static_cast<uint32_t>((static_cast<int64_t>(y - dstRect.top) * srcHeight) / dstRect.Height());
        for (int x = clipped.left; x < clipped.right; ++x) {
            const uint32_t sx = static_cast<uint32_t>((static_cast<int64_t>(x - dstRect.left) * srcWidth) / dstRect.Width());
            const size_t srcIdx = (static_cast<size_t>(sy) * srcWidth + sx) * 4;
            Color color(srcPixels[srcIdx + 2], srcPixels[srcIdx + 1], srcPixels[srcIdx], srcPixels[srcIdx + 3]);
            BlendPixel(x, y, color);
        }
    }
}

void SoftRenderer::DrawCheckerboard(const Rect& rect, int cellSize, const Color& a, const Color& b) {
    const Rect clipped = ClipRect(rect, m_frame.width, m_frame.height);
    for (int y = clipped.top; y < clipped.bottom; ++y) {
        for (int x = clipped.left; x < clipped.right; ++x) {
            const bool even = (((x - rect.left) / cellSize) + ((y - rect.top) / cellSize)) % 2 == 0;
            BlendPixel(x, y, even ? a : b);
        }
    }
}

void SoftRenderer::BlendPixel(int x, int y, const Color& color) {
    if (x < 0 || y < 0 || x >= static_cast<int>(m_frame.width) || y >= static_cast<int>(m_frame.height)) {
        return;
    }
    const size_t idx = (static_cast<size_t>(y) * m_frame.width + x) * 4;
    const float a = color.a / 255.0f;
    const float inv = 1.0f - a;
    m_frame.pixels[idx + 0] = static_cast<uint8_t>(color.b * a + m_frame.pixels[idx + 0] * inv);
    m_frame.pixels[idx + 1] = static_cast<uint8_t>(color.g * a + m_frame.pixels[idx + 1] * inv);
    m_frame.pixels[idx + 2] = static_cast<uint8_t>(color.r * a + m_frame.pixels[idx + 2] * inv);
    m_frame.pixels[idx + 3] = 255;
}

void SoftRenderer::DrawGlyph(int x, int y, wchar_t ch, const Color& color, int scale) {
    for (int row = 0; row < 7; ++row) {
        const uint8_t bits = FontBits(ch, row);
        for (int col = 0; col < 5; ++col) {
            if ((bits & (1 << (4 - col))) != 0) {
                FillRect(Rect(x + col * scale, y + row * scale, x + (col + 1) * scale, y + (row + 1) * scale), color);
            }
        }
    }
}

} // namespace CloverPic::Presentation
