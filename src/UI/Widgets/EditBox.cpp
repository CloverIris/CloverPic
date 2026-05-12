#include "UI/Widgets/EditBox.h"

namespace VividPic {
namespace UI {

EditBox::EditBox() {
    SetTabStop(true);
}

bool EditBox::OnCreate() {
    // Create a 1-pixel-wide caret; height will match font on first show
    CreateCaret(m_hwnd, nullptr, Theme::GetSize(1), Theme::GetSize(16));
    HideCaret(m_hwnd);
    return true;
}

void EditBox::OnDestroy() {
    DestroyCaret();
}

LRESULT EditBox::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SETFOCUS:
            m_focused = true;
            ShowCaret(m_hwnd);
            UpdateCaret();
            Invalidate();
            return 0;
        case WM_KILLFOCUS:
            m_focused = false;
            HideCaret(m_hwnd);
            Invalidate();
            return 0;
    }
    return Window::HandleMessage(msg, wParam, lParam);
}

void EditBox::SetText(const String& text) {
    m_text = text;
    m_cursorPos = static_cast<int>(m_text.length());
    m_selAnchor = m_cursorPos;
    UpdateCaret();
    Invalidate();
}

void EditBox::OnPaint(HDC hdc, const Rect& clip) {
    (void)clip;
    Rect client = GetClientBounds();
    
    // Background
    uint32_t bgColor = m_focused ? 0x2A2A2A : 0x333333;
    HBRUSH brush = Theme::CachedBrush(bgColor);
    RECT rc = client.ToWin32Rect();
    FillRect(hdc, &rc, brush);
    
    // Border
    HPEN borderPen = CreatePen(PS_SOLID, 1, m_focused ? Theme::HighlightBlue : Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
    HBRUSH nullBrush = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, nullBrush));
    Rectangle(hdc, client.left, client.top, client.right, client.bottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(borderPen);
    
    HFONT font = Theme::GetCachedFont(Theme::FontID::Value);
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
    
    int paddingLeft = Theme::GetSize(6);
    int paddingTop = Theme::GetSize(2);
    int textY = paddingTop;
    
    // Selection highlight (invert rect style)
    if (HasSelection() && m_focused) {
        int selStartX = paddingLeft + GetTextWidthUpTo(SelectionStart());
        int selEndX = paddingLeft + GetTextWidthUpTo(SelectionEnd());
        RECT selRc = { selStartX, paddingTop, selEndX, client.Height() - paddingTop };
        InvertRect(hdc, &selRc);
    }
    
    // Text
    if (!m_text.empty()) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, Theme::TextPrimary);
        
        RECT textRect = { paddingLeft, textY, client.right - paddingLeft, client.bottom - paddingTop };
        DrawTextW(hdc, m_text.c_str(), -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    }
    
    SelectObject(hdc, oldFont);
}

void EditBox::OnMouseDown(const Point& pos, MouseButton button) {
    if (button == MouseButton::Left) {
        SetFocus();
        int newPos = HitTestPos(pos.x);
        SetCursorPos(newPos, false);
        Invalidate();
    }
}

void EditBox::OnKeyDown(uint32_t keyCode) {
    if (m_readOnly) {
        // Allow copy/select even when read-only
        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        if (ctrl && keyCode == 'C') {
            CopyToClipboard();
        } else if (ctrl && keyCode == 'A') {
            SetCursorPos(0, false);
            SetCursorPos(static_cast<int>(m_text.length()), true);
        }
        return;
    }
    
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    
    if (ctrl) {
        switch (keyCode) {
            case 'C':
                CopyToClipboard();
                return;
            case 'X':
                CutToClipboard();
                return;
            case 'V':
                PasteFromClipboard();
                return;
            case 'A':
                SetCursorPos(0, false);
                SetCursorPos(static_cast<int>(m_text.length()), true);
                return;
        }
    }
    
    switch (keyCode) {
        case VK_LEFT:
            if (HasSelection() && !shift) {
                SetCursorPos(SelectionStart(), false);
            } else {
                SetCursorPos(m_cursorPos - 1, shift);
            }
            break;
        case VK_RIGHT:
            if (HasSelection() && !shift) {
                SetCursorPos(SelectionEnd(), false);
            } else {
                SetCursorPos(m_cursorPos + 1, shift);
            }
            break;
        case VK_HOME:
            SetCursorPos(0, shift);
            break;
        case VK_END:
            SetCursorPos(static_cast<int>(m_text.length()), shift);
            break;
        case VK_DELETE:
            if (HasSelection()) {
                DeleteSelection();
            } else {
                DeleteCharForward();
            }
            break;
    }
}

void EditBox::OnKeyUp(uint32_t keyCode) {
    (void)keyCode;
}

void EditBox::OnChar(wchar_t ch) {
    if (m_readOnly) return;
    
    if (ch == L'\b') {
        if (HasSelection()) {
            DeleteSelection();
        } else {
            DeleteChar();
        }
    } else if (ch == L'\r' || ch == L'\n') {
        // Enter loses focus (common dialog behavior)
        m_focused = false;
        HideCaret(m_hwnd);
        Invalidate();
    } else if (ch >= 32) {
        if (m_numericOnly) {
            if (!((ch >= L'0' && ch <= L'9') || ch == L'.' || ch == L'-')) {
                return;
            }
        }
        if (HasSelection()) {
            DeleteSelection();
        }
        InsertChar(ch);
    }
}

void EditBox::InsertChar(wchar_t ch) {
    if (m_readOnly) return;
    if (m_cursorPos < 0) m_cursorPos = 0;
    if (m_cursorPos > static_cast<int>(m_text.length())) m_cursorPos = static_cast<int>(m_text.length());
    
    m_text.insert(m_cursorPos, 1, ch);
    ++m_cursorPos;
    m_selAnchor = m_cursorPos;
    UpdateCaret();
    Invalidate();
    NotifyChanged();
}

