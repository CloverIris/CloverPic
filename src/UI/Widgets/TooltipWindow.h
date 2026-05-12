#pragma once

#include "UI/Core/Window.h"
#include "UI/Core/Theme.h"

namespace VividPic {
namespace UI {

class TooltipWindow : public Window {
public:
    TooltipWindow();
    
    void ShowAt(int screenX, int screenY, const wchar_t* text);
    void Hide();
    bool IsShowing() const { return m_showing; }
    
protected:
    void OnPaint(HDC hdc, const Rect& clip) override;
    
    DWORD GetDefaultStyle() const override { return WS_POPUP; }
    DWORD GetDefaultExStyle() const override { return WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST; }
    
private:
    String m_text;
    bool m_showing = false;
};

} // namespace UI
} // namespace VividPic
