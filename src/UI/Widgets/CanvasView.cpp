#include "UI/Widgets/CanvasView.h"
#include "Render/RenderBackend.h"
#include "App/Application.h"
#include <windowsx.h>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <cmath>

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

void CanvasView::SetCurrentTool(ToolType tool) {
    if (tool == m_currentTool) return;
    if (m_currentTool != ToolType::Eyedropper) {
        m_previousTool = m_currentTool;
    }
    m_currentTool = tool;
    Invalidate();
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
        DrawSelectionOutline(m_renderTarget);
        DrawBrushCursor(m_renderTarget);
        
        // Tool drag preview overlays (selection rect, lasso path, gradient line)
        if (m_isDragging) {
            ID2D1SolidColorBrush* previewBrush = nullptr;
            m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.7f), &previewBrush);
            if (previewBrush) {
                switch (m_currentTool) {
                    case ToolType::RectSelect: {
                        float x1 = std::min(m_dragStartX, m_dragCurrentX);
                        float y1 = std::min(m_dragStartY, m_dragCurrentY);
                        float x2 = std::max(m_dragStartX, m_dragCurrentX);
                        float y2 = std::max(m_dragStartY, m_dragCurrentY);
                        D2D1_RECT_F rc = D2D1::RectF(x1, y1, x2, y2);
                        m_renderTarget->DrawRectangle(rc, previewBrush, 1.0f);
                        break;
                    }
                    case ToolType::EllipseSelect: {
                        float cx = (m_dragStartX + m_dragCurrentX) * 0.5f;
                        float cy = (m_dragStartY + m_dragCurrentY) * 0.5f;
                        float rx = std::abs(m_dragCurrentX - m_dragStartX) * 0.5f;
                        float ry = std::abs(m_dragCurrentY - m_dragStartY) * 0.5f;
                        D2D1_ELLIPSE ellipse = D2D1::Ellipse(D2D1::Point2F(cx, cy), rx, ry);
                        m_renderTarget->DrawEllipse(ellipse, previewBrush, 1.0f);
                        break;
                    }
                    case ToolType::LassoSelect: {
                        if (m_lassoPoints.size() > 1) {
                            for (size_t i = 1; i < m_lassoPoints.size(); ++i) {
                                m_renderTarget->DrawLine(
                                    D2D1::Point2F(static_cast<float>(m_lassoPoints[i-1].x), static_cast<float>(m_lassoPoints[i-1].y)),
                                    D2D1::Point2F(static_cast<float>(m_lassoPoints[i].x), static_cast<float>(m_lassoPoints[i].y)),
                                    previewBrush, 1.0f);
                            }
                        }
                        break;
                    }
                    case ToolType::Gradient: {
                        m_renderTarget->DrawLine(
                            D2D1::Point2F(m_dragStartX, m_dragStartY),
                            D2D1::Point2F(m_dragCurrentX, m_dragCurrentY),
                            previewBrush, 1.5f);
                        // Draw start/end dots
                        D2D1_ELLIPSE e1 = D2D1::Ellipse(D2D1::Point2F(m_dragStartX, m_dragStartY), 3.0f, 3.0f);
                        D2D1_ELLIPSE e2 = D2D1::Ellipse(D2D1::Point2F(m_dragCurrentX, m_dragCurrentY), 3.0f, 3.0f);
                        m_renderTarget->FillEllipse(e1, previewBrush);
                        m_renderTarget->FillEllipse(e2, previewBrush);
                        break;
                    }
                    case ToolType::Move: {
                        // Show ghost offset line
                        m_renderTarget->DrawLine(
                            D2D1::Point2F(m_dragStartX, m_dragStartY),
                            D2D1::Point2F(m_dragCurrentX, m_dragCurrentY),
                            previewBrush, 1.0f);
                        break;
                    }
                    default:
                        break;
                }
                previewBrush->Release();
            }
        }
        
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

