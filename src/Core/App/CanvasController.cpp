#include "Core/App/CanvasController.h"
#include "Core/Filters.h"
#include "Core/LayerType.h"
#include "Core/Render/BrushEngine.h"
#include "Core/TextLayer.h"
#include <algorithm>
#include <cmath>

namespace CloverPic::Core {

CanvasController::CanvasController() {
    m_layerManager.SetHistoryManager(&m_history);
}

bool CanvasController::NewCanvas(uint32_t width, uint32_t height, float dpi, bool transparent,
                                 LayerType initialLayerType,
                                 const Color& backgroundColor,
                                 const String& rgbProfilePath,
                                 const String& cmykProfilePath,
                                 const std::vector<uint8_t>& rgbProfileBytes,
                                 const std::vector<uint8_t>& cmykProfileBytes) {
    m_project = MakeRef<Project>(L"Untitled");
    m_project->GetCanvas().widthPx = width;
    m_project->GetCanvas().heightPx = height;
    m_project->GetCanvas().dpi = dpi;
    m_project->GetCanvas().transparent = transparent;
    if (!transparent && initialLayerType == LayerType::Transparent) {
        initialLayerType = LayerType::Color;
    }
    m_project->GetCanvas().initialLayerType = transparent ? LayerType::Transparent : initialLayerType;
    m_project->GetCanvas().rgbProfile = rgbProfilePath;
    m_project->GetCanvas().cmykProfile = cmykProfilePath;
    m_project->GetCanvas().rgbProfileBytes = rgbProfileBytes;
    m_project->GetCanvas().cmykProfileBytes = cmykProfileBytes;

    m_history.Clear();
    m_layerManager.Initialize(width, height);
    m_selection = MakeScope<SelectionMask>(width, height);
    auto layer = m_layerManager.AddLayer(L"Layer 1", m_project->GetCanvas().initialLayerType);
    if (layer && !transparent) {
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                layer->SetPixel(x, y, backgroundColor);
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
    const auto& canvas = m_project->GetCanvas();
    m_selection = MakeScope<SelectionMask>(canvas.widthPx, canvas.heightPx);
    EnsureCompositeBuffer();
    return true;
}

void CanvasController::CloseProject() {
    m_project.reset();
    m_layerManager.Shutdown();
    m_history.Clear();
    m_selection.reset();
    m_compositeBuffer.clear();
    m_drawing = false;
    m_panning = false;
    m_selecting = false;
    m_shapeDrawing = false;
    m_zoom = 1.0f;
    m_panX = 0.0f;
    m_panY = 0.0f;
}

void CanvasController::Render(Presentation::SoftRenderer& renderer, const Rect& viewport) {
    if (!m_project || viewport.Width() <= 0 || viewport.Height() <= 0) {
        return;
    }

    renderer.PushClip(viewport);
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
    if (!canvas.transparent || !m_showTransparentBackground) {
        renderer.FillRect(canvasRect, Color(255, 255, 255, 255));
    }
    renderer.BlitBgra(canvasRect, m_compositeBuffer.data(), canvas.widthPx, canvas.heightPx);
    renderer.StrokeRect(canvasRect, Color(20, 22, 26, 255), 2);

    if (m_showGrid) {
        const int step = std::max(8, static_cast<int>(64.0f * m_zoom));
        const int startX = canvasRect.left + static_cast<int>(std::fmod(std::max(0.0f, -m_panX), 64.0f) * m_zoom);
        const int startY = canvasRect.top + static_cast<int>(std::fmod(std::max(0.0f, -m_panY), 64.0f) * m_zoom);
        for (int x = startX; x < canvasRect.right; x += step) {
            if (x >= canvasRect.left) renderer.DrawLine(x, canvasRect.top, x, canvasRect.bottom, Color(0, 120, 215, 70), 1);
        }
        for (int y = startY; y < canvasRect.bottom; y += step) {
            if (y >= canvasRect.top) renderer.DrawLine(canvasRect.left, y, canvasRect.right, y, Color(0, 120, 215, 70), 1);
        }
    }
    if (m_showPixelGrid && m_zoom >= 8.0f) {
        const int step = static_cast<int>(m_zoom);
        for (int x = canvasRect.left; x < canvasRect.right; x += step) {
            renderer.DrawLine(x, canvasRect.top, x, canvasRect.bottom, Color(30, 34, 38, 90), 1);
        }
        for (int y = canvasRect.top; y < canvasRect.bottom; y += step) {
            renderer.DrawLine(canvasRect.left, y, canvasRect.right, y, Color(30, 34, 38, 90), 1);
        }
    }

    RenderSelection(renderer, viewport);

    if (m_selecting || m_shapeDrawing) {
        float canvasX = 0.0f;
        float canvasY = 0.0f;
        ScreenToCanvas(m_lastPointer, viewport, canvasX, canvasY);
        const float startX = m_selecting ? m_selectionStartX : m_shapeStartX;
        const float startY = m_selecting ? m_selectionStartY : m_shapeStartY;
        const int left = viewport.left + static_cast<int>(m_panX + std::min(startX, canvasX) * m_zoom);
        const int top = viewport.top + static_cast<int>(m_panY + std::min(startY, canvasY) * m_zoom);
        const int right = viewport.left + static_cast<int>(m_panX + std::max(startX, canvasX) * m_zoom);
        const int bottom = viewport.top + static_cast<int>(m_panY + std::max(startY, canvasY) * m_zoom);
        const Rect preview(left, top, right, bottom);
        renderer.StrokeRect(preview, m_selecting ? Color(245, 248, 250, 230) : Color(0, 160, 255, 230), 1);
        renderer.StrokeRect(Rect(preview.left + 1, preview.top + 1, preview.right - 1, preview.bottom - 1),
                            m_selecting ? Color(15, 17, 20, 220) : Color(230, 248, 255, 180), 1);
    }

    if (viewport.Contains(m_lastPointer)) {
        if (m_tool == ToolType::Brush || m_tool == ToolType::Eraser) {
            const int radius = std::max(4, static_cast<int>(m_brush.GetSize() * m_zoom * 0.5f));
            const Color cursor = m_tool == ToolType::Eraser ? Color(122, 210, 255, 210) : Color(250, 250, 250, 190);
            renderer.StrokeCircle(m_lastPointer.x, m_lastPointer.y, radius, cursor, 1);
            renderer.StrokeCircle(m_lastPointer.x, m_lastPointer.y, std::max(1, radius - 1), Color(25, 28, 32, 140), 1);
        } else {
            renderer.DrawLine(m_lastPointer.x - 7, m_lastPointer.y, m_lastPointer.x + 7, m_lastPointer.y, Color(245, 248, 250, 190), 1);
            renderer.DrawLine(m_lastPointer.x, m_lastPointer.y - 7, m_lastPointer.x, m_lastPointer.y + 7, Color(245, 248, 250, 190), 1);
        }
    }
    renderer.PopClip();
}

void CanvasController::ResizeViewport(const Rect& viewport) {
    m_lastViewport = viewport;
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
        if (m_tool == ToolType::Move) {
            m_panning = true;
            return;
        }
        if (m_tool == ToolType::Eyedropper) {
            if (auto layer = m_layerManager.GetActiveLayer()) {
                const auto x = static_cast<uint32_t>(std::clamp(canvasX, 0.0f, static_cast<float>(layer->GetCanvasWidth() - 1)));
                const auto y = static_cast<uint32_t>(std::clamp(canvasY, 0.0f, static_cast<float>(layer->GetCanvasHeight() - 1)));
                const Color picked = layer->GetPixel(x, y);
                if (picked.a > 0) SetColor(picked);
            }
            return;
        }
        if (m_tool == ToolType::Fill) {
            if (canvasX >= 0 && canvasY >= 0) {
                FloodFill(static_cast<uint32_t>(canvasX), static_cast<uint32_t>(canvasY), m_color);
            }
            return;
        }
        if (m_tool == ToolType::RectSelect) {
            m_selecting = true;
            m_selectionStartX = canvasX;
            m_selectionStartY = canvasY;
            return;
        }
        if (m_tool == ToolType::Shape) {
            m_shapeDrawing = true;
            m_shapeStartX = canvasX;
            m_shapeStartY = canvasY;
            return;
        }
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
        if (m_tool == ToolType::Move) {
            m_panning = false;
            return;
        }
        if (m_shapeDrawing) {
            FillShapeRect(m_shapeStartX, m_shapeStartY, canvasX, canvasY, m_color);
            m_shapeDrawing = false;
            return;
        }
        if (m_selecting) {
            if (m_selection && m_project) {
                const int left = std::max(0, static_cast<int>(std::floor(std::min(m_selectionStartX, canvasX))));
                const int top = std::max(0, static_cast<int>(std::floor(std::min(m_selectionStartY, canvasY))));
                const int right = std::min(static_cast<int>(m_project->GetCanvas().widthPx), static_cast<int>(std::ceil(std::max(m_selectionStartX, canvasX))));
                const int bottom = std::min(static_cast<int>(m_project->GetCanvas().heightPx), static_cast<int>(std::ceil(std::max(m_selectionStartY, canvasY))));
                m_selection->Clear();
                if (right > left && bottom > top) {
                    m_selection->FillRect(static_cast<uint32_t>(left), static_cast<uint32_t>(top),
                                          static_cast<uint32_t>(right - left), static_cast<uint32_t>(bottom - top));
                }
            }
            m_selecting = false;
            return;
        }
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

void CanvasController::SetBrushOpacity(float opacity) {
    m_brush.SetOpacity(opacity);
}

float CanvasController::GetBrushOpacity() const {
    return m_brush.GetOpacity();
}

void CanvasController::SetBrushFlow(float flow) {
    m_brush.SetFlow(flow);
}

float CanvasController::GetBrushFlow() const {
    return m_brush.GetFlow();
}

void CanvasController::SetBrushSpacing(float spacing) {
    m_brush.SetSpacing(spacing);
}

float CanvasController::GetBrushSpacing() const {
    return m_brush.GetSpacing();
}

void CanvasController::SetBrushWetMix(float wetMix) {
    m_brush.SetWetMix(wetMix);
}

float CanvasController::GetBrushWetMix() const {
    return m_brush.GetWetMix();
}

void CanvasController::SetBrushTip(Render::BrushTipType tip) {
    m_brush.SetTipType(tip);
}

Render::BrushTipType CanvasController::GetBrushTip() const {
    return m_brush.GetTipType();
}

void CanvasController::SetBrushParam(BrushParamId param, uint16_t value) {
    switch (param) {
        case BrushParamId::Size:
            SetBrushSize(static_cast<float>(std::max<uint16_t>(1, value)));
            break;
        case BrushParamId::Opacity:
            SetBrushOpacity(std::clamp(value / 100.0f, 0.0f, 1.0f));
            break;
        case BrushParamId::Flow:
            SetBrushFlow(std::clamp(value / 100.0f, 0.0f, 1.0f));
            break;
        case BrushParamId::Spacing:
            SetBrushSpacing(std::max(1u, static_cast<unsigned>(value)) / 100.0f);
            break;
        case BrushParamId::WetMix:
            SetBrushWetMix(std::clamp(value / 100.0f, 0.0f, 1.0f));
            break;
    }
}

void CanvasController::ApplyBrushPreset(uint16_t size, uint16_t tip) {
    SetBrushSize(static_cast<float>(std::max<uint16_t>(1, size)));
    SetBrushTip(static_cast<Render::BrushTipType>(tip));
    SetBrushOpacity(1.0f);
    SetBrushFlow(1.0f);
}

void CanvasController::SelectLayer(size_t index) {
    m_layerManager.SetActiveLayer(index);
}

void CanvasController::AddRasterLayer() {
    m_layerManager.AddLayer(L"Layer", LayerType::Transparent);
}

void CanvasController::AddTextLayer(const String& text, float fontSize) {
    auto layer = std::dynamic_pointer_cast<TextLayer>(m_layerManager.AddLayer(L"Text", LayerType::Text));
    if (!layer) return;
    layer->SetText(text.empty() ? L"CloverPic" : text);
    layer->SetTextColor(m_color);
    layer->SetFontSize(std::clamp(fontSize, 8.0f, 256.0f));
    layer->SetPosition(Point(80, 80));
    layer->RasterizeIfNeeded();
    m_layerManager.MarkCompositeDirty();
}

void CanvasController::DeleteActiveLayer() {
    if (m_layerManager.GetLayerCount() <= 1) return;
    m_layerManager.DeleteLayer(m_layerManager.GetActiveLayerIndex());
}

void CanvasController::DuplicateActiveLayer() {
    m_layerManager.DuplicateLayer(m_layerManager.GetActiveLayerIndex());
}

void CanvasController::MergeActiveLayerDown() {
    m_layerManager.MergeDown(m_layerManager.GetActiveLayerIndex());
}

void CanvasController::ToggleActiveLayerVisibility() {
    m_layerManager.ToggleLayerVisibility(m_layerManager.GetActiveLayerIndex());
}

void CanvasController::ToggleActiveLayerLock() {
    m_layerManager.ToggleLayerLock(m_layerManager.GetActiveLayerIndex());
}

void CanvasController::ToggleActiveLayerProtectAlpha() {
    auto layer = m_layerManager.GetActiveLayer();
    if (!layer) return;
    layer->SetProtectAlpha(!layer->IsProtectAlpha());
    layer->MarkDirty();
    m_layerManager.MarkCompositeDirty();
}

void CanvasController::ToggleActiveLayerSolo() {
    m_layerManager.ToggleSolo(m_layerManager.GetActiveLayerIndex());
}

void CanvasController::SetActiveLayerOpacity(uint8_t opacity) {
    auto layer = m_layerManager.GetActiveLayer();
    if (!layer) return;
    layer->SetOpacity(opacity);
    layer->MarkDirty();
    m_layerManager.MarkCompositeDirty();
}

void CanvasController::SetActiveLayerBlendMode(BlendMode mode) {
    auto layer = m_layerManager.GetActiveLayer();
    if (!layer) return;
    layer->SetBlendMode(mode);
    layer->MarkDirty();
    m_layerManager.MarkCompositeDirty();
}

void CanvasController::ApplyFilter(FilterCommandId filter, uint16_t value) {
    auto layer = m_layerManager.GetActiveLayer();
    if (!layer || layer->IsLocked()) return;
    switch (filter) {
        case FilterCommandId::Invert:
            Filters::ApplyInvert(layer.get());
            break;
        case FilterCommandId::Threshold:
            Filters::ApplyThreshold(layer.get(), static_cast<uint8_t>(value == 0 ? 128 : value));
            break;
        case FilterCommandId::BrightnessContrast:
            Filters::ApplyBrightnessContrast(layer.get(), 12, 12);
            break;
        case FilterCommandId::GaussianBlur:
            Filters::ApplyGaussianBlur(layer.get(), std::max<uint16_t>(1, value == 0 ? 2 : value));
            break;
        case FilterCommandId::Sharpen:
            Filters::ApplySharpen(layer.get(), std::max<uint16_t>(1, value == 0 ? 40 : value));
            break;
        case FilterCommandId::HueSaturation:
            Filters::ApplyHueSaturation(layer.get(), 18, 20);
            break;
    }
    layer->MarkDirty();
    m_layerManager.MarkCompositeDirty();
}

void CanvasController::ClearSelection() {
    if (m_selection) {
        m_selection->Clear();
    }
}

void CanvasController::InvertSelection() {
    if (m_selection) {
        m_selection->Invert();
    }
}

void CanvasController::SelectAll() {
    if (!m_selection || !m_project) return;
    const auto& canvas = m_project->GetCanvas();
    m_selection->Clear();
    m_selection->FillRect(0, 0, canvas.widthPx, canvas.heightPx);
}

void CanvasController::FlipActiveLayerHorizontal() {
    auto layer = m_layerManager.GetActiveLayer();
    if (!layer || layer->IsLocked()) return;
    const uint32_t width = layer->GetCanvasWidth();
    const uint32_t height = layer->GetCanvasHeight();
    std::vector<Color> pixels(static_cast<size_t>(width) * height);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            pixels[static_cast<size_t>(y) * width + x] = layer->GetPixel(width - 1 - x, y);
        }
    }
    layer->Clear();
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            layer->SetPixel(x, y, pixels[static_cast<size_t>(y) * width + x]);
        }
    }
    layer->MarkDirty();
    m_layerManager.MarkCompositeDirty();
}

