#pragma once

#include "Utils/Types.h"
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>

namespace VividPic {
namespace Render {

class RenderBackend {
public:
    static RenderBackend& GetInstance();
    
    bool Initialize();
    void Shutdown();
    bool IsInitialized() const { return m_factory != nullptr; }
    
    // Factory access
    ID2D1Factory* GetFactory() const { return m_factory; }
    IDWriteFactory* GetWriteFactory() const { return m_writeFactory; }
    IWICImagingFactory* GetWicFactory() const { return m_wicFactory; }
    
    // Render target management
    bool CreateHwndRenderTarget(HWND hwnd, uint32_t width, uint32_t height, 
                                 ID2D1HwndRenderTarget** outTarget);
    
    // D2D1 Color helper
    static D2D1_COLOR_F ToD2DColor(const Color& color);
    
private:
    RenderBackend() = default;
    ~RenderBackend();
    
    ID2D1Factory* m_factory = nullptr;
    IDWriteFactory* m_writeFactory = nullptr;
    IWICImagingFactory* m_wicFactory = nullptr;
};

} // namespace Render
} // namespace VividPic