void CanvasView::DrawBrushCursor(ID2D1RenderTarget* rt) {
    if (!rt || m_isDrawing || !m_cursorInside) return;
    
    float canvasX, canvasY;
    ScreenToCanvas(m_cursorScreenX, m_cursorScreenY, canvasX, canvasY);
    
    // Only draw if inside canvas bounds
    if (canvasX < 0 || canvasX >= static_cast<float>(m_canvasWidth) ||
        canvasY < 0 || canvasY >= static_cast<float>(m_canvasHeight)) {
        return;
    }
    
    ID2D1SolidColorBrush* cursorBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.7f, 0.7f, 0.7f, 0.8f), &cursorBrush);
    if (!cursorBrush) return;
    
    auto& brush = Render::BrushEngine::GetInstance();
    float radius = (brush.GetSize() * 0.5f) * m_zoom;
    if (radius < 1.0f) radius = 1.0f;
    
    switch (m_currentTool) {
        case ToolType::Brush:
        case ToolType::Eraser: {
            D2D1_ELLIPSE ellipse = D2D1::Ellipse(
                D2D1::Point2F(canvasX, canvasY),
                radius, radius
            );
            rt->DrawEllipse(ellipse, cursorBrush, 1.0f);
            break;
        }
        case ToolType::Eyedropper: {
            float len = 8.0f;
            float cx = canvasX;
            float cy = canvasY;
            rt->DrawLine(D2D1::Point2F(cx - len, cy), D2D1::Point2F(cx + len, cy), cursorBrush, 1.0f);
            rt->DrawLine(D2D1::Point2F(cx, cy - len), D2D1::Point2F(cx, cy + len), cursorBrush, 1.0f);
            D2D1_RECT_F rect = D2D1::RectF(cx - 4, cy - 4, cx + 4, cy + 4);
            rt->DrawRectangle(rect, cursorBrush, 1.0f);
            break;
        }
        case ToolType::Fill: {
            float s = 10.0f;
            float cx = canvasX;
            float cy = canvasY;
            rt->DrawLine(D2D1::Point2F(cx - s*0.5f, cy - s*0.3f), D2D1::Point2F(cx + s*0.5f, cy - s*0.3f), cursorBrush, 1.5f);
            rt->DrawLine(D2D1::Point2F(cx - s*0.5f, cy - s*0.3f), D2D1::Point2F(cx - s*0.3f, cy + s*0.5f), cursorBrush, 1.5f);
            rt->DrawLine(D2D1::Point2F(cx + s*0.5f, cy - s*0.3f), D2D1::Point2F(cx + s*0.3f, cy + s*0.5f), cursorBrush, 1.5f);
            rt->DrawLine(D2D1::Point2F(cx - s*0.3f, cy + s*0.5f), D2D1::Point2F(cx + s*0.3f, cy + s*0.5f), cursorBrush, 1.5f);
            break;
        }
        case ToolType::RectSelect: {
            float s = 6.0f;
            rt->DrawLine(D2D1::Point2F(canvasX - s, canvasY), D2D1::Point2F(canvasX + s, canvasY), cursorBrush, 1.0f);
            rt->DrawLine(D2D1::Point2F(canvasX, canvasY - s), D2D1::Point2F(canvasX, canvasY + s), cursorBrush, 1.0f);
            D2D1_RECT_F rc = D2D1::RectF(canvasX - s, canvasY - s, canvasX + s, canvasY + s);
            rt->DrawRectangle(rc, cursorBrush, 1.0f);
            break;
        }
        case ToolType::EllipseSelect: {
            D2D1_ELLIPSE ellipse = D2D1::Ellipse(D2D1::Point2F(canvasX, canvasY), 6.0f, 6.0f);
            rt->DrawEllipse(ellipse, cursorBrush, 1.0f);
            break;
        }
        case ToolType::LassoSelect: {
            float s = 6.0f;
            rt->DrawLine(D2D1::Point2F(canvasX, canvasY - s), D2D1::Point2F(canvasX + s*0.5f, canvasY + s*0.5f), cursorBrush, 1.0f);
            rt->DrawLine(D2D1::Point2F(canvasX + s*0.5f, canvasY + s*0.5f), D2D1::Point2F(canvasX - s*0.5f, canvasY + s*0.5f), cursorBrush, 1.0f);
            rt->DrawLine(D2D1::Point2F(canvasX - s*0.5f, canvasY + s*0.5f), D2D1::Point2F(canvasX, canvasY - s), cursorBrush, 1.0f);
            break;
        }
        case ToolType::MagicWand: {
            float s = 6.0f;
            rt->DrawLine(D2D1::Point2F(canvasX - s, canvasY), D2D1::Point2F(canvasX + s, canvasY), cursorBrush, 1.0f);
            rt->DrawLine(D2D1::Point2F(canvasX, canvasY - s), D2D1::Point2F(canvasX, canvasY + s), cursorBrush, 1.0f);
            D2D1_ELLIPSE e = D2D1::Ellipse(D2D1::Point2F(canvasX, canvasY), 3.0f, 3.0f);
            rt->FillEllipse(e, cursorBrush);
            break;
        }
        case ToolType::Move: {
            float s = 8.0f;
            rt->DrawLine(D2D1::Point2F(canvasX - s, canvasY), D2D1::Point2F(canvasX + s, canvasY), cursorBrush, 1.0f);
            rt->DrawLine(D2D1::Point2F(canvasX, canvasY - s), D2D1::Point2F(canvasX, canvasY + s), cursorBrush, 1.0f);
            // Arrow heads
            rt->DrawLine(D2D1::Point2F(canvasX + s - 3, canvasY - 3), D2D1::Point2F(canvasX + s, canvasY), cursorBrush, 1.0f);
            rt->DrawLine(D2D1::Point2F(canvasX + s - 3, canvasY + 3), D2D1::Point2F(canvasX + s, canvasY), cursorBrush, 1.0f);
            rt->DrawLine(D2D1::Point2F(canvasX - 3, canvasY + s - 3), D2D1::Point2F(canvasX, canvasY + s), cursorBrush, 1.0f);
            rt->DrawLine(D2D1::Point2F(canvasX + 3, canvasY + s - 3), D2D1::Point2F(canvasX, canvasY + s), cursorBrush, 1.0f);
            break;
        }
        case ToolType::Gradient: {
            float s = 8.0f;
            rt->DrawLine(D2D1::Point2F(canvasX - s, canvasY), D2D1::Point2F(canvasX + s, canvasY), cursorBrush, 1.0f);
            D2D1_ELLIPSE e1 = D2D1::Ellipse(D2D1::Point2F(canvasX - s, canvasY), 2.0f, 2.0f);
            D2D1_ELLIPSE e2 = D2D1::Ellipse(D2D1::Point2F(canvasX + s, canvasY), 2.0f, 2.0f);
            rt->FillEllipse(e1, cursorBrush);
            rt->FillEllipse(e2, cursorBrush);
            break;
        }
        case ToolType::Transform: {
            float s = 8.0f;
            D2D1_RECT_F rc = D2D1::RectF(canvasX - s, canvasY - s, canvasX + s, canvasY + s);
            rt->DrawRectangle(rc, cursorBrush, 1.0f);
            rt->DrawLine(D2D1::Point2F(canvasX - s, canvasY - s), D2D1::Point2F(canvasX + s, canvasY + s), cursorBrush, 1.0f);
            rt->DrawLine(D2D1::Point2F(canvasX + s, canvasY - s), D2D1::Point2F(canvasX - s, canvasY + s), cursorBrush, 1.0f);
            break;
        }
        case ToolType::Text: {
            // I-beam cursor shape
            float w = 3.0f, h = 10.0f;
            D2D1_RECT_F rc = D2D1::RectF(canvasX - w*0.5f, canvasY - h*0.5f, canvasX + w*0.5f, canvasY + h*0.5f);
            rt->FillRectangle(rc, cursorBrush);
            rt->DrawLine(D2D1::Point2F(canvasX - w - 2, canvasY - h*0.5f), D2D1::Point2F(canvasX + w + 2, canvasY - h*0.5f), cursorBrush, 1.0f);
            rt->DrawLine(D2D1::Point2F(canvasX - w - 2, canvasY + h*0.5f), D2D1::Point2F(canvasX + w + 2, canvasY + h*0.5f), cursorBrush, 1.0f);
            break;
        }
        case ToolType::Shape: {
            float s = 6.0f;
            D2D1_RECT_F rc = D2D1::RectF(canvasX - s, canvasY - s, canvasX + s, canvasY + s);
            rt->DrawRectangle(rc, cursorBrush, 1.0f);
            break;
        }
        default:
            break;
    }
    
    cursorBrush->Release();
}

