#include "Core/App/CanvasController.h"
#include "Core/LayerType.h"
#include "Core/Render/BrushEngine.h"
#include <algorithm>

namespace CloverPic::Core {

CanvasController::CanvasController() {
    m_layerManager.SetHistoryManager(&m_history);
}

bool CanvasController::NewCanvas(uint32_t width, uint32_t height, float dpi, bool transparent) {
    m_project = MakeRef<Project>(L"Untitled");
    m_project->GetCanvas().widthPx = width;
    m_project->GetCanvas().heightPx = height;
    m_project->GetCanvas().dpi = dpi;
    m_project->GetCanvas().transparent = transparent;
    m_project->GetCanvas().initialLayerType = transparent ? LayerType::Transparent : LayerType::Color;

    m_history.Clear();
    m_layerManager.Initialize(width, height);
    auto layer = m_layerManager.AddLayer(L"Layer 1", m_project->GetCanvas().initialLayerType);
    if (layer && !transparent) {
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                layer->SetPixel(x, y, Color(255, 255, 255, 255));
            }
        }
    }
    EnsureCompositeBuffer();
    return true;
}

bool CanvasController::AttachLoadedProject(Ref<Project> project) {
    if (!project) {
        return false;
    }
    m_project = std::move(project);
    m_layerManager.SetHistoryManager(&m_history);
    m_history.Clear();
    EnsureCompositeBuffer();
    return true;
}

void CanvasController::Render(Presentation::SoftRenderer& renderer, const Rect& viewport) {
    if (!m_project || viewport.Width() <= 0 || viewport.Height() <= 0) {
        return;
    }

    const auto& canvas = m_project->GetCanvas();
    renderer.DrawCheckerboard(viewport, 12, Color(82, 84, 89), Color(68, 70, 74));
    if (m_layerManager.NeedsComposite()) {
        EnsureCompositeBuffer();
        m_layerManager.CompositeToBuffer(m_compositeBuffer.data(), canvas.widthPx, canvas.heightPx);
    }

    Rect canvasRect(
        static_cast<int32_t>(viewport.left + m_panX),
        static_cast<int32_t>(viewport.top + m_panY),
        static_cast<int32_t>(viewport.left + m_panX + canvas.widthPx * m_zoom),
        static_cast<int32_t>(viewport.top + m_panY + canvas.heightPx * m_zoom));
    renderer.FillRect(canvasRect, Color(255, 255, 255, 255));
    renderer.BlitBgra(canvasRect, m_compositeBuffer.data(), canvas.widthPx, canvas.heightPx);
    renderer.StrokeRect(canvasRect, Color(20, 22, 26, 255), 2);

    if (viewport.Contains(m_lastPointer)) {
        const int radius = std::max(4, static_cast<int>(m_brush.GetSize() * m_zoom * 0.5f));
        renderer.StrokeRect(Rect(m_lastPointer.x - radius, m_lastPointer.y - radius, m_lastPointer.x + radius, m_lastPointer.y + radius), Color(250, 250, 250, 180), 1);
    }
}

void CanvasController::ResizeViewport(const Rect& viewport) {
    if (m_project) {
        FitToViewport(viewport);
    }
}

