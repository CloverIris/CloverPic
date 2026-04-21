#pragma once

#include "UI/Core/Window.h"
#include "UI/Core/Theme.h"

namespace VividPic {
namespace UI {

class Button : public Window {
public:
    Button();
    
    void SetText(const String& text);
    String GetText() const { return m_text; }
    
    void SetColorTag(uint32_t color) { m_colorTag = color; m_hasColorTag = true; }
    void ClearColorTag() { m_hasColorTag = false; }
    
    void SetCallback(Callback callback) { m_callback = callback; }
    
    // Size presets (for brush list)
    void SetSizePreset(int preset) { m_sizePreset = preset; m_hasSizePreset = true; }
    
    void SetHoverColor(uint32_t color) { m_hoverColor = color; }
    void SetPressedColor(uint32_t color) { m_pressedColor = color; }
    void SetDefaultColor(uint32_t color) { m_defaultColor = color; }
    
protected:
    void OnPaint(HDC hdc, const Rect& clip) override;
    void OnMouseDown(const Point& pos, MouseButton button) override;
    void OnMouseUp(const Point& pos, MouseButton button) override;
    void OnMouseEnter() override;
    void OnMouseLeave() override;
    
    DWORD GetDefaultStyle() const override { return WS_CHILD | WS_VISIBLE; }
    
private:
    String m_text;
    Callback m_callback;
    bool m_hovered = false;
    bool m_pressed = false;
    
    uint32_t m_defaultColor = Theme::ButtonDefault;
    uint32_t m_hoverColor = Theme::ButtonHover;
    uint32_t m_pressedColor = Theme::ButtonPressed;
    
    // For brush list display
    bool m_hasColorTag = false;
    uint32_t m_colorTag = 0;
    bool m_hasSizePreset = false;
    int m_sizePreset = 0;
};

} // namespace UI
} // namespace VividPic