void CanvasController::FlipActiveLayerVertical() {
    auto layer = m_layerManager.GetActiveLayer();
    if (!layer || layer->IsLocked()) return;
    const uint32_t width = layer->GetCanvasWidth();
    const uint32_t height = layer->GetCanvasHeight();
    std::vector<Color> pixels(static_cast<size_t>(width) * height);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            pixels[static_cast<size_t>(y) * width + x] = layer->GetPixel(x, height - 1 - y);
        }
    }
    layer->Clear();
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            layer->SetPixel(x, y, pixels[static_cast<size_t>(y) * width + x]);
        }
    }
    layer->MarkDirty();
    m_layerManager.MarkCompositeDirty();
}

void CanvasController::RotateActiveLayerRight() {
    auto layer = m_layerManager.GetActiveLayer();
    if (!layer || layer->IsLocked()) return;
    const uint32_t width = layer->GetCanvasWidth();
    const uint32_t height = layer->GetCanvasHeight();
    std::vector<Color> src(static_cast<size_t>(width) * height);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            src[static_cast<size_t>(y) * width + x] = layer->GetPixel(x, y);
        }
    }
    layer->Clear();
    const int cx = static_cast<int>(width / 2);
    const int cy = static_cast<int>(height / 2);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const int nx = cx + (static_cast<int>(y) - cy);
            const int ny = cy - (static_cast<int>(x) - cx);
            if (nx >= 0 && ny >= 0 && nx < static_cast<int>(width) && ny < static_cast<int>(height)) {
                layer->SetPixel(static_cast<uint32_t>(nx), static_cast<uint32_t>(ny), src[static_cast<size_t>(y) * width + x]);
            }
        }
    }
    layer->MarkDirty();
    m_layerManager.MarkCompositeDirty();
}

