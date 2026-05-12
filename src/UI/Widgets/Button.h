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
    
    // Icon support (Segoe MDL2 Assets character)
    void SetIconChar(wchar_t icon) { m_iconChar = icon; m_hasIcon = true; }
    void ClearIcon() { m_hasIcon = false; }
    
    // Size presets (for brush list)
    void SetSizePreset(int preset) { m_sizePreset = preset; m_hasSizePreset = true; }
    
    void SetHoverColor(uint32_t color) { m_hoverColor = color; }
    void SetPressedColor(uint32_t color) { m_pressedColor = color; }
    void SetDefaultColor(uint32_t color) { m_defaultColor = color; }
    
    void SetDisabled(bool disabled) { m_disabled = disabled; Invalidate(); }
    bool IsDisabled() const { return m_disabled; }
    
protected:
    void OnPaint(HDC hdc, const Rect& clip) override;
    void OnMouseDown(const Point& pos, MouseButton button) override;
    void OnMouseUp(const Point& pos, MouseButton button) override;
    void OnMouseEnter() override;
    void OnMouseLeave() override;
    void OnKeyDown(uint32_t keyCode) override;
    void OnKeyUp(uint32_t keyCode) override;
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) override;
    
    DWORD GetDefaultStyle() const override { return WS_CHILD | WS_VISIBLE; }
    
private:
    String m_text;
    Callback m_callback;
    bool m_hovered = false;
    bool m_pressed = false;
    bool m_disabled = false;
    bool m_focused = false;
    
    uint32_t m_defaultColor = Theme::ButtonDefault;
    uint32_t m_hoverColor = Theme::ButtonHover;
    uint32_t m_pressedColor = Theme::ButtonPressed;
    
    // Icon
    bool m_hasIcon = false;
    wchar_t m_iconChar = L'\0';
    
    // For brush list display
    bool m_hasColorTag = false;
    uint32_t m_colorTag = 0;
    bool m_hasSizePreset = false;
    int m_sizePreset = 0;
};

} // namespace UI
} // namespace VividPic
