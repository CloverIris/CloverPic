#pragma once

#include "UI/Core/Window.h"

namespace VividPic {
namespace UI {

// -------------------------------------------------------------------------
// ToastWindow — ephemeral operation feedback overlay
// Auto-dismisses after a timeout. No activate, no focus.
// -------------------------------------------------------------------------
class ToastWindow : public Window {
public:
    ToastWindow();
    
    void Show(const wchar_t* text, int durationMs = 2000);
    void Hide();
    
    void SetText(const wchar_t* text);

protected:
    void OnPaint(HDC hdc, const Rect& clip) override;
    DWORD GetDefaultStyle() const override { return WS_POPUP; }
    DWORD GetDefaultExStyle() const override { return WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_LAYERED; }
    
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) override;

private:
    String m_text;
    static constexpr uint32_t TIMER_HIDE = 3001;
};

} // namespace UI
} // namespace VividPic
