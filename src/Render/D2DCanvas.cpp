#include "Render/D2DCanvas.h"
#include <cmath>
#include <algorithm>

namespace VividPic {
namespace Render {

D2DCanvas::D2DCanvas() = default;

D2DCanvas::~D2DCanvas() {
    Destroy();
}

bool D2DCanvas::Create(uint32_t width, uint32_t height) {
    Destroy();
    
    auto& backend = RenderBackend::GetInstance();
    if (!backend.IsInitialized()) {
        if (!backend.Initialize()) return false;
    }
    
    ID2D1Factory* factory = backend.GetFactory();
    if (!factory) return false;
    
    // Create a compatible render target to get a bitmap
    // We'll create a WIC bitmap first, then convert to D2D bitmap
    IWICImagingFactory* wicFactory = backend.GetWicFactory();
    if (!wicFactory) return false;
    
    IWICBitmap* wicBitmap = nullptr;
    HRESULT hr = wicFactory->CreateBitmap(
        width, height,
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapCacheOnLoad,
        &wicBitmap
    );
    if (FAILED(hr)) return false;
    
    // Create a temporary render target from WIC bitmap
    ID2D1RenderTarget* rt = nullptr;
    hr = factory->CreateWicBitmapRenderTarget(
        wicBitmap,
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
        ),
        &rt
    );
    if (FAILED(hr)) {
        wicBitmap->Release();
        return false;
    }
    
    // Get the bitmap from render target
    hr = rt->CreateSharedBitmap(
        __uuidof(IWICBitmap),
        wicBitmap,
        nullptr,
        &m_bitmap
    );
    
    rt->Release();
    wicBitmap->Release();
    
    if (FAILED(hr)) return false;
    
    m_width = width;
    m_height = height;
    
    // Create cached brushes
    rt->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, 1), &m_solidBrush);
    
    return true;
}

void D2DCanvas::Destroy() {
    if (m_bitmap) {
        m_bitmap->Release();
        m_bitmap = nullptr;
    }
    if (m_solidBrush) {
        m_solidBrush->Release();
        m_solidBrush = nullptr;
    }
    if (m_radialBrush) {
        m_radialBrush->Release();
        m_radialBrush = nullptr;
    }
    m_width = 0;
    m_height = 0;
}

void D2DCanvas::Clear(const Color& color) {
    if (!m_bitmap) return;
    
    // To clear, we need a render target. We'll create a compatible one.
    auto& backend = RenderBackend::GetInstance();
    ID2D1Factory* factory = backend.GetFactory();
    if (!factory) return;
    
    IWICImagingFactory* wicFactory = backend.GetWicFactory();
    if (!wicFactory) return;
    
    // Get the WIC bitmap from D2D bitmap
    IWICBitmap* wicBitmap = nullptr;
    if (!m_bitmap) return;
    
    // For simplicity in M2, recreate the bitmap with cleared color
    Destroy();
    
    HRESULT hr = wicFactory->CreateBitmap(
        m_width, m_height,
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapCacheOnLoad,
        &wicBitmap
    );
    if (FAILED(hr)) return;
    
    ID2D1RenderTarget* rt = nullptr;
    hr = factory->CreateWicBitmapRenderTarget(
        wicBitmap,
        D2D1::RenderTargetProperties(),
        &rt
    );
    if (SUCCEEDED(hr)) {
        rt->BeginDraw();
        rt->Clear(RenderBackend::ToD2DColor(color));
        rt->EndDraw();
        
        hr = rt->CreateSharedBitmap(
            __uuidof(IWICBitmap),
            wicBitmap,
            nullptr,
            &m_bitmap
        );
        rt->Release();
    }
    wicBitmap->Release();
}

void D2DCanvas::DrawBrushStamp(ID2D1RenderTarget* rt, float x, float y, float pressure,
                                float baseSize, const Color& color) {
    if (!rt) return;
    
    float size = baseSize * (0.2f + 0.8f * pressure);
    float alpha = (color.a / 255.0f) * (0.3f + 0.7f * pressure);
    
    ID2D1SolidColorBrush* brush = nullptr;
    HRESULT hr = rt->CreateSolidColorBrush(
        D2D1::ColorF(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, alpha),
        &brush
    );
    if (FAILED(hr)) return;
    
    D2D1_ELLIPSE ellipse = D2D1::Ellipse(D2D1::Point2F(x, y), size * 0.5f, size * 0.5f);
    rt->FillEllipse(ellipse, brush);
    
    brush->Release();
}

void D2DCanvas::Render(ID2D1RenderTarget* rt, float offsetX, float offsetY, float scale) {
    if (!rt || !m_bitmap) return;
    
    D2D1_MATRIX_3X2_F oldTransform;
    rt->GetTransform(&oldTransform);
    
    D2D1_MATRIX_3X2_F transform = D2D1::Matrix3x2F::Scale(scale, scale) *
                                   D2D1::Matrix3x2F::Translation(offsetX, offsetY);
    rt->SetTransform(transform);
    
    D2D1_RECT_F destRect = D2D1::RectF(0, 0, static_cast<float>(m_width), static_cast<float>(m_height));
    rt->DrawBitmap(m_bitmap, destRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    
    rt->SetTransform(oldTransform);
}

} // namespace Render
} // namespace VividPic
