#pragma once

#include "Core/Platform/PlatformInterfaces.h"

namespace CloverPic::Presentation {

class SoftRenderer {
public:
    explicit SoftRenderer(Core::RgbaFrame& frame);

    void Clear(const Color& color);
    void FillRect(const Rect& rect, const Color& color);
    void StrokeRect(const Rect& rect, const Color& color, int thickness = 1);
    void FillCircle(int cx, int cy, int radius, const Color& color);
    void DrawLine(int x0, int y0, int x1, int y1, const Color& color, int thickness = 1);
    void DrawText(int x, int y, const String& text, const Color& color, int scale = 2);
    void BlitBgra(const Rect& dstRect, const uint8_t* srcPixels, uint32_t srcWidth, uint32_t srcHeight);
    void DrawCheckerboard(const Rect& rect, int cellSize, const Color& a, const Color& b);

private:
    void BlendPixel(int x, int y, const Color& color);
    void DrawGlyph(int x, int y, wchar_t ch, const Color& color, int scale);

    Core::RgbaFrame& m_frame;
};

} // namespace CloverPic::Presentation