void CanvasView::OnMouseMove(const Point& pos) {
    // Track cursor for brush preview ring
    m_cursorScreenX = static_cast<float>(pos.x);
    m_cursorScreenY = static_cast<float>(pos.y);
    m_cursorInside = true;
    
    // Tablet pointer input (Windows Ink or WinTab)
    if (m_tablet && m_tablet->HasPressure() && m_isDrawing &&
        (m_currentTool == ToolType::Brush || m_currentTool == ToolType::Eraser)) {
        TabletInput::TabletState state = m_tablet->GetState();
        if (state.isTouching) {
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
    
    // Mouse fallback
    if (m_isDrawing && (m_currentTool == ToolType::Brush || m_currentTool == ToolType::Eraser)) {
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
    } else if (m_isDragging) {
        float canvasX, canvasY;
        ScreenToCanvas(static_cast<float>(pos.x), static_cast<float>(pos.y), canvasX, canvasY);
        m_dragCurrentX = canvasX;
        m_dragCurrentY = canvasY;
        switch (m_currentTool) {
            case ToolType::RectSelect:
                UpdateSelectionRect(canvasX, canvasY);
                break;
            case ToolType::EllipseSelect:
                UpdateSelectionEllipse(canvasX, canvasY);
                break;
            case ToolType::LassoSelect:
                UpdateLasso(canvasX, canvasY);
                break;
            case ToolType::Move:
                UpdateMove(canvasX, canvasY);
                break;
            case ToolType::Gradient:
                UpdateGradient(canvasX, canvasY);
                break;
            default:
                break;
        }
        Invalidate();
    } else {
        // Not drawing/panning: just refresh to show cursor ring
        Invalidate();
    }
}

void CanvasView::OnMouseDown(const Point& pos, MouseButton button) {
    if (!m_layerManager) return;
    
    if (button == MouseButton::Left) {
        float canvasX, canvasY;
        ScreenToCanvas(static_cast<float>(pos.x), static_cast<float>(pos.y), canvasX, canvasY);
        m_lastCanvasX = canvasX;
        m_lastCanvasY = canvasY;
        m_lastPressure = 1.0f;
        
        switch (m_currentTool) {
            case ToolType::Brush:
            case ToolType::Eraser: {
                m_isDrawing = true;
                auto layer = m_layerManager->GetActiveLayer();
                if (layer) layer->BeginStroke();
                ApplyBrush(canvasX, canvasY, m_lastPressure);
                Invalidate();
                break;
            }
            case ToolType::Eyedropper: {
                Color picked = PickColor(static_cast<int>(canvasX), static_cast<int>(canvasY));
                SetBrushColor(picked);
                SetCurrentTool(m_previousTool);
                break;
            }
            case ToolType::Fill: {
                ApplyFill(canvasX, canvasY);
                Invalidate();
                break;
            }
            case ToolType::Gradient: {
                StartGradient(canvasX, canvasY);
                m_isDragging = true;
                break;
            }
            case ToolType::Move: {
                StartMove(canvasX, canvasY);
                m_isDragging = true;
                break;
            }
            case ToolType::RectSelect: {
                StartSelectionRect(canvasX, canvasY);
                m_isDragging = true;
                break;
            }
            case ToolType::EllipseSelect: {
                StartSelectionEllipse(canvasX, canvasY);
                m_isDragging = true;
                break;
            }
            case ToolType::LassoSelect: {
                StartLasso(canvasX, canvasY);
                m_isDragging = true;
                break;
            }
            case ToolType::MagicWand: {
                if (!m_selection) {
                    m_selection = std::make_unique<SelectionMask>(m_canvasWidth, m_canvasHeight);
                }
                int ix = static_cast<int>(canvasX);
                int iy = static_cast<int>(canvasY);
                if (ix >= 0 && iy >= 0 && ix < static_cast<int>(m_canvasWidth) && iy < static_cast<int>(m_canvasHeight)) {
                    auto layer = m_layerManager->GetActiveLayer();
                    if (layer) {
                        m_selection->FloodFill(static_cast<uint32_t>(ix), static_cast<uint32_t>(iy), layer.get(), 32, 255);
                        Invalidate();
                    }
                }
                break;
            }
            case ToolType::Transform:
            case ToolType::Text:
            case ToolType::Shape: {
                // Stub: show a brief status or just invalidate
                Invalidate();
                break;
            }
            default:
                break;
        }
    } else if (button == MouseButton::Middle) {
        m_isPanning = true;
        m_panStartPos = pos;
    }
}

void CanvasView::OnMouseUp(const Point& pos, MouseButton button) {
    if (button == MouseButton::Left) {
        if (m_isDrawing) {
            std::cout << "[CV] OnMouseUp MOUSE isDrawing=false" << std::endl;
            m_isDrawing = false;
            auto layer = m_layerManager->GetActiveLayer();
            if (layer) layer->EndStroke();
        }
        if (m_isDragging) {
            m_isDragging = false;
            switch (m_currentTool) {
                case ToolType::RectSelect:
                    FinishSelectionRect();
                    break;
                case ToolType::EllipseSelect:
                    FinishSelectionEllipse();
                    break;
                case ToolType::LassoSelect:
                    FinishLasso();
                    break;
                case ToolType::Move:
                    FinishMove();
                    break;
                case ToolType::Gradient:
                    FinishGradient();
                    break;
                default:
                    break;
            }
            Invalidate();
        }
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
    
    Color drawColor = m_brushColor;
    if (m_currentTool == ToolType::Eraser) {
        drawColor = Color(0, 0, 0, 0);
    }
    brush.SetColor(drawColor);

    auto stamps = brush.GenerateStamps(m_lastCanvasX, m_lastCanvasY, m_lastPressure, x, y, pressure);

    for (const auto& stamp : stamps) {
        float size = brush.GetSize() * (0.2f + 0.8f * stamp.pressure);
        float opacity = brush.GetOpacity() * stamp.pressure;
        const SelectionMask* sel = (m_selection && !m_selection->IsEmpty()) ? m_selection.get() : nullptr;
        if (m_currentTool == ToolType::Eraser) {
            // Eraser: use Hard tip for clean edges, ignore wet mix
            layer->DrawBrushStamp(stamp.x, stamp.y, size * 0.5f, drawColor, opacity,
                                  Render::BrushTipType::RoundHard, 1.0f, 0.0f, sel);
        } else {
            layer->DrawBrushStamp(stamp.x, stamp.y, size * 0.5f, drawColor, opacity,
                                  brush.GetTipType(), brush.GetFlow(), brush.GetWetMix(), sel);
        }
    }
    
    m_layerManager->MarkCompositeDirty();
}

Color CanvasView::PickColor(int x, int y) {
    if (!m_layerManager) return Color(0,0,0,255);
    
    // Clamp to canvas bounds
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= static_cast<int>(m_canvasWidth)) x = static_cast<int>(m_canvasWidth) - 1;
    if (y >= static_cast<int>(m_canvasHeight)) y = static_cast<int>(m_canvasHeight) - 1;
    
    // Sample from composite buffer if available, otherwise from top visible layer
    for (int i = static_cast<int>(m_layerManager->GetLayerCount()) - 1; i >= 0; --i) {
        auto layer = m_layerManager->GetLayer(i);
        if (!layer || !layer->IsVisible()) continue;
        Color c = layer->GetPixel(static_cast<uint32_t>(x), static_cast<uint32_t>(y));
        if (c.a > 0) return c;
    }
    return m_bgColor;
}

void CanvasView::ApplyFill(float x, float y) {
    if (!m_layerManager) return;
    auto layer = m_layerManager->GetActiveLayer();
    if (!layer || layer->IsLocked()) return;
    
    int ix = static_cast<int>(x);
    int iy = static_cast<int>(y);
    if (ix < 0 || iy < 0 || ix >= static_cast<int>(m_canvasWidth) || iy >= static_cast<int>(m_canvasHeight)) return;
    
    // Simple flood fill with tolerance
    Color target = layer->GetPixel(static_cast<uint32_t>(ix), static_cast<uint32_t>(iy));
    if (target.r == m_brushColor.r && target.g == m_brushColor.g && target.b == m_brushColor.b && target.a == m_brushColor.a) return; // Already target color
    
    std::vector<std::pair<int,int>> stack;
    stack.reserve(1024);
    stack.push_back({ix, iy});
    
    const int tolerance = 32;
    auto match = [&](const Color& c) -> bool {
        return std::abs(static_cast<int>(c.r) - static_cast<int>(target.r)) <= tolerance &&
               std::abs(static_cast<int>(c.g) - static_cast<int>(target.g)) <= tolerance &&
               std::abs(static_cast<int>(c.b) - static_cast<int>(target.b)) <= tolerance &&
               std::abs(static_cast<int>(c.a) - static_cast<int>(target.a)) <= tolerance;
    };
    
    // Use a simple visited grid (could be optimized with bitset)
    std::vector<bool> visited(m_canvasWidth * m_canvasHeight, false);
    auto idx = [&](int x, int y) { return y * m_canvasWidth + x; };
    
    while (!stack.empty()) {
        auto [cx, cy] = stack.back();
        stack.pop_back();
        
        if (cx < 0 || cy < 0 || cx >= static_cast<int>(m_canvasWidth) || cy >= static_cast<int>(m_canvasHeight)) continue;
        size_t i = idx(cx, cy);
        if (visited[i]) continue;
        
        Color c = layer->GetPixel(static_cast<uint32_t>(cx), static_cast<uint32_t>(cy));
        if (!match(c)) continue;
        
        visited[i] = true;
        if (!HasSelection() || m_selection->GetPixel(static_cast<uint32_t>(cx), static_cast<uint32_t>(cy)) > 0) {
            layer->SetPixel(static_cast<uint32_t>(cx), static_cast<uint32_t>(cy), m_brushColor);
        }
        
        stack.push_back({cx + 1, cy});
        stack.push_back({cx - 1, cy});
        stack.push_back({cx, cy + 1});
        stack.push_back({cx, cy - 1});
    }
    
    m_layerManager->MarkCompositeDirty();
}

LRESULT CanvasView::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_POINTERDOWN:
        case WM_POINTERUPDATE:
        case WM_POINTERUP:
            if (m_tablet) {
                bool processed = m_tablet->ProcessMessage(msg, wParam, lParam);
                if (processed) {
                    TabletInput::TabletState state = m_tablet->GetState();
                    if (msg == WM_POINTERDOWN) {
                        std::cout << "[CV] WM_POINTERDOWN processed=" << processed
                                  << " state.x=" << state.x << " y=" << state.y
                                  << " p=" << state.pressure << " touching=" << state.isTouching << std::endl;
                        float canvasX, canvasY;
                        ScreenToCanvas(state.x, state.y, canvasX, canvasY);
                        m_lastCanvasX = canvasX;
                        m_lastCanvasY = canvasY;
                        m_lastPressure = state.pressure;
                        if (m_currentTool == ToolType::Brush || m_currentTool == ToolType::Eraser) {
                            m_isDrawing = true;
                            auto layer = m_layerManager->GetActiveLayer();
                            if (layer) layer->BeginStroke();
                            ApplyBrush(canvasX, canvasY, state.pressure);
                        }
                        Invalidate();
                        return 0;
                    } else if (msg == WM_POINTERUPDATE) {
                        // Track pen hover position for brush preview ring
                        m_cursorScreenX = state.x;
                        m_cursorScreenY = state.y;
                        m_cursorInside = true;
                        if (state.isTouching && m_isDrawing &&
                            (m_currentTool == ToolType::Brush || m_currentTool == ToolType::Eraser)) {
                            float canvasX, canvasY;
                            ScreenToCanvas(state.x, state.y, canvasX, canvasY);
                            ApplyBrush(canvasX, canvasY, state.pressure);
                            m_lastCanvasX = canvasX;
                            m_lastCanvasY = canvasY;
                            m_lastPressure = state.pressure;
                        }
                        Invalidate();
                        return 0;
                    } else if (msg == WM_POINTERUP) {
                        std::cout << "[CV] WM_POINTERUP isDrawing=false" << std::endl;
                        if (m_isDrawing) {
                            m_isDrawing = false;
                            auto layer = m_layerManager->GetActiveLayer();
                            if (layer) layer->EndStroke();
                        }
                        Invalidate();
                        return 0;
                    }
                }
            }
            break;
            
        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT) {
                SetCursor(nullptr);
                return TRUE;
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

// -------------------------------------------------------------------------
// Selection helpers
// -------------------------------------------------------------------------
void CanvasView::ClearSelection() {
    if (m_selection) {
        m_selection->Clear();
    }
}

bool CanvasView::HasSelection() const {
    return m_selection && !m_selection->IsEmpty();
}

// -------------------------------------------------------------------------
// Rectangular selection
// -------------------------------------------------------------------------
void CanvasView::StartSelectionRect(float x, float y) {
    if (!m_selection) {
        m_selection = std::make_unique<SelectionMask>(m_canvasWidth, m_canvasHeight);
    }
    m_selection->Clear();
    m_dragStartX = x;
    m_dragStartY = y;
    m_dragCurrentX = x;
    m_dragCurrentY = y;
}

void CanvasView::UpdateSelectionRect(float x, float y) {
    m_dragCurrentX = x;
    m_dragCurrentY = y;
}

void CanvasView::FinishSelectionRect() {
    if (!m_selection) return;
    int x1 = static_cast<int>(std::floor(std::min(m_dragStartX, m_dragCurrentX)));
    int y1 = static_cast<int>(std::floor(std::min(m_dragStartY, m_dragCurrentY)));
    int x2 = static_cast<int>(std::ceil(std::max(m_dragStartX, m_dragCurrentX)));
    int y2 = static_cast<int>(std::ceil(std::max(m_dragStartY, m_dragCurrentY)));
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > static_cast<int>(m_canvasWidth)) x2 = static_cast<int>(m_canvasWidth);
    if (y2 > static_cast<int>(m_canvasHeight)) y2 = static_cast<int>(m_canvasHeight);
    if (x2 > x1 && y2 > y1) {
        m_selection->FillRect(static_cast<uint32_t>(x1), static_cast<uint32_t>(y1),
                              static_cast<uint32_t>(x2 - x1), static_cast<uint32_t>(y2 - y1), 255);
    }
}

// -------------------------------------------------------------------------
// Elliptical selection
// -------------------------------------------------------------------------
void CanvasView::StartSelectionEllipse(float x, float y) {
    if (!m_selection) {
        m_selection = std::make_unique<SelectionMask>(m_canvasWidth, m_canvasHeight);
    }
    m_selection->Clear();
    m_dragStartX = x;
    m_dragStartY = y;
    m_dragCurrentX = x;
    m_dragCurrentY = y;
}

void CanvasView::UpdateSelectionEllipse(float x, float y) {
    m_dragCurrentX = x;
    m_dragCurrentY = y;
}

void CanvasView::FinishSelectionEllipse() {
    if (!m_selection) return;
    float cx = (m_dragStartX + m_dragCurrentX) * 0.5f;
    float cy = (m_dragStartY + m_dragCurrentY) * 0.5f;
    float rx = std::abs(m_dragCurrentX - m_dragStartX) * 0.5f;
    float ry = std::abs(m_dragCurrentY - m_dragStartY) * 0.5f;
    if (rx > 0.5f && ry > 0.5f) {
        m_selection->FillEllipse(static_cast<uint32_t>(cx), static_cast<uint32_t>(cy),
                                 static_cast<uint32_t>(rx), static_cast<uint32_t>(ry), 255);
    }
}

// -------------------------------------------------------------------------
// Lasso selection
// -------------------------------------------------------------------------
void CanvasView::StartLasso(float x, float y) {
    if (!m_selection) {
        m_selection = std::make_unique<SelectionMask>(m_canvasWidth, m_canvasHeight);
    }
    m_selection->Clear();
    m_lassoPoints.clear();
    m_lassoPoints.push_back(Point(static_cast<int32_t>(x), static_cast<int32_t>(y)));
    m_dragStartX = x;
    m_dragStartY = y;
}

void CanvasView::UpdateLasso(float x, float y) {
    if (m_lassoPoints.empty()) return;
    const auto& last = m_lassoPoints.back();
    int dx = static_cast<int>(x) - last.x;
    int dy = static_cast<int>(y) - last.y;
    if (dx * dx + dy * dy > 16) { // minimum 4px spacing
        m_lassoPoints.push_back(Point(static_cast<int32_t>(x), static_cast<int32_t>(y)));
    }
}

void CanvasView::FinishLasso() {
    if (!m_selection || m_lassoPoints.size() < 3) return;
    m_selection->FillPolygon(m_lassoPoints, 255);
    m_lassoPoints.clear();
}

// -------------------------------------------------------------------------
// Move tool — simple pixel shift of the active layer (or selection content)
// -------------------------------------------------------------------------
void CanvasView::StartMove(float x, float y) {
    m_dragStartX = x;
    m_dragStartY = y;
    m_dragCurrentX = x;
    m_dragCurrentY = y;
    m_moveState.offsetX = 0;
    m_moveState.offsetY = 0;
    m_moveState.hasSelection = HasSelection();
    
    auto layer = m_layerManager->GetActiveLayer();
    if (!layer) return;
    
    // Capture the region we intend to move
    if (m_moveState.hasSelection) {
        // Compute selection bounding box
        int minX = static_cast<int>(m_canvasWidth), minY = static_cast<int>(m_canvasHeight);
        int maxX = 0, maxY = 0;
        for (uint32_t y = 0; y < m_canvasHeight; ++y) {
            for (uint32_t x = 0; x < m_canvasWidth; ++x) {
                if (m_selection->GetPixel(x, y) > 0) {
                    minX = std::min(minX, static_cast<int>(x));
                    minY = std::min(minY, static_cast<int>(y));
                    maxX = std::max(maxX, static_cast<int>(x));
                    maxY = std::max(maxY, static_cast<int>(y));
                }
            }
        }
        if (minX > maxX || minY > maxY) {
            m_moveState.hasSelection = false;
            // Fall through to full-layer move
        } else {
            m_moveState.srcX = minX;
            m_moveState.srcY = minY;
            m_moveState.width = maxX - minX + 1;
            m_moveState.height = maxY - minY + 1;
            m_moveState.pixelBuffer.resize(static_cast<size_t>(m_moveState.width) * m_moveState.height * 4);
            for (int py = 0; py < m_moveState.height; ++py) {
                for (int px = 0; px < m_moveState.width; ++px) {
                    Color c = layer->GetPixel(static_cast<uint32_t>(minX + px), static_cast<uint32_t>(minY + py));
                    size_t idx = (py * m_moveState.width + px) * 4;
                    m_moveState.pixelBuffer[idx + 0] = c.b;
                    m_moveState.pixelBuffer[idx + 1] = c.g;
                    m_moveState.pixelBuffer[idx + 2] = c.r;
                    m_moveState.pixelBuffer[idx + 3] = c.a;
                }
            }
            // Clear original pixels in selection
            for (int py = 0; py < m_moveState.height; ++py) {
                for (int px = 0; px < m_moveState.width; ++px) {
                    if (m_selection->GetPixel(static_cast<uint32_t>(minX + px), static_cast<uint32_t>(minY + py)) > 0) {
                        layer->SetPixel(static_cast<uint32_t>(minX + px), static_cast<uint32_t>(minY + py), Color(0,0,0,0));
                    }
                }
            }
            return;
        }
    }
    
    // Full-layer move: capture entire layer bbox (simplified: whole canvas)
    m_moveState.srcX = 0;
    m_moveState.srcY = 0;
    m_moveState.width = static_cast<int>(m_canvasWidth);
    m_moveState.height = static_cast<int>(m_canvasHeight);
    m_moveState.pixelBuffer.resize(static_cast<size_t>(m_moveState.width) * m_moveState.height * 4);
    for (int py = 0; py < m_moveState.height; ++py) {
        for (int px = 0; px < m_moveState.width; ++px) {
            Color c = layer->GetPixel(static_cast<uint32_t>(px), static_cast<uint32_t>(py));
            size_t idx = (py * m_moveState.width + px) * 4;
            m_moveState.pixelBuffer[idx + 0] = c.b;
            m_moveState.pixelBuffer[idx + 1] = c.g;
            m_moveState.pixelBuffer[idx + 2] = c.r;
            m_moveState.pixelBuffer[idx + 3] = c.a;
        }
    }
    layer->Clear();
}

void CanvasView::UpdateMove(float x, float y) {
    m_dragCurrentX = x;
    m_dragCurrentY = y;
    m_moveState.offsetX = static_cast<int>(x - m_dragStartX);
    m_moveState.offsetY = static_cast<int>(y - m_dragStartY);
}

void CanvasView::FinishMove() {
    auto layer = m_layerManager->GetActiveLayer();
    if (!layer) return;
    
    int dstX = m_moveState.srcX + m_moveState.offsetX;
    int dstY = m_moveState.srcY + m_moveState.offsetY;
    
    for (int py = 0; py < m_moveState.height; ++py) {
        for (int px = 0; px < m_moveState.width; ++px) {
            int tx = dstX + px;
            int ty = dstY + py;
            if (tx < 0 || ty < 0 || tx >= static_cast<int>(m_canvasWidth) || ty >= static_cast<int>(m_canvasHeight)) continue;
            
            if (m_moveState.hasSelection) {
                // Only move pixels that were originally selected
                int srcPixelX = m_moveState.srcX + px;
                int srcPixelY = m_moveState.srcY + py;
                if (srcPixelX < 0 || srcPixelY < 0 ||
                    srcPixelX >= static_cast<int>(m_canvasWidth) || srcPixelY >= static_cast<int>(m_canvasHeight)) continue;
                if (m_selection->GetPixel(static_cast<uint32_t>(srcPixelX), static_cast<uint32_t>(srcPixelY)) == 0) continue;
            }
            
            size_t idx = (py * m_moveState.width + px) * 4;
            uint8_t b = m_moveState.pixelBuffer[idx + 0];
            uint8_t g = m_moveState.pixelBuffer[idx + 1];
            uint8_t r = m_moveState.pixelBuffer[idx + 2];
            uint8_t a = m_moveState.pixelBuffer[idx + 3];
            layer->SetPixel(static_cast<uint32_t>(tx), static_cast<uint32_t>(ty), Color(r, g, b, a));
        }
    }
    
    m_moveState.pixelBuffer.clear();
    m_moveState.offsetX = 0;
    m_moveState.offsetY = 0;
    m_layerManager->MarkCompositeDirty();
}

// -------------------------------------------------------------------------
// Gradient tool — linear gradient between two points
// -------------------------------------------------------------------------
void CanvasView::StartGradient(float x, float y) {
    m_dragStartX = x;
    m_dragStartY = y;
    m_dragCurrentX = x;
    m_dragCurrentY = y;
}

void CanvasView::UpdateGradient(float x, float y) {
    m_dragCurrentX = x;
    m_dragCurrentY = y;
}

void CanvasView::FinishGradient() {
    ApplyGradient(m_dragStartX, m_dragStartY, m_dragCurrentX, m_dragCurrentY);
}

void CanvasView::ApplyGradient(float x1, float y1, float x2, float y2) {
    if (!m_layerManager) return;
    auto layer = m_layerManager->GetActiveLayer();
    if (!layer || layer->IsLocked()) return;
    
    float dx = x2 - x1;
    float dy = y2 - y1;
    float lenSq = dx * dx + dy * dy;
    if (lenSq < 0.001f) return;
    float len = std::sqrt(lenSq);
    float nx = dx / len;
    float ny = dy / len;
    
    Color c1 = m_brushColor;
    Color c2 = Color::FromHex(0xFFFFFF); // Secondary color: white for now (TODO: add secondary color picker)
    
    for (uint32_t y = 0; y < m_canvasHeight; ++y) {
        for (uint32_t x = 0; x < m_canvasWidth; ++x) {
            if (HasSelection() && m_selection->GetPixel(x, y) == 0) continue;
            
            float px = static_cast<float>(x) - x1;
            float py = static_cast<float>(y) - y1;
            float proj = px * nx + py * ny; // projection onto gradient axis
            float t = std::clamp(proj / len, 0.0f, 1.0f);
            
            uint8_t r = static_cast<uint8_t>(c1.r * (1.0f - t) + c2.r * t);
            uint8_t g = static_cast<uint8_t>(c1.g * (1.0f - t) + c2.g * t);
            uint8_t b = static_cast<uint8_t>(c1.b * (1.0f - t) + c2.b * t);
            uint8_t a = static_cast<uint8_t>(c1.a * (1.0f - t) + c2.a * t);
            
            layer->SetPixel(x, y, Color(r, g, b, a));
        }
    }
    
    m_layerManager->MarkCompositeDirty();
}

// -------------------------------------------------------------------------
// D2D selection outline (marching ants)
// -------------------------------------------------------------------------
void CanvasView::DrawSelectionOutline(ID2D1RenderTarget* rt) {
    if (!rt || !m_selection || m_selection->IsEmpty()) return;
    
    // For performance, sample boundary pixels sparsely
    std::vector<std::pair<int, int>> boundary;
    m_selection->GetBoundaryPixels(boundary, 4);
    if (boundary.empty()) return;
    
    ID2D1SolidColorBrush* brush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.8f), &brush);
    if (!brush) return;
    
    // Draw small dots along the boundary
    for (const auto& [x, y] : boundary) {
        D2D1_RECT_F rc = D2D1::RectF(static_cast<float>(x), static_cast<float>(y),
                                      static_cast<float>(x + 1), static_cast<float>(y + 1));
        rt->FillRectangle(rc, brush);
    }
    
    brush->Release();
}

} // namespace UI
} // namespace VividPic
