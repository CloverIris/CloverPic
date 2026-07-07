#pragma once

#include "Core/App/CanvasController.h"

namespace CloverPic::Core {

class WorkspaceEditorFacade {
public:
    WorkspaceEditorFacade() = default;
    explicit WorkspaceEditorFacade(CanvasController& canvas) : m_canvas(&canvas) {}

    void Bind(CanvasController& canvas) { m_canvas = &canvas; }

    bool HasProject() const { return m_canvas && m_canvas->HasProject(); }
    Ref<Project> GetProject() const { return m_canvas ? m_canvas->GetProject() : nullptr; }
    LayerManager* GetLayerManager() { return m_canvas ? m_canvas->GetLayerManager() : nullptr; }
    const LayerManager* GetLayerManager() const { return m_canvas ? m_canvas->GetLayerManager() : nullptr; }

    ToolType GetTool() const { return m_canvas ? m_canvas->GetTool() : ToolType::Brush; }
    Color GetColor() const { return m_canvas ? m_canvas->GetColor() : Color::FromHex(0x111111); }
    void SetColor(const Color& color) { if (m_canvas) m_canvas->SetColor(color); }

    float GetBrushSize() const { return m_canvas ? m_canvas->GetBrushSize() : 1.0f; }
    void SetBrushSize(float size) { if (m_canvas) m_canvas->SetBrushSize(size); }
    float GetBrushOpacity() const { return m_canvas ? m_canvas->GetBrushOpacity() : 1.0f; }
    float GetBrushFlow() const { return m_canvas ? m_canvas->GetBrushFlow() : 1.0f; }
    float GetBrushSpacing() const { return m_canvas ? m_canvas->GetBrushSpacing() : 0.25f; }
    float GetBrushWetMix() const { return m_canvas ? m_canvas->GetBrushWetMix() : 0.0f; }
    Render::BrushTipType GetBrushTip() const { return m_canvas ? m_canvas->GetBrushTip() : Render::BrushTipType::RoundHard; }

    void SetBrushParam(BrushParamId param, uint16_t value) { if (m_canvas) m_canvas->SetBrushParam(param, value); }
    void SetActiveLayerOpacity(uint8_t opacity) { if (m_canvas) m_canvas->SetActiveLayerOpacity(opacity); }

    SnapModeId GetSnapMode() const { return m_canvas ? m_canvas->GetSnapMode() : SnapModeId::Off; }
    bool IsViewOptionEnabled(ViewOptionId option) const { return m_canvas && m_canvas->IsViewOptionEnabled(option); }

    bool HandleCanvasPointer(const Input::PointerEvent& event, const Rect& viewport) {
        return m_canvas && m_canvas->HandlePointer(event, viewport);
    }
    void HandleCanvasWheel(int delta, const Point& position, const Rect& viewport) {
        if (m_canvas) m_canvas->HandleWheel(delta, position, viewport);
    }
    bool IsInteracting() const { return m_canvas && m_canvas->IsInteracting(); }

private:
    CanvasController* m_canvas = nullptr;
};

} // namespace CloverPic::Core