void CanvasController::RotateActiveLayerLeft() {
    auto layer = m_layerManager.GetActiveLayer();
    if (!layer || layer->IsLocked()) return;
    const uint32_t width = layer->GetCanvasWidth();
    const uint32_t height = layer->GetCanvasHeight();
    std::vector<Color> src(static_cast<size_t>(width) * height);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            src[static_cast<size_t>(y) * width + x] = layer->GetPixel(x, y);
        }
    }
    layer->Clear();
    const int cx = static_cast<int>(width / 2);
    const int cy = static_cast<int>(height / 2);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const int nx = cx - (static_cast<int>(y) - cy);
            const int ny = cy + (static_cast<int>(x) - cx);
            if (nx >= 0 && ny >= 0 && nx < static_cast<int>(width) && ny < static_cast<int>(height)) {
                layer->SetPixel(static_cast<uint32_t>(nx), static_cast<uint32_t>(ny), src[static_cast<size_t>(y) * width + x]);
            }
        }
    }
    layer->MarkDirty();
    m_layerManager.MarkCompositeDirty();
}

void CanvasController::SetZoom(float zoom) {
    if (!m_project) return;
    m_zoom = std::clamp(zoom, 0.05f, 16.0f);
}

void CanvasController::ZoomBy(float factor) {
    if (!m_project) return;
    SetZoom(m_zoom * std::clamp(factor, 0.1f, 10.0f));
}