void EditBox::DeleteChar() {
    if (m_readOnly) return;
    if (m_cursorPos > 0) {
        m_text.erase(m_cursorPos - 1, 1);
        --m_cursorPos;
        m_selAnchor = m_cursorPos;
        UpdateCaret();
        Invalidate();
        NotifyChanged();
    }
}

void EditBox::DeleteCharForward() {
    if (m_readOnly) return;
    if (m_cursorPos < static_cast<int>(m_text.length())) {
        m_text.erase(m_cursorPos, 1);
        // cursor stays
        m_selAnchor = m_cursorPos;
        UpdateCaret();
        Invalidate();
        NotifyChanged();
    }
}

void EditBox::NotifyChanged() {
    if (m_onChanged) {
        m_onChanged();
    }
}

void EditBox::UpdateCaret() {
    if (!m_hwnd) return;
    int x = Theme::GetSize(6) + GetTextWidthUpTo(m_cursorPos);
    int y = Theme::GetSize(2);
    SetCaretPos(x, y);
}

void EditBox::SetCursorPos(int pos, bool extendSelection) {
    pos = std::clamp(pos, 0, static_cast<int>(m_text.length()));
    if (!extendSelection) {
        m_selAnchor = pos;
    }
    m_cursorPos = pos;
    UpdateCaret();
    Invalidate();
}

void EditBox::ClearSelection() {
    m_selAnchor = m_cursorPos;
    Invalidate();
}

bool EditBox::HasSelection() const {
    return m_selAnchor != m_cursorPos;
}

int EditBox::SelectionStart() const {
    return std::min(m_selAnchor, m_cursorPos);
}

int EditBox::SelectionEnd() const {
    return std::max(m_selAnchor, m_cursorPos);
}

void EditBox::DeleteSelection() {
    if (!HasSelection()) return;
    int start = SelectionStart();
    int end = SelectionEnd();
    m_text.erase(start, end - start);
    m_cursorPos = start;
    m_selAnchor = start;
    UpdateCaret();
    Invalidate();
    NotifyChanged();
}

void EditBox::CopyToClipboard() {
    if (!HasSelection()) return;
    int start = SelectionStart();
    int end = SelectionEnd();
    String selText = m_text.substr(start, end - start);
    
    if (!OpenClipboard(m_hwnd)) return;
    EmptyClipboard();
    
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (selText.length() + 1) * sizeof(wchar_t));
    if (hMem) {
        wchar_t* pMem = static_cast<wchar_t*>(GlobalLock(hMem));
        memcpy(pMem, selText.c_str(), (selText.length() + 1) * sizeof(wchar_t));
        GlobalUnlock(hMem);
        SetClipboardData(CF_UNICODETEXT, hMem);
    }
    CloseClipboard();
}

void EditBox::PasteFromClipboard() {
    if (m_readOnly) return;
    if (!OpenClipboard(m_hwnd)) return;
    
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData) {
        const wchar_t* pData = static_cast<const wchar_t*>(GlobalLock(hData));
        if (pData) {
            String clipText(pData);
            GlobalUnlock(hData);
            
            // Filter numeric if needed
            if (m_numericOnly) {
                String filtered;
                for (wchar_t c : clipText) {
                    if ((c >= L'0' && c <= L'9') || c == L'.' || c == L'-') {
                        filtered += c;
                    }
                }
                clipText = filtered;
            }
            
            if (HasSelection()) {
                DeleteSelection();
            }
            for (wchar_t c : clipText) {
                if (c >= 32) {
                    m_text.insert(m_cursorPos, 1, c);
                    ++m_cursorPos;
                }
            }
            m_selAnchor = m_cursorPos;
            UpdateCaret();
            Invalidate();
            NotifyChanged();
        }
    }
    CloseClipboard();
}

void EditBox::CutToClipboard() {
    if (m_readOnly) return;
    CopyToClipboard();
    DeleteSelection();
}

int EditBox::HitTestPos(int mouseX) const {
    int paddingLeft = Theme::GetSize(6);
    if (m_text.empty()) return 0;
    
    HDC hdc = GetDC(m_hwnd);
    HFONT font = Theme::GetCachedFont(Theme::FontID::Value);
    HFONT old = static_cast<HFONT>(SelectObject(hdc, font));
    
    int bestPos = 0;
    int bestDist = INT_MAX;
    for (int i = 0; i <= static_cast<int>(m_text.length()); ++i) {
        SIZE sz;
        GetTextExtentPoint32W(hdc, m_text.c_str(), i, &sz);
        int dist = abs(paddingLeft + sz.cx - mouseX);
        if (dist < bestDist) {
            bestDist = dist;
            bestPos = i;
        }
    }
    
    SelectObject(hdc, old);
    ReleaseDC(m_hwnd, hdc);
    return bestPos;
}

int EditBox::GetTextWidthUpTo(int charCount) const {
    if (charCount <= 0) return 0;
    if (charCount > static_cast<int>(m_text.length())) charCount = static_cast<int>(m_text.length());
    
    HDC hdc = GetDC(m_hwnd);
    HFONT font = Theme::GetCachedFont(Theme::FontID::Value);
    HFONT old = static_cast<HFONT>(SelectObject(hdc, font));
    
    SIZE sz;
    GetTextExtentPoint32W(hdc, m_text.c_str(), charCount, &sz);
    
    SelectObject(hdc, old);
    ReleaseDC(m_hwnd, hdc);
    return sz.cx;
}

} // namespace UI
} // namespace VividPic
