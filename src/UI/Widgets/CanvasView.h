#pragma once

#include "UI/Core/Window.h"
#include "UI/Core/ToolType.h"
#include "Render/BrushEngine.h"
#include "Tablet/TabletInput.h"
#include "Core/LayerManager.h"
#include "Core/SelectionMask.h"
#include "UI/Core/Theme.h"
#include <d2d1.h>

namespace VividPic {
namespace UI {

class CanvasView : public Window {
public:
    CanvasView();
    ~CanvasView() override;
    
    bool InitializeCanvas(uint32_t width, uint32_t height, const Color& bgColor, bool transparent = false, LayerType initialLayer = LayerType::Color);
    void ShutdownCanvas();
    
    // View transform
    void SetZoom(float zoom);
    float GetZoom() const { return m_zoom; }
    void SetPan(float x, float y);
    Point GetPan() const { return Point(static_cast<int32_t>(m_panX), static_cast<int32_t>(m_panY)); }
    void SetRotation(float degrees);
    float GetRotation() const { return m_viewRotation; }
    
    // View navigation
    void FitToWindow();
    void ResetView();
    
    // Screen to canvas coordinates
    void ScreenToCanvas(float screenX, float screenY, float& canvasX, float& canvasY) const;
    
    // Force re-composite and render
    void InvalidateCanvas();
    void ApplyBrush(float x, float y, float pressure);
    Color PickColor(int x, int y);
    void ApplyFill(float x, float y);
    void ApplyGradient(float x1, float y1, float x2, float y2);
    
    // Layer access
    LayerManager* GetLayerManager() const { return m_layerManager; }
    
    // Current brush color
    void SetBrushColor(const Color& color);
    Color GetBrushColor() const { return m_brushColor; }
    
    // Current tool
    void SetCurrentTool(ToolType tool);
    ToolType GetCurrentTool() const { return m_currentTool; }
    
    // Selection
    SelectionMask* GetSelection() const { return m_selection.get(); }
    void ClearSelection();
    bool HasSelection() const;
    
protected:
    void OnPaint(HDC hdc, const Rect& clip) override;
    void OnSize(const Size& newSize) override;
    void OnMouseMove(const Point& pos) override;
    void OnMouseDown(const Point& pos, MouseButton button) override;
    void OnMouseUp(const Point& pos, MouseButton button) override;
    void OnMouseDoubleClick(const Point& pos, MouseButton button) override {}
    void OnKeyDown(uint32_t keyCode) override;
    void OnKeyUp(uint32_t keyCode) override;
    
    bool OnCreate() override;
    void OnDestroy() override;
    
    DWORD GetDefaultStyle() const override { return WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN; }
    
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    
private:
    void RenderCanvas();
    void CompositeAndRender();
    void DrawCheckerboard(ID2D1RenderTarget* rt);
    void DrawCanvasBorder(ID2D1RenderTarget* rt);
    void DrawBrushCursor(ID2D1RenderTarget* rt);
    void DrawSelectionOutline(ID2D1RenderTarget* rt);
    void UpdateCompositeBitmap();
    
    // Tool-specific helpers
    void StartSelectionRect(float x, float y);
    void UpdateSelectionRect(float x, float y);
    void FinishSelectionRect();
    void StartSelectionEllipse(float x, float y);
    void UpdateSelectionEllipse(float x, float y);
    void FinishSelectionEllipse();
    void StartLasso(float x, float y);
    void UpdateLasso(float x, float y);
    void FinishLasso();
    void StartMove(float x, float y);
    void UpdateMove(float x, float y);
    void FinishMove();
    void StartGradient(float x, float y);
    void UpdateGradient(float x, float y);
    void FinishGradient();
    
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
    float m_viewRotation = 0.0f; // degrees
    
    // Spacebar panning
    bool m_spacePanning = false;
    ToolType m_spaceRestoreTool = ToolType::Brush;
    
    // Input state
    bool m_isDrawing = false;
    bool m_isPanning = false;
    Point m_lastMousePos;
    Point m_panStartPos;
    float m_lastCanvasX = 0.0f;
    float m_lastCanvasY = 0.0f;
    float m_lastPressure = 1.0f;
    
    // Cursor tracking for brush preview ring
    float m_cursorScreenX = 0.0f;
    float m_cursorScreenY = 0.0f;
    bool m_cursorInside = false;
    
    // Brush color
    Color m_brushColor = Color::FromHex(0x000000);
    
    // Tool
    ToolType m_currentTool = ToolType::Brush;
    ToolType m_previousTool = ToolType::Brush; // For eyedropper restore
    
    // Selection
    std::unique_ptr<SelectionMask> m_selection;
    
    // Tool interaction state
    bool m_isDragging = false;         // Generic drag gesture active
    float m_dragStartX = 0.0f;         // Canvas coords
    float m_dragStartY = 0.0f;
    float m_dragCurrentX = 0.0f;
    float m_dragCurrentY = 0.0f;
    std::vector<Point> m_lassoPoints;  // For lasso selection
    
    // Move tool state
    struct MoveState {
        std::vector<uint8_t> pixelBuffer; // RGBA copy of moved region
        int srcX = 0, srcY = 0;
        int width = 0, height = 0;
        int offsetX = 0, offsetY = 0;
        bool hasSelection = false;
    };
    MoveState m_moveState;
    
    // Tablet
    TabletInput::TabletManager* m_tablet = nullptr;
};

} // namespace UI
} // namespace VividPic