void CanvasController::FitCanvas() {
    FitToViewport(m_lastViewport);
}

void CanvasController::ResetView() {
    m_zoom = 1.0f;
    m_panX = 40.0f;
    m_panY = 40.0f;
}

void CanvasController::ToggleViewOption(ViewOptionId option) {
    switch (option) {
        case ViewOptionId::Grid:
            m_showGrid = !m_showGrid;
            break;
        case ViewOptionId::PixelGrid:
            m_showPixelGrid = !m_showPixelGrid;
            break;
        case ViewOptionId::TransparentBackground:
            m_showTransparentBackground = !m_showTransparentBackground;
            break;
        case ViewOptionId::ColorProfileDisplay:
            m_showColorProfile = !m_showColorProfile;
            break;
    }
}

bool CanvasController::IsViewOptionEnabled(ViewOptionId option) const {
    switch (option) {
        case ViewOptionId::Grid: return m_showGrid;
        case ViewOptionId::PixelGrid: return m_showPixelGrid;
        case ViewOptionId::TransparentBackground: return m_showTransparentBackground;
        case ViewOptionId::ColorProfileDisplay: return m_showColorProfile;
    }
    return false;
}

void CanvasController::TogglePanel(WorkspacePanelId panel) {
    switch (panel) {
        case WorkspacePanelId::Color: m_panelColorVisible = !m_panelColorVisible; break;
        case WorkspacePanelId::BrushPreview: m_panelBrushPreviewVisible = !m_panelBrushPreviewVisible; break;
        case WorkspacePanelId::BrushControl: m_panelBrushControlVisible = !m_panelBrushControlVisible; break;
        case WorkspacePanelId::BrushPresets: m_panelBrushPresetsVisible = !m_panelBrushPresetsVisible; break;
        case WorkspacePanelId::Navigator: m_panelNavigatorVisible = !m_panelNavigatorVisible; break;
        case WorkspacePanelId::Layer: m_panelLayerVisible = !m_panelLayerVisible; break;
        case WorkspacePanelId::BrushSize: m_panelBrushSizeVisible = !m_panelBrushSizeVisible; break;
        case WorkspacePanelId::StatusBar: m_panelStatusBarVisible = !m_panelStatusBarVisible; break;
    }
}

