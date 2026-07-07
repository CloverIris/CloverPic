#pragma once

#include "Core/Input/InputEvents.h"
#include "Core/Editing/History.h"
#include "Core/Layers/LayerManager.h"
#include "Core/Layers/BlendMode.h"
#include "Core/Document/Project.h"
#include "Core/Render/BrushEngine.h"
#include "Core/App/AppCommandPayload.h"
#include "Core/Layers/SelectionMask.h"
#include "Core/Presentation/SoftRenderer.h"
#include "Core/App/ToolType.h"

namespace CloverPic::Core {

class CanvasController {
public:
    CanvasController();

    bool NewCanvas(uint32_t width, uint32_t height, float dpi, bool transparent,
                   LayerType initialLayerType = LayerType::Transparent,
                   const Color& backgroundColor = Color(255, 255, 255, 255),
                   const String& rgbProfilePath = L"",
                   const String& cmykProfilePath = L"",
                   const std::vector<uint8_t>& rgbProfileBytes = {},
                   const std::vector<uint8_t>& cmykProfileBytes = {});
    bool AttachLoadedProject(Ref<Project> project);
    bool ResizeCanvas(uint32_t width, uint32_t height, uint32_t anchorX = 1, uint32_t anchorY = 1);
    void CloseProject();

    void Render(Presentation::SoftRenderer& renderer, const Rect& viewport);
    void ResizeViewport(const Rect& viewport);
    bool HandlePointer(const Input::PointerEvent& event, const Rect& viewport);
    void HandleWheel(int delta, const Point& position, const Rect& viewport);

    void SetTool(ToolType tool) { m_tool = tool; }
    ToolType GetTool() const { return m_tool; }
    void SetColor(const Color& color);
    Color GetColor() const { return m_color; }
    void SetBrushSize(float size);
    float GetBrushSize() const;
    void SetBrushOpacity(float opacity);
    float GetBrushOpacity() const;
    void SetBrushFlow(float flow);
    float GetBrushFlow() const;
    void SetBrushSpacing(float spacing);
    float GetBrushSpacing() const;
    void SetBrushWetMix(float wetMix);
    float GetBrushWetMix() const;
    void SetBrushTip(Render::BrushTipType tip);
    Render::BrushTipType GetBrushTip() const;
    void SetBrushParam(BrushParamId param, uint16_t value);
    void ApplyBrushPreset(uint16_t size, uint16_t tip);
    void SelectLayer(size_t index);
    void AddRasterLayer();
    void AddTextLayer(const String& text, float fontSize = 36.0f);
    void DeleteActiveLayer();
    void DuplicateActiveLayer();
    void MergeActiveLayerDown();
    void ToggleActiveLayerVisibility();
    void ToggleActiveLayerLock();
    void ToggleActiveLayerProtectAlpha();
    void ToggleActiveLayerSolo();
    void SetActiveLayerOpacity(uint8_t opacity);
    void SetActiveLayerBlendMode(BlendMode mode);
    void ApplyFilter(FilterCommandId filter, uint16_t value);
    void ClearSelection();
    void InvertSelection();
    void SelectAll();
    void FlipActiveLayerHorizontal();
    void FlipActiveLayerVertical();
    void RotateActiveLayerLeft();
    void RotateActiveLayerRight();
    void SetZoom(float zoom);
    void FitCanvas();
    void ResetView();
    void ZoomBy(float factor);
    void ToggleViewOption(ViewOptionId option);
    bool IsViewOptionEnabled(ViewOptionId option) const;
    void TogglePanel(WorkspacePanelId panel);
    void SetPanelVisible(WorkspacePanelId panel, bool visible);
    bool IsPanelVisible(WorkspacePanelId panel) const;
    void ToggleLeftSidebar() { m_leftSidebarExpanded = !m_leftSidebarExpanded; }
    void ToggleRightSidebar() { m_rightSidebarExpanded = !m_rightSidebarExpanded; }
    void SetLeftSidebarExpanded(bool expanded) { m_leftSidebarExpanded = expanded; }
    void SetRightSidebarExpanded(bool expanded) { m_rightSidebarExpanded = expanded; }
    bool IsLeftSidebarExpanded() const { return m_leftSidebarExpanded; }
    bool IsRightSidebarExpanded() const { return m_rightSidebarExpanded; }
    void InitializeWorkspaceLayout();
    void SetSnapMode(SnapModeId mode) { m_snapMode = mode; }
    SnapModeId GetSnapMode() const { return m_snapMode; }

    Ref<Project> GetProject() const { return m_project; }
    bool HasProject() const { return static_cast<bool>(m_project); }
    LayerManager* GetLayerManager() { return &m_layerManager; }
    const LayerManager* GetLayerManager() const { return &m_layerManager; }
    HistoryManager* GetHistoryManager() { return &m_history; }
    float GetZoom() const { return m_zoom; }
    Point GetPan() const { return Point(static_cast<int32_t>(m_panX), static_cast<int32_t>(m_panY)); }
    bool IsInteracting() const { return m_drawing || m_panning || m_selecting || m_shapeDrawing; }

private:
    void ScreenToCanvas(const Point& position, const Rect& viewport, float& canvasX, float& canvasY) const;
    void ApplySnap(float anchorX, float anchorY, float& x, float& y) const;
    void SnapPointToMode(float& x, float& y) const;
    const SelectionMask* ActiveSelectionMask() const;
    void RenderSelection(Presentation::SoftRenderer& renderer, const Rect& viewport) const;
    void RenderSnapGuide(Presentation::SoftRenderer& renderer, const Rect& viewport) const;
    void ApplyBrush(float x, float y, float pressure);
    void FloodFill(uint32_t x, uint32_t y, const Color& color);
    void FillShapeRect(float x1, float y1, float x2, float y2, const Color& color);
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
    Rect m_lastViewport;
    bool m_drawing = false;
    bool m_panning = false;
    Point m_lastPointer;
    float m_lastCanvasX = 0.0f;
    float m_lastCanvasY = 0.0f;
    float m_lastPressure = 1.0f;
    bool m_shapeDrawing = false;
    float m_shapeStartX = 0.0f;
    float m_shapeStartY = 0.0f;
    Scope<SelectionMask> m_selection;
    bool m_selecting = false;
    float m_selectionStartX = 0.0f;
    float m_selectionStartY = 0.0f;
    bool m_snapGuideActive = false;
    float m_snapGuideAnchorX = 0.0f;
    float m_snapGuideAnchorY = 0.0f;
    float m_snapGuideTargetX = 0.0f;
    float m_snapGuideTargetY = 0.0f;
    bool m_showGrid = false;
    bool m_showPixelGrid = true;
    bool m_showTransparentBackground = true;
    bool m_showColorProfile = false;
    bool m_panelColorVisible = true;
    bool m_panelBrushPreviewVisible = true;
    bool m_panelBrushControlVisible = true;
    bool m_panelBrushPresetsVisible = true;
    bool m_panelNavigatorVisible = true;
    bool m_panelLayerVisible = true;
    bool m_panelBrushSizeVisible = true;
    bool m_panelStatusBarVisible = true;
    bool m_leftSidebarExpanded = true;
    bool m_rightSidebarExpanded = true;
    SnapModeId m_snapMode = SnapModeId::Off;
};

} // namespace CloverPic::Core
