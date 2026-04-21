#pragma once

#include "UI/Core/Window.h"
#include "UI/Core/Theme.h"

namespace VividPic {
namespace UI {

class EditBox : public Window {
public:
    EditBox();
    
    void SetText(const String& text);
    String GetText() const { return m_text; }
    
    void SetNumericOnly(bool numeric) { m_numericOnly = numeric; }
    void SetReadOnly(bool readOnly) { m_readOnly = readOnly; }
    
    void SetOnChanged(Callback callback) { m_onChanged = callback; }
    
protected:
    void OnPaint(HDC hdc, const Rect& clip) override;
    void OnMouseDown(const Point& pos, MouseButton button) override;
    void OnKeyDown(uint32_t keyCode) override;
    void OnKeyUp(uint32_t keyCode) override;
    void OnChar(wchar_t ch) override;
    bool OnCreate() override;
    
    DWORD GetDefaultStyle() const override { return WS_CHILD | WS_VISIBLE; }
    
private:
    String m_text;
    bool m_focused = false;
    bool m_numericOnly = false;
    bool m_readOnly = false;
    Callback m_onChanged;
    
    void InsertChar(wchar_t ch);
    void DeleteChar();
    void NotifyChanged();
};

} // namespace UI
} // namespace VividPic
