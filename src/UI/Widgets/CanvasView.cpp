#include "UI/Widgets/CanvasView.h"
#include "Render/RenderBackend.h"
#include "App/Application.h"
#include <windowsx.h>
#include <algorithm>

namespace VividPic {
namespace UI {

CanvasView::CanvasView() = default;

CanvasView::~CanvasView() {
    ShutdownCanvas();
}

bool CanvasView::OnCreate() {
    auto& backend = Render::RenderBackend::GetInstance();
    if (!backend.IsInitialized()) {
        backend.Initialize();
    }
    
    m_tablet = &TabletInput::TabletManager::GetInstance();
    m_tablet->Initialize(m_hwnd);
    
    return true;
}

void CanvasView::OnDestroy() {
    ShutdownCanvas();
    if (m_tablet) {
        m_tablet->Shutdown();
    }
}

bool CanvasView::InitializeCanvas(uint32_t width, uint32_t height, const Color& bgColor) {
    ShutdownCanvas();
    
    m_canvasWidth = width;
    m_canvasHeight = height;
    m_bgColor = bgColor;
    m_transparentBg = (bgColor.a == 0);
    
    auto& backend = Render::RenderBackend::GetInstance();
    Rect client = GetClientBounds();
    if (!backend.CreateHwndRenderTarget(m_hwnd, client.Width(), client.Height(), &m_renderTarget)) {
        return false;
    }
    
    // Initialize layer manager
    m_layerManager = &LayerManager::GetInstance();
    m_layerManager->Initialize(width, height);
    
    // Create initial layer
    auto layer = m_layerManager->AddLayer(L"图层 1", LayerType::Color);
    if (layer && !m_transparentBg) {
        // Fill with background color
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                layer->SetPixel(x, y, m_bgColor);
            }
        }
    }
    
    // Allocate composite buffer
    m_compositeBuffer.resize(static_cast<size_t>(width) * height * 4, 0);
    
    // Create composite D2D bitmap
    auto& factory = Render::RenderBackend::GetInstance();
    IWICImagingFactory* wicFactory = factory.GetWicFactory();
    if (wicFactory) {
        IWICBitmap* wicBitmap = nullptr;
        HRESULT hr = wicFactory->CreateBitmap(
            width, height,
            GUID_WICPixelFormat32bppPBGRA,
            WICBitmapCacheOnLoad,
            &wicBitmap
        );
        if (SUCCEEDED(hr)) {
            m_renderTarget->CreateSharedBitmap(
                __uuidof(IWICBitmap),
                wicBitmap,
                nullptr,
                &m_compositeBitmap
            );
            wicBitmap->Release();
        }
    }
    
    // Center canvas
    m_panX = (client.Width() - static_cast<float>(width)) * 0.5f;
    m_panY = (client.Height() - static_cast<float>(height)) * 0.5f;
    
    Invalidate();
    return true;
}

void CanvasView::ShutdownCanvas() {
    if (m_compositeBitmap) {
        m_compositeBitmap->Release();
        m_compositeBitmap = nullptr;
    }
    if (m_renderTarget) {
        m_renderTarget->Release();
        m_renderTarget = nullptr;
    }
    m_compositeBuffer.clear();
    if (m_layerManager) {
        m_layerManager->Shutdown();
        m_layerManager = nullptr;
    }
}

void CanvasView::SetZoom(float zoom) {
    m_zoom = std::clamp(zoom, 0.01f, 16.0f);
    Invalidate();
}

void CanvasView::SetPan(float x, float y) {
    m_panX = x;
    m_panY = y;
    Invalidate();
}

void CanvasView::ScreenToCanvas(float screenX, float screenY, float& canvasX, float& canvasY) const {
    canvasX = (screenX - m_panX) / m_zoom;
    canvasY = (screenY - m_panY) / m_zoom;
}

void CanvasView::InvalidateCanvas() {
    Invalidate();
}

void CanvasView::SetBrushColor(const Color& color) {
    m_brushColor = color;
    Render::BrushEngine::GetInstance().SetColor(color);
}

void CanvasView::OnSize(const Size& newSize) {
    if (m_renderTarget) {
        m_renderTarget->Resize(D2D1::SizeU(newSize.width, newSize.height));
    }
    Invalidate();
}