bool CanvasController::IsPanelVisible(WorkspacePanelId panel) const {
    switch (panel) {
        case WorkspacePanelId::Color: return m_panelColorVisible;
        case WorkspacePanelId::BrushPreview: return m_panelBrushPreviewVisible;
        case WorkspacePanelId::BrushControl: return m_panelBrushControlVisible;
        case WorkspacePanelId::BrushPresets: return m_panelBrushPresetsVisible;
        case WorkspacePanelId::Navigator: return m_panelNavigatorVisible;
        case WorkspacePanelId::Layer: return m_panelLayerVisible;
        case WorkspacePanelId::BrushSize: return m_panelBrushSizeVisible;
        case WorkspacePanelId::StatusBar: return m_panelStatusBarVisible;
    }
    return true;
}

void CanvasController::InitializeWorkspaceLayout() {
    m_panelColorVisible = true;
    m_panelBrushPreviewVisible = true;
    m_panelBrushControlVisible = true;
    m_panelBrushPresetsVisible = true;
    m_panelNavigatorVisible = true;
    m_panelLayerVisible = true;
    m_panelBrushSizeVisible = true;
    m_panelStatusBarVisible = true;
    m_showGrid = false;
    m_showPixelGrid = true;
    m_showTransparentBackground = true;
    m_showColorProfile = false;
    m_snapMode = SnapModeId::Off;
}

