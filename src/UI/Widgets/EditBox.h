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
    void OnDestroy() override;
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) override;
    
    DWORD GetDefaultStyle() const override { return WS_CHILD | WS_VISIBLE; }
    
private:
    String m_text;
    bool m_focused = false;
    bool m_numericOnly = false;
    bool m_readOnly = false;
    Callback m_onChanged;
    
    // Cursor & selection
    int m_cursorPos = 0;      // logical insertion point (0..length)
    int m_selAnchor = 0;      // selection anchor (moves only on non-extended navigation)
    
    void InsertChar(wchar_t ch);
    void DeleteChar();            // backspace (left)
    void DeleteCharForward();     // delete (right)
    void NotifyChanged();
    
    void UpdateCaret();
    void SetCursorPos(int pos, bool extendSelection);
    void ClearSelection();
    bool HasSelection() const;
    int SelectionStart() const;
    int SelectionEnd() const;
    void DeleteSelection();
    
    void CopyToClipboard();
    void PasteFromClipboard();
    void CutToClipboard();
    
    int HitTestPos(int mouseX) const;
    int GetTextWidthUpTo(int charCount) const;
};

} // namespace UI
} // namespace VividPic