void CanvasView::OnPaint(HDC hdc, const Rect& clip) {
    CompositeAndRender();
}

void CanvasView::CompositeAndRender() {
    if (!m_renderTarget) return;
    
    // Update composite bitmap if needed
    if (m_layerManager && m_layerManager->NeedsComposite()) {
        UpdateCompositeBitmap();
    }
    
    m_renderTarget->BeginDraw();
    m_renderTarget->Clear(Render::RenderBackend::ToD2DColor(Color::FromHex(Theme::CanvasOuter)));
    
    if (m_compositeBitmap) {
        if (m_transparentBg) {
            DrawCheckerboard(m_renderTarget);
        }
        
        D2D1_MATRIX_3X2_F oldTransform;
        m_renderTarget->GetTransform(&oldTransform);
        
        D2D1_MATRIX_3X2_F transform = D2D1::Matrix3x2F::Scale(m_zoom, m_zoom) *
                                       D2D1::Matrix3x2F::Translation(m_panX, m_panY);
        m_renderTarget->SetTransform(transform);
        
        D2D1_RECT_F destRect = D2D1::RectF(0, 0, static_cast<float>(m_canvasWidth), static_cast<float>(m_canvasHeight));
        m_renderTarget->DrawBitmap(m_compositeBitmap, destRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        
        DrawCanvasBorder(m_renderTarget);
        
        m_renderTarget->SetTransform(oldTransform);
    }
    
    HRESULT hr = m_renderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        ShutdownCanvas();
        InitializeCanvas(m_canvasWidth, m_canvasHeight, m_bgColor);
    }
}

void CanvasView::UpdateCompositeBitmap() {
    if (!m_layerManager || !m_renderTarget) return;
    
    // Composite to buffer
    m_layerManager->CompositeToBuffer(m_compositeBuffer.data(), m_canvasWidth, m_canvasHeight);
    
    // Update D2D bitmap from buffer
    if (m_compositeBitmap) {
        m_compositeBitmap->Release();
        m_compositeBitmap = nullptr;
    }
    
    D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f, 96.0f
    );
    
    HRESULT hr = m_renderTarget->CreateBitmap(
        D2D1::SizeU(m_canvasWidth, m_canvasHeight),
        m_compositeBuffer.data(),
        m_canvasWidth * 4,
        &props,
        &m_compositeBitmap
    );
}

void CanvasView::DrawCheckerboard(ID2D1RenderTarget* rt) {
    if (!rt) return;
    
    const float checkSize = 8.0f * m_zoom;
    const int checksX = static_cast<int>(m_canvasWidth / 8.0f) + 1;
    const int checksY = static_cast<int>(m_canvasHeight / 8.0f) + 1;
    
    ID2D1SolidColorBrush* lightBrush = nullptr;
    ID2D1SolidColorBrush* darkBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.5f, 0.5f, 0.5f, 1.0f), &lightBrush);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.4f, 0.4f, 0.4f, 1.0f), &darkBrush);
    
    if (lightBrush && darkBrush) {
        for (int y = 0; y < checksY; ++y) {
            for (int x = 0; x < checksX; ++x) {
                float px = m_panX + x * checkSize;
                float py = m_panY + y * checkSize;
                D2D1_RECT_F rect = D2D1::RectF(px, py, px + checkSize, py + checkSize);
                rt->FillRectangle(rect, ((x + y) % 2 == 0) ? lightBrush : darkBrush);
            }
        }
    }
    
    if (lightBrush) lightBrush->Release();
    if (darkBrush) darkBrush->Release();
}

void CanvasView::DrawCanvasBorder(ID2D1RenderTarget* rt) {
    if (!rt) return;
    
    ID2D1SolidColorBrush* borderBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.3f, 0.3f, 0.3f, 1.0f), &borderBrush);
    
    if (borderBrush) {
        D2D1_RECT_F rect = D2D1::RectF(0, 0, static_cast<float>(m_canvasWidth), static_cast<float>(m_canvasHeight));
        rt->DrawRectangle(rect, borderBrush, 1.0f);
        borderBrush->Release();
    }
}