void CanvasController::ScreenToCanvas(const Point& position, const Rect& viewport, float& canvasX, float& canvasY) const {
    canvasX = (position.x - viewport.left - m_panX) / std::max(0.001f, m_zoom);
    canvasY = (position.y - viewport.top - m_panY) / std::max(0.001f, m_zoom);
}

const SelectionMask* CanvasController::ActiveSelectionMask() const {
    return (m_selection && !m_selection->IsEmpty()) ? m_selection.get() : nullptr;
}

void CanvasController::RenderSelection(Presentation::SoftRenderer& renderer, const Rect& viewport) const {
    if (!m_selection || m_selection->IsEmpty()) return;
    std::vector<std::pair<int, int>> boundary;
    const int step = std::max(1, static_cast<int>(2.0f / std::max(0.2f, m_zoom)));
    m_selection->GetBoundaryPixels(boundary, step);
    for (const auto& [x, y] : boundary) {
        const int sx = viewport.left + static_cast<int>(m_panX + x * m_zoom);
        const int sy = viewport.top + static_cast<int>(m_panY + y * m_zoom);
        const bool dash = ((x + y) / std::max(1, step * 4)) % 2 == 0;
        renderer.FillRect(Rect(sx, sy, sx + std::max(1, static_cast<int>(m_zoom)), sy + std::max(1, static_cast<int>(m_zoom))),
                          dash ? Color(255, 255, 255, 210) : Color(20, 20, 20, 210));
    }
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
                              brush.GetTipType(), brush.GetFlow(), brush.GetWetMix(), ActiveSelectionMask());
    }
    m_layerManager.MarkCompositeDirty();
}

