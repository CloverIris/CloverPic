#pragma once

#include "UI/Core/Window.h"

namespace VividPic {
namespace UI {

class BrushPanel : public Window {
public:
    BrushPanel();
    
    void SetOnSizeChanged(std::function<void(float)> callback) { m_onSizeChanged = callback; }
    void SetOnOpacityChanged(std::function<void(float)> callback) { m_onOpacityChanged = callback; }
    
    void Refresh() { Invalidate(); }
    
protected:
    void OnPaint(HDC hdc, const Rect& clip) override;
    void OnMouseDown(const Point& pos, MouseButton button) override;
    void OnMouseMove(const Point& pos) override;
    void OnMouseUp(const Point& pos, MouseButton button) override;
    
    DWORD GetDefaultStyle() const override { return WS_CHILD | WS_VISIBLE; }
    
private:
    void DrawSlider(HDC hdc, int x, int y, int width, float value, const String& label);
    int HitTestSlider(const Point& pos, int& outY) const;
    
    float m_brushSize = 10.0f;
    float m_opacity = 1.0f;
    
    bool m_draggingSize = false;
    bool m_draggingOpacity = false;
    
    static constexpr int SliderWidth = 180;
    static constexpr int SliderHeight = 16;
    
    std::function<void(float)> m_onSizeChanged;
    std::function<void(float)> m_onOpacityChanged;
};

} // namespace UI
} // namespace VividPic
