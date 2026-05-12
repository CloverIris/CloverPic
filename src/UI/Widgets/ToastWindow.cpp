#include "UI/Widgets/ToastWindow.h"
#include "UI/Core/Theme.h"
#include <windowsx.h>

namespace VividPic {
namespace UI {

ToastWindow::ToastWindow() = default;

void ToastWindow::Show(const wchar_t* text, int durationMs) {
    m_text = text ? text : L"";
    if (m_text.empty()) {
        Hide();
        return;
    }
    
    if (!m_hwnd) {
        // Lazy create
        Rect r(0, 0, 200, 40);
        Create(L"", r, nullptr);
    }
    
    // Measure text to size window
    HDC hdc = GetDC(m_hwnd);
    HFONT font = Theme::GetCachedFont(Theme::FontID::Label);
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
    SIZE ts;
    GetTextExtentPoint32W(hdc, m_text.c_str(), static_cast<int>(m_text.length()), &ts);
    SelectObject(hdc, oldFont);
    ReleaseDC(m_hwnd, hdc);
    
    int padX = Theme::GetSize(24);
    int padY = Theme::GetSize(12);
    int w = ts.cx + padX * 2;
    int h = ts.cy + padY * 2;
    if (h < Theme::GetSize(36)) h = Theme::GetSize(36);
    
    // Center on primary monitor, just above bottom
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - w) / 2;
    int y = screenH - h - Theme::GetSize(80);
    
    SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, w, h, SWP_SHOWWINDOW | SWP_NOACTIVATE);
    SetTimer(m_hwnd, TIMER_HIDE, durationMs, nullptr);
    Invalidate();
}

void ToastWindow::Hide() {
    if (m_hwnd) {
        KillTimer(m_hwnd, TIMER_HIDE);
        ShowWindow(m_hwnd, SW_HIDE);
    }
}

void ToastWindow::SetText(const wchar_t* text) {
    m_text = text ? text : L"";
    Invalidate();
}

void ToastWindow::OnPaint(HDC hdc, const Rect& clip) {
    (void)clip;
    Rect client = GetClientBounds();
    
    // Rounded-rect background via simple fill
    HBRUSH bgBrush = Theme::CachedBrush(Theme::TooltipBackground);
    RECT rc = client.ToWin32Rect();
    FillRect(hdc, &rc, bgBrush);
    
    // Border
    HPEN pen = Theme::Pen(Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    HBRUSH nullBr = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
    RoundRect(hdc, 0, 0, client.Width(), client.Height(), Theme::GetSize(8), Theme::GetSize(8));
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(pen);
    
    // Text
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::TextPrimary);
    HFONT font = Theme::GetCachedFont(Theme::FontID::Label);
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
    RECT textRc = { Theme::GetSize(12), 0, client.Width() - Theme::GetSize(12), client.Height() };
    DrawTextW(hdc, m_text.c_str(), -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
    SelectObject(hdc, oldFont);
}

LRESULT ToastWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_TIMER:
            if (wParam == TIMER_HIDE) {
                KillTimer(m_hwnd, TIMER_HIDE);
                ShowWindow(m_hwnd, SW_HIDE);
                return 0;
            }
            break;
    }
    return Window::HandleMessage(msg, wParam, lParam);
}

} // namespace UI
} // namespace VividPic
