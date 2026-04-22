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
    void SetOnWetMixChanged(std::function<void(float)> callback) { m_onWetMixChanged = callback; }
    void SetOnTipTypeChanged(std::function<void(Render::BrushTipType)> callback) { m_onTipTypeChanged = callback; }
    void SetOnPresetSelected(std::function<void(int)> callback) { m_onPresetSelected = callback; }

    void Refresh() { Invalidate(); }

    // External sync
    void SetBrushSize(float size) { m_brushSize = size; Invalidate(); }
    void SetBrushOpacity(float opacity) { m_opacity = opacity; Invalidate(); }
    void SetBrushFlow(float flow) { m_flow = flow; Invalidate(); }
    void SetBrushSpacing(float spacing) { m_spacing = spacing; Invalidate(); }
    void SetBrushWetMix(float wetMix) { m_wetMix = wetMix; Invalidate(); }
    void SetTipType(Render::BrushTipType type) { m_tipType = type; Invalidate(); }
    void SetActivePreset(int index) { m_activePreset = index; Invalidate(); }

protected:
    void OnPaint(HDC hdc, const Rect& clip) override;
    void OnMouseDown(const Point& pos, MouseButton button) override;
    void OnMouseMove(const Point& pos) override;
    void OnMouseUp(const Point& pos, MouseButton button) override;
    void OnMouseLeave() override;

    DWORD GetDefaultStyle() const override { return WS_CHILD | WS_VISIBLE; }

private:
    // Layout metrics (base values, scaled by Theme::Scale at runtime)
    static constexpr int Pad = 8;
    static constexpr int TitleH = 22;
    static constexpr int SepMargin = 4;
    static constexpr int PreviewSize = 48;
    static constexpr int StrokePreviewW = 96;
    static constexpr int StrokePreviewH = 24;
    static constexpr int TipBtnW = 28;
    static constexpr int TipBtnH = 20;
    static constexpr int SliderH = 32;
    static constexpr int PresetBtnH = 22;

    struct SliderInfo {
        const wchar_t* label;
        float* value;
        float max;
        bool* dragging;
        std::function<void()> onChanged;
        std::wstring FormatValue() const;
    };

    void DrawPanelHeader(HDC hdc, const Rect& client);
    void DrawBrushPreview(HDC hdc, int x, int y);
    void DrawStrokePreview(HDC hdc, int x, int y);
    void DrawTipButtons(HDC hdc, int x, int y, int width);
    void DrawSliders(HDC hdc, int x, int y, int width);
    void DrawPresetButtons(HDC hdc, int x, int y, int width);

    void DrawBrushTipShape(HDC hdc, const Rect& rc, Render::BrushTipType type, float size);
    void DrawBrushStroke(HDC hdc, const Rect& rc, Render::BrushTipType type, float size, float opacity);

    int HitTestTipButton(const Point& pos) const;
    int HitTestPresetButton(const Point& pos) const;
    int HitTestSlider(const Point& pos) const;

    float m_brushSize = 10.0f;
    float m_opacity = 1.0f;
    float m_flow = 1.0f;
    float m_spacing = 0.25f;
    float m_wetMix = 0.0f;
    Render::BrushTipType m_tipType = Render::BrushTipType::RoundHard;
    int m_activePreset = -1;

    bool m_draggingSize = false;
    bool m_draggingOpacity = false;
    bool m_draggingFlow = false;
    bool m_draggingSpacing = false;
    bool m_draggingWetMix = false;

    int m_hoverTipIndex = -1;
    int m_hoverPresetIndex = -1;
    int m_hoverSliderIndex = -1;

    std::function<void(float)> m_onSizeChanged;
    std::function<void(float)> m_onOpacityChanged;
    std::function<void(float)> m_onFlowChanged;
    std::function<void(float)> m_onSpacingChanged;
    std::function<void(float)> m_onWetMixChanged;
    std::function<void(Render::BrushTipType)> m_onTipTypeChanged;
    std::function<void(int)> m_onPresetSelected;
};

} // namespace UI
} // namespace VividPic