void CanvasController::HandlePointer(const Input::PointerEvent& event, const Rect& viewport) {
    const Point previousPointer = m_lastPointer;
    m_lastPointer = event.position;
    if (!m_project) {
        return;
    }

    if (event.action == Input::PointerAction::Down && event.button == MouseButton::Middle) {
        m_panning = true;
        return;
    }
    if (event.action == Input::PointerAction::Up && event.button == MouseButton::Middle) {
        m_panning = false;
        return;
    }
    if (event.action == Input::PointerAction::Move && m_panning) {
        m_panX += event.position.x - previousPointer.x;
        m_panY += event.position.y - previousPointer.y;
        return;
    }

    float canvasX = 0;
    float canvasY = 0;
    ScreenToCanvas(event.position, viewport, canvasX, canvasY);

    if (event.action == Input::PointerAction::Down && event.button == MouseButton::Left) {
        if (m_tool == ToolType::Brush || m_tool == ToolType::Eraser) {
            m_drawing = true;
            if (auto layer = m_layerManager.GetActiveLayer()) {
                layer->BeginStroke();
            }
            m_lastCanvasX = canvasX;
            m_lastCanvasY = canvasY;
            m_lastPressure = std::max(0.05f, event.pressure);
            ApplyBrush(canvasX, canvasY, m_lastPressure);
        }
    } else if (event.action == Input::PointerAction::Move && m_drawing) {
        const float pressure = std::max(0.05f, event.pressure);
        ApplyBrush(canvasX, canvasY, pressure);
        m_lastCanvasX = canvasX;
        m_lastCanvasY = canvasY;
        m_lastPressure = pressure;
    } else if (event.action == Input::PointerAction::Up && event.button == MouseButton::Left) {
        if (m_drawing) {
            m_drawing = false;
            if (auto layer = m_layerManager.GetActiveLayer()) {
                layer->EndStroke();
            }
        }
    }
}

void CanvasController::HandleWheel(int delta, const Point& position, const Rect& viewport) {
    if (!m_project) return;
    float beforeX = 0;
    float beforeY = 0;
    ScreenToCanvas(position, viewport, beforeX, beforeY);
    m_zoom = std::clamp(m_zoom * (delta > 0 ? 1.1f : 0.9f), 0.05f, 16.0f);
    m_panX = position.x - viewport.left - beforeX * m_zoom;
    m_panY = position.y - viewport.top - beforeY * m_zoom;
}

void CanvasController::SetColor(const Color& color) {
    m_color = color;
    m_brush.SetColor(color);
}

void CanvasController::SetBrushSize(float size) {
    m_brush.SetSize(size);
}

float CanvasController::GetBrushSize() const {
    return m_brush.GetSize();
}

void CanvasController::SelectLayer(size_t index) {
    m_layerManager.SetActiveLayer(index);
}

void CanvasController::ScreenToCanvas(const Point& position, const Rect& viewport, float& canvasX, float& canvasY) const {
    canvasX = (position.x - viewport.left - m_panX) / std::max(0.001f, m_zoom);
    canvasY = (position.y - viewport.top - m_panY) / std::max(0.001f, m_zoom);
}

void CanvasController::ApplyBrush(float x, float y, float pressure) {
    auto layer = m_layerManager.GetActiveLayer();
    if (!layer || layer->IsLocked()) {
        return;
    }

    auto& brush = m_brush;
    Color drawColor = m_tool == ToolType::Eraser ? Color(0, 0, 0, 0) : m_color;
    auto stamps = brush.GenerateStamps(m_lastCanvasX, m_lastCanvasY, m_lastPressure, x, y, pressure);
    for (const auto& stamp : stamps) {
        const float size = brush.GetSize() * brush.ApplyPressureCurve(stamp.pressure);
        layer->DrawBrushStamp(stamp.x, stamp.y, size * 0.5f, drawColor, brush.GetOpacity(),
                              brush.GetTipType(), brush.GetFlow(), brush.GetWetMix(), nullptr);
    }
    m_layerManager.MarkCompositeDirty();
}

void CanvasController::FitToViewport(const Rect& viewport) {
    if (!m_project || viewport.Width() <= 0 || viewport.Height() <= 0) return;
    const auto& canvas = m_project->GetCanvas();
    const float zx = (viewport.Width() - 80.0f) / std::max(1u, canvas.widthPx);
    const float zy = (viewport.Height() - 80.0f) / std::max(1u, canvas.heightPx);
    m_zoom = std::clamp(std::min(zx, zy), 0.05f, 2.0f);
    m_panX = (viewport.Width() - canvas.widthPx * m_zoom) * 0.5f;
    m_panY = (viewport.Height() - canvas.heightPx * m_zoom) * 0.5f;
}

void CanvasController::EnsureCompositeBuffer() {
    if (!m_project) return;
    const auto& canvas = m_project->GetCanvas();
    m_compositeBuffer.resize(static_cast<size_t>(canvas.widthPx) * canvas.heightPx * 4, 0);
}

} // namespace CloverPic::Core
