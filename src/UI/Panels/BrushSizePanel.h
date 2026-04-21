#pragma once

#include "UI/Core/Window.h"
#include <functional>

namespace VividPic {
namespace UI {

class BrushSizePanel : public Window {
public:
    BrushSizePanel();
    
    void SetCurrentSize(float size);
    void SetOnSizeChanged(std::function<void(float)> callback) { m_onSizeChanged = std::move(callback); }
    void Refresh();
    
protected:
    void OnPaint(HDC hdc, const Rect& clip) override;
    void OnMouseDown(const Point& pos, MouseButton button) override;
    void OnMouseMove(const Point& pos) override;
    void OnMouseLeave() override;
    
    DWORD GetDefaultStyle() const override { return WS_CHILD | WS_VISIBLE; }
    
private:
    static constexpr int PresetCount = 24;
    static const float Presets[PresetCount];
    static constexpr int ButtonSize = 28;
    static constexpr int ButtonsPerRow = 8;
    
    float m_currentSize = 8.0f;
    int m_hoverIndex = -1;
    std::function<void(float)> m_onSizeChanged;
    
    int HitTest(const Point& pos) const;
    void DrawButton(HDC hdc, int index, const Rect& rc, bool active, bool hovered);
};

} // namespace UI
} // namespace VividPic
