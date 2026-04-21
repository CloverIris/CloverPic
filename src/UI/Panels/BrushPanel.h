#pragma once

#include "UI/Core/Window.h"
#include "Render/BrushPreset.h"

namespace VividPic {
namespace UI {

class BrushPanel : public Window {
public:
    BrushPanel();

    void SetOnSizeChanged(std::function<void(float)> callback) { m_onSizeChanged = callback; }
    void SetOnOpacityChanged(std::function<void(float)> callback) { m_onOpacityChanged = callback; }
    void SetOnFlowChanged(std::function<void(float)> callback) { m_onFlowChanged = callback; }
    void SetOnSpacingChanged(std::function<void(float)> callback) { m_onSpacingChanged = callback; }
    void SetOnTipTypeChanged(std::function<void(Render::BrushTipType)> callback) { m_onTipTypeChanged = callback; }
    void SetOnPresetSelected(std::function<void(int)> callback) { m_onPresetSelected = callback; }

    void Refresh() { Invalidate(); }

    // External sync
    void SetBrushSize(float size) { m_brushSize = size; Invalidate(); }
    void SetBrushOpacity(float opacity) { m_opacity = opacity; Invalidate(); }
    void SetBrushFlow(float flow) { m_flow = flow; Invalidate(); }
    void SetBrushSpacing(float spacing) { m_spacing = spacing; Invalidate(); }
    void SetTipType(Render::BrushTipType type) { m_tipType = type; Invalidate(); }

protected:
    void OnPaint(HDC hdc, const Rect& clip) override;
    void OnMouseDown(const Point& pos, MouseButton button) override;
    void OnMouseMove(const Point& pos) override;
    void OnMouseUp(const Point& pos, MouseButton button) override;

    DWORD GetDefaultStyle() const override { return WS_CHILD | WS_VISIBLE; }

private:
    void DrawSlider(HDC hdc, int x, int y, int width, float value, const String& label, const String& valueText);
    void DrawTipButtons(HDC hdc, int x, int y);
    void DrawPresetButtons(HDC hdc, int x, int y, int width);

    int HitTestSlider(const Point& pos, int& outSliderIndex) const;
    int HitTestTipButton(const Point& pos) const;
    int HitTestPresetButton(const Point& pos) const;

    float m_brushSize = 10.0f;
    float m_opacity = 1.0f;
    float m_flow = 1.0f;
    float m_spacing = 0.25f;
    Render::BrushTipType m_tipType = Render::BrushTipType::RoundHard;

    bool m_draggingSize = false;
    bool m_draggingOpacity = false;
    bool m_draggingFlow = false;
    bool m_draggingSpacing = false;

    static constexpr int SliderWidth = 180;
    static constexpr int SliderHeight = 14;

    std::function<void(float)> m_onSizeChanged;
    std::function<void(float)> m_onOpacityChanged;
    std::function<void(float)> m_onFlowChanged;
    std::function<void(float)> m_onSpacingChanged;
    std::function<void(Render::BrushTipType)> m_onTipTypeChanged;
    std::function<void(int)> m_onPresetSelected;
};

} // namespace UI
} // namespace VividPic
