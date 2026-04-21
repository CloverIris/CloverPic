#pragma once

#include "Utils/Types.h"
#include "Render/RenderBackend.h"
#include <d2d1.h>

namespace VividPic {
namespace Render {

class D2DCanvas {
public:
    D2DCanvas();
    ~D2DCanvas();
    
    // Create canvas bitmap with given dimensions
    bool Create(uint32_t width, uint32_t height);
    void Destroy();
    bool IsValid() const { return m_bitmap != nullptr; }
    
    // Dimensions
    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    
    // Drawing operations (must be called between BeginDraw/EndDraw on a render target)
    void Clear(const Color& color);
    void DrawBrushStamp(ID2D1RenderTarget* rt, float x, float y, float pressure, 
                        float baseSize, const Color& color);
    
    // Render the canvas bitmap to a render target with transform
    void Render(ID2D1RenderTarget* rt, float offsetX, float offsetY, float scale);
    
    // Access underlying bitmap
    ID2D1Bitmap* GetBitmap() const { return m_bitmap; }
    
private:
    ID2D1Bitmap* m_bitmap = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    
    // Brush resources (cached)
    ID2D1SolidColorBrush* m_solidBrush = nullptr;
    ID2D1RadialGradientBrush* m_radialBrush = nullptr;
};

} // namespace Render
} // namespace VividPic
