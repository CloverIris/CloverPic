#pragma once

#include "Core/Input/InputEvents.h"
#include "Core/History.h"
#include "Core/LayerManager.h"
#include "Core/Project.h"
#include "Core/Render/BrushEngine.h"
#include "Core/SelectionMask.h"
#include "Core/Presentation/SoftRenderer.h"
#include "Core/App/ToolType.h"

namespace CloverPic::Core {

class CanvasController {
public:
    CanvasController();

    bool NewCanvas(uint32_t width, uint32_t height, float dpi, bool transparent);
    bool AttachLoadedProject(Ref<Project> project);

    void Render(Presentation::SoftRenderer& renderer, const Rect& viewport);
    void ResizeViewport(const Rect& viewport);
    void HandlePointer(const Input::PointerEvent& event, const Rect& viewport);
    void HandleWheel(int delta, const Point& position, const Rect& viewport);

    void SetTool(ToolType tool) { m_tool = tool; }
    ToolType GetTool() const { return m_tool; }
    void SetColor(const Color& color);
    Color GetColor() const { return m_color; }
    void SetBrushSize(float size);
    float GetBrushSize() const;
    void SelectLayer(size_t index);

    Ref<Project> GetProject() const { return m_project; }
    LayerManager* GetLayerManager() { return &m_layerManager; }
    const LayerManager* GetLayerManager() const { return &m_layerManager; }
    HistoryManager* GetHistoryManager() { return &m_history; }
    float GetZoom() const { return m_zoom; }
    Point GetPan() const { return Point(static_cast<int32_t>(m_panX), static_cast<int32_t>(m_panY)); }
    bool IsInteracting() const { return m_drawing || m_panning; }

private:
    void ScreenToCanvas(const Point& position, const Rect& viewport, float& canvasX, float& canvasY) const;
    void ApplyBrush(float x, float y, float pressure);
    void FitToViewport(const Rect& viewport);
    void EnsureCompositeBuffer();

    Ref<Project> m_project;
    LayerManager m_layerManager;
    HistoryManager m_history;
    Render::BrushEngine m_brush;
    std::vector<uint8_t> m_compositeBuffer;
    ToolType m_tool = ToolType::Brush;
    Color m_color = Color::FromHex(0x111111);
    float m_zoom = 1.0f;
    float m_panX = 0.0f;
    float m_panY = 0.0f;
    bool m_drawing = false;
    bool m_panning = false;
    Point m_lastPointer;
    float m_lastCanvasX = 0.0f;
    float m_lastCanvasY = 0.0f;
    float m_lastPressure = 1.0f;
};

} // namespace CloverPic::Core
