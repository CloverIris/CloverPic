#pragma once

#include "UI/Core/Window.h"
#include "Render/BrushEngine.h"
#include "Tablet/TabletInput.h"
#include "Core/LayerManager.h"
#include "UI/Core/Theme.h"
#include <d2d1.h>

namespace VividPic {
namespace UI {

class CanvasView : public Window {
public:
    CanvasView();
    ~CanvasView() override;
    
    bool InitializeCanvas(uint32_t width, uint32_t height, const Color& bgColor);
    void ShutdownCanvas();
    
    // View transform
    void SetZoom(float zoom);
    float GetZoom() const { return m_zoom; }
    void SetPan(float x, float y);
    Point GetPan() const { return Point(static_cast<int32_t>(m_panX), static_cast<int32_t>(m_panY)); }
    
    // Screen to canvas coordinates
    void ScreenToCanvas(float screenX, float screenY, float& canvasX, float& canvasY) const;
    
    // Force re-composite and render
    void InvalidateCanvas();
    void ApplyBrush(float x, float y, float pressure);
    
    // Layer access
    LayerManager* GetLayerManager() const { return m_layerManager; }
    
    // Current brush color
    void SetBrushColor(const Color& color);
    Color GetBrushColor() const { return m_brushColor; }
    
protected:
    void OnPaint(HDC hdc, const Rect& clip) override;
    void OnSize(const Size& newSize) override;
    void OnMouseMove(const Point& pos) override;
    void OnMouseDown(const Point& pos, MouseButton button) override;
    void OnMouseUp(const Point& pos, MouseButton button) override;
    void OnMouseDoubleClick(const Point& pos, MouseButton button) override {}
    void OnKeyDown(uint32_t keyCode) override;
    
    bool OnCreate() override;
    void OnDestroy() override;
    
    DWORD GetDefaultStyle() const override { return WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN; }
    
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    
private:
    void RenderCanvas();
    void CompositeAndRender();
    void DrawCheckerboard(ID2D1RenderTarget* rt);
    void DrawCanvasBorder(ID2D1RenderTarget* rt);
    void UpdateCompositeBitmap();
    
    // D2D
    ID2D1HwndRenderTarget* m_renderTarget = nullptr;
    ID2D1Bitmap* m_compositeBitmap = nullptr;
    std::vector<uint8_t> m_compositeBuffer;
    
    // Layer management
    LayerManager* m_layerManager = nullptr;
    uint32_t m_canvasWidth = 0;
    uint32_t m_canvasHeight = 0;
    Color m_bgColor;
    bool m_transparentBg = false;
    
    // View state
    float m_zoom = 1.0f;
    float m_panX = 0.0f;
    float m_panY = 0.0f;
    
    // Input state
    bool m_isDrawing = false;
    bool m_isPanning = false;
    Point m_lastMousePos;
    Point m_panStartPos;
    float m_lastCanvasX = 0.0f;
    float m_lastCanvasY = 0.0f;
    float m_lastPressure = 1.0f;
    
    // Brush color
    Color m_brushColor = Color::FromHex(0x000000);
    
    // Tablet
    TabletInput::TabletManager* m_tablet = nullptr;
};

} // namespace UI
} // namespace VividPic
