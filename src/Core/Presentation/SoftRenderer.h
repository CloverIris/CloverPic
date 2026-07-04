#pragma once

#include "Core/Platform/PlatformInterfaces.h"

namespace CloverPic::Presentation {

class SoftRenderer {
public:
    explicit SoftRenderer(Core::RgbaFrame& frame);

    void Clear(const Color& color);
    void FillRect(const Rect& rect, const Color& color);
    void StrokeRect(const Rect& rect, const Color& color, int thickness = 1);
    void FillVerticalGradient(const Rect& rect, const Color& top, const Color& bottom);
    void FillHorizontalGradient(const Rect& rect, const Color& left, const Color& right);
    void DrawHsvSquare(const Rect& rect, float hueDegrees);
    void DrawHueStrip(const Rect& rect);
    void FillCircle(int cx, int cy, int radius, const Color& color);
    void StrokeCircle(int cx, int cy, int radius, const Color& color, int thickness = 1);
    void DrawLine(int x0, int y0, int x1, int y1, const Color& color, int thickness = 1);
    void DrawText(int x, int y, const String& text, const Color& color, int scale = 2);
    void BlitBgra(const Rect& dstRect, const uint8_t* srcPixels, uint32_t srcWidth, uint32_t srcHeight);
    void DrawCheckerboard(const Rect& rect, int cellSize, const Color& a, const Color& b);
    void PushClip(const Rect& rect);
    void PopClip();

private:
    void BlendPixel(int x, int y, const Color& color);
    void DrawGlyph(int x, int y, wchar_t ch, const Color& color, int scale);

    Core::RgbaFrame& m_frame;
    Rect m_clipRect;
    std::vector<Rect> m_clipStack;
};

} // namespace CloverPic::Presentation