void CanvasController::FloodFill(uint32_t x, uint32_t y, const Color& color) {
    auto layer = m_layerManager.GetActiveLayer();
    if (!layer || layer->IsLocked() || x >= layer->GetCanvasWidth() || y >= layer->GetCanvasHeight()) return;
    const Color target = layer->GetPixel(x, y);
    if (target.r == color.r && target.g == color.g && target.b == color.b && target.a == color.a) return;

    const uint32_t width = layer->GetCanvasWidth();
    const uint32_t height = layer->GetCanvasHeight();
    std::vector<uint8_t> visited(static_cast<size_t>(width) * height, 0);
    std::vector<Point> stack;
    stack.push_back(Point(static_cast<int32_t>(x), static_cast<int32_t>(y)));

    auto matches = [&](uint32_t px, uint32_t py) {
        const Color p = layer->GetPixel(px, py);
        return p.r == target.r && p.g == target.g && p.b == target.b && p.a == target.a;
    };
    const SelectionMask* selection = ActiveSelectionMask();

    while (!stack.empty()) {
        const Point p = stack.back();
        stack.pop_back();
        if (p.x < 0 || p.y < 0 || p.x >= static_cast<int32_t>(width) || p.y >= static_cast<int32_t>(height)) continue;
        const auto idx = static_cast<size_t>(p.y) * width + static_cast<size_t>(p.x);
        if (visited[idx]) continue;
        visited[idx] = 1;
        const auto px = static_cast<uint32_t>(p.x);
        const auto py = static_cast<uint32_t>(p.y);
        if (selection && selection->GetPixel(px, py) == 0) continue;
        if (!matches(px, py)) continue;
        layer->SetPixel(px, py, color);
        stack.push_back(Point(p.x + 1, p.y));
        stack.push_back(Point(p.x - 1, p.y));
        stack.push_back(Point(p.x, p.y + 1));
        stack.push_back(Point(p.x, p.y - 1));
    }
    layer->MarkDirty();
    m_layerManager.MarkCompositeDirty();
}

void CanvasController::FillShapeRect(float x1, float y1, float x2, float y2, const Color& color) {
    auto layer = m_layerManager.GetActiveLayer();
    if (!layer || layer->IsLocked()) return;
    const int left = std::max(0, static_cast<int>(std::floor(std::min(x1, x2))));
    const int top = std::max(0, static_cast<int>(std::floor(std::min(y1, y2))));
    const int right = std::min(static_cast<int>(layer->GetCanvasWidth()), static_cast<int>(std::ceil(std::max(x1, x2))));
    const int bottom = std::min(static_cast<int>(layer->GetCanvasHeight()), static_cast<int>(std::ceil(std::max(y1, y2))));
    for (int py = top; py < bottom; ++py) {
        for (int px = left; px < right; ++px) {
            if (ActiveSelectionMask() && ActiveSelectionMask()->GetPixel(static_cast<uint32_t>(px), static_cast<uint32_t>(py)) == 0) {
                continue;
            }
            layer->SetPixel(static_cast<uint32_t>(px), static_cast<uint32_t>(py), color);
        }
    }
    layer->MarkDirty();
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
