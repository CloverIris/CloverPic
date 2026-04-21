#include "Render/RenderBackend.h"
#include <stdexcept>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace VividPic {
namespace Render {

RenderBackend& RenderBackend::GetInstance() {
    static RenderBackend instance;
    return instance;
}

RenderBackend::~RenderBackend() {
    Shutdown();
}

bool RenderBackend::Initialize() {
    if (m_factory) return true;
    
    HRESULT hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        &m_factory
    );
    if (FAILED(hr)) return false;
    
    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&m_writeFactory)
    );
    if (FAILED(hr)) {
        Shutdown();
        return false;
    }
    
    hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&m_wicFactory)
    );
    if (FAILED(hr)) {
        Shutdown();
        return false;
    }
    
    return true;
}

void RenderBackend::Shutdown() {
    if (m_wicFactory) {
        m_wicFactory->Release();
        m_wicFactory = nullptr;
    }
    if (m_writeFactory) {
        m_writeFactory->Release();
        m_writeFactory = nullptr;
    }
    if (m_factory) {
        m_factory->Release();
        m_factory = nullptr;
    }
}

bool RenderBackend::CreateHwndRenderTarget(HWND hwnd, uint32_t width, uint32_t height, 
                                            ID2D1HwndRenderTarget** outTarget) {
    if (!m_factory || !outTarget) return false;
    
    RECT rc;
    GetClientRect(hwnd, &rc);
    
    D2D1_SIZE_U size = D2D1::SizeU(
        width > 0 ? width : static_cast<uint32_t>(rc.right - rc.left),
        height > 0 ? height : static_cast<uint32_t>(rc.bottom - rc.top)
    );
    
    HRESULT hr = m_factory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
        ),
        D2D1::HwndRenderTargetProperties(hwnd, size),
        outTarget
    );
    
    return SUCCEEDED(hr);
}

D2D1_COLOR_F RenderBackend::ToD2DColor(const Color& color) {
    return D2D1::ColorF(
        color.r / 255.0f,
        color.g / 255.0f,
        color.b / 255.0f,
        color.a / 255.0f
    );
}

} // namespace Render
} // namespace VividPic