void CanvasView::OnMouseMove(const Point& pos) {
    if (m_tablet && m_tablet->GetActiveDriver() != TabletInput::TabletManager::DriverType::Mouse) {
        TabletInput::TabletState state = m_tablet->GetState();
        if (state.isTouching && m_isDrawing) {
            float canvasX, canvasY;
            ScreenToCanvas(state.x, state.y, canvasX, canvasY);
            ApplyBrush(canvasX, canvasY, state.pressure);
            m_lastCanvasX = canvasX;
            m_lastCanvasY = canvasY;
            m_lastPressure = state.pressure;
            Invalidate();
            return;
        }
    }
    
    if (m_isDrawing) {
        float canvasX, canvasY;
        ScreenToCanvas(static_cast<float>(pos.x), static_cast<float>(pos.y), canvasX, canvasY);
        ApplyBrush(canvasX, canvasY, m_lastPressure);
        m_lastCanvasX = canvasX;
        m_lastCanvasY = canvasY;
        Invalidate();
    } else if (m_isPanning) {
        float dx = static_cast<float>(pos.x - m_panStartPos.x);
        float dy = static_cast<float>(pos.y - m_panStartPos.y);
        m_panX += dx;
        m_panY += dy;
        m_panStartPos = pos;
        Invalidate();
    }
}

void CanvasView::OnMouseDown(const Point& pos, MouseButton button) {
    if (!m_layerManager) return;
    
    if (button == MouseButton::Left) {
        m_isDrawing = true;
        float canvasX, canvasY;
        ScreenToCanvas(static_cast<float>(pos.x), static_cast<float>(pos.y), canvasX, canvasY);
        m_lastCanvasX = canvasX;
        m_lastCanvasY = canvasY;
        m_lastPressure = 1.0f;
        ApplyBrush(canvasX, canvasY, m_lastPressure);
        Invalidate();
    } else if (button == MouseButton::Middle) {
        m_isPanning = true;
        m_panStartPos = pos;
    }
}

void CanvasView::OnMouseUp(const Point& pos, MouseButton button) {
    if (button == MouseButton::Left) {
        m_isDrawing = false;
    } else if (button == MouseButton::Middle) {
        m_isPanning = false;
    }
}

void CanvasView::OnKeyDown(uint32_t keyCode) {
    // Shortcuts handled at Workspace level
}

void CanvasView::ApplyBrush(float x, float y, float pressure) {
    if (!m_layerManager) return;
    
    auto layer = m_layerManager->GetActiveLayer();
    if (!layer || layer->IsLocked()) return;
    
    auto& brush = Render::BrushEngine::GetInstance();
    brush.SetColor(m_brushColor);

    auto stamps = brush.GenerateStamps(m_lastCanvasX, m_lastCanvasY, m_lastPressure, x, y, pressure);

    for (const auto& stamp : stamps) {
        float size = brush.GetSize() * (0.2f + 0.8f * stamp.pressure);
        float opacity = brush.GetOpacity() * stamp.pressure;
        layer->DrawBrushStamp(stamp.x, stamp.y, size * 0.5f, m_brushColor, opacity,
                              brush.GetTipType(), brush.GetFlow(), brush.GetWetMix());
    }
    
    m_layerManager->MarkCompositeDirty();
}

LRESULT CanvasView::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_POINTERDOWN:
        case WM_POINTERUPDATE:
        case WM_POINTERUP:
            if (m_tablet) {
                if (m_tablet->ProcessMessage(msg, wParam, lParam)) {
                    if (msg == WM_POINTERDOWN) {
                        TabletInput::TabletState state = m_tablet->GetState();
                        m_isDrawing = true;
                        float canvasX, canvasY;
                        ScreenToCanvas(state.x, state.y, canvasX, canvasY);
                        m_lastCanvasX = canvasX;
                        m_lastCanvasY = canvasY;
                        m_lastPressure = state.pressure;
                        ApplyBrush(canvasX, canvasY, state.pressure);
                        Invalidate();
                        return 0;
                    } else if (msg == WM_POINTERUP) {
                        m_isDrawing = false;
                        return 0;
                    }
                }
            }
            break;
            
        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            float zoomFactor = (delta > 0) ? 1.1f : 0.9f;
            SetZoom(m_zoom * zoomFactor);
            return 0;
        }
    }
    
    return Window::HandleMessage(msg, wParam, lParam);
}

} // namespace UI
} // namespace VividPic
