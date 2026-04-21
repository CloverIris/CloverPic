#pragma once

#include "UI/Core/Window.h"
#include "UI/Core/Theme.h"

namespace VividPic {
namespace UI {

class Panel : public Window {
public:
    Panel();
    
    void SetBackgroundColor(uint32_t color) { m_bgColor = color; }
    uint32_t GetBackgroundColor() const { return m_bgColor; }
    
    void SetTitle(const String& title) { m_title = title; }
    String GetTitle() const { return m_title; }
    
protected:
    void OnPaint(HDC hdc, const Rect& clip) override;
    DWORD GetDefaultStyle() const override { return WS_CHILD | WS_VISIBLE; }
    
private:
    uint32_t m_bgColor = Theme::PanelBackground;
    String m_title;
};

} // namespace UI
} // namespace VividPic
