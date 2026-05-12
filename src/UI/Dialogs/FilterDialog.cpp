#include "UI/Dialogs/FilterDialog.h"
#include "UI/Core/Theme.h"
#include <sstream>

namespace VividPic {
namespace UI {

FilterDialog::FilterDialog() = default;

bool FilterDialog::Initialize(const String& title, const std::vector<FilterParamDef>& params) {
    m_title = title;
    m_params = params;
    return true;
}

int FilterDialog::GetDialogHeight() const {
    int baseHeight = 80 + static_cast<int>(m_params.size()) * RowHeight + 60;
    return std::max(baseHeight, 180);
}

bool FilterDialog::ShowModal(Window* parent) {
    if (!m_hwnd) {
        int w = Theme::GetSize(DialogWidth);
        int h = Theme::GetSize(GetDialogHeight());
        Rect bounds(0, 0, w, h);
        DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
        DWORD exStyle = WS_EX_DLGMODALFRAME;
        if (!Create(m_title.c_str(), bounds, parent, style, exStyle)) {
            return false;
        }
        CenterOnParent(parent);
    }

    if (parent) {
        EnableWindow(parent->GetHandle(), FALSE);
    }

    SetVisible(true);
    SetFocus();

    MSG msg;
    while (IsVisible() && GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (parent) {
        EnableWindow(parent->GetHandle(), TRUE);
        SetForegroundWindow(parent->GetHandle());
    }

    return m_confirmed;
}

void FilterDialog::CloseDialog(bool confirmed) {
    m_confirmed = confirmed;
    SetVisible(false);
}

void FilterDialog::CenterOnParent(Window* parent) {
    if (!m_hwnd) return;

    RECT rc;
    GetWindowRect(m_hwnd, &rc);
    int dlgW = rc.right - rc.left;
    int dlgH = rc.bottom - rc.top;

    int cx, cy;
    if (parent && parent->GetHandle()) {
        RECT prc;
        GetWindowRect(parent->GetHandle(), &prc);
        cx = (prc.left + prc.right - dlgW) / 2;
        cy = (prc.top + prc.bottom - dlgH) / 2;
    } else {
        cx = (GetSystemMetrics(SM_CXSCREEN) - dlgW) / 2;
        cy = (GetSystemMetrics(SM_CYSCREEN) - dlgH) / 2;
    }

    SetWindowPos(m_hwnd, nullptr, cx, cy, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

std::vector<int> FilterDialog::GetValues() const {
    std::vector<int> values;
    values.reserve(m_params.size());
    for (const auto& p : m_params) {
        values.push_back(p.currentVal);
    }
    return values;
}

bool FilterDialog::OnCreate() {
    int btnY = Theme::GetSize(GetDialogHeight()) - Theme::GetSize(ButtonHeight) - Theme::GetSize(Margin);
    int btnX = Theme::GetSize(DialogWidth) - Theme::GetSize(Margin) - Theme::GetSize(ButtonWidth);

    m_cancelButton = MakeRef<Button>();
    Rect cancelRect(btnX, btnY, btnX + Theme::GetSize(ButtonWidth), btnY + Theme::GetSize(ButtonHeight));
    m_cancelButton->Create(L"取消", cancelRect, this);
    m_cancelButton->SetCallback([this]() { CloseDialog(false); });

    btnX -= Theme::GetSize(ButtonWidth) + Theme::GetSize(10);
    m_okButton = MakeRef<Button>();
    Rect okRect(btnX, btnY, btnX + Theme::GetSize(ButtonWidth), btnY + Theme::GetSize(ButtonHeight));
    m_okButton->Create(L"确定", okRect, this);
    m_okButton->SetCallback([this]() { CloseDialog(true); });

    return true;
}

LRESULT FilterDialog::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CLOSE:
            CloseDialog(false);
            return 0;
    }
    return Window::HandleMessage(msg, wParam, lParam);
}

void FilterDialog::OnPaint(HDC hdc, const Rect& clip) {
    Rect client = GetClientBounds();

    // Background
    HBRUSH bgBrush = Theme::CachedBrush(Theme::PanelBackground);
    RECT rc = client.ToWin32Rect();
    FillRect(hdc, &rc, bgBrush);

    // Title and sliders
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::TextPrimary);

    HFONT font = Theme::GetCachedFont(Theme::FontID::Label);
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));

    for (size_t i = 0; i < m_params.size(); ++i) {
        DrawSlider(hdc, static_cast<int>(i));
    }

    SelectObject(hdc, oldFont);
}

Rect FilterDialog::GetSliderTrackRect(int paramIndex) const {
    int x = Theme::GetSize(Margin);
    int y = Theme::GetSize(50) + paramIndex * Theme::GetSize(RowHeight);
    int w = Theme::GetSize(DialogWidth) - Theme::GetSize(Margin) * 2 - Theme::GetSize(50);
    int h = Theme::GetSize(SliderTrackHeight);
    return Rect(x, y + (Theme::GetSize(SliderHeight) - h) / 2, x + w, y + (Theme::GetSize(SliderHeight) - h) / 2 + h);
}

Rect FilterDialog::GetSliderThumbRect(int paramIndex) const {
    const auto& p = m_params[paramIndex];
    Rect track = GetSliderTrackRect(paramIndex);
    float t = static_cast<float>(p.currentVal - p.minVal) / (p.maxVal - p.minVal);
    int thumbX = track.left + static_cast<int>(t * track.Width());
    int thumbW = Theme::GetSize(SliderThumbWidth);
    int thumbH = Theme::GetSize(SliderHeight);
    return Rect(thumbX - thumbW / 2, track.top + track.Height() / 2 - thumbH / 2,
                thumbX + thumbW / 2, track.top + track.Height() / 2 + thumbH / 2);
}

void FilterDialog::DrawSlider(HDC hdc, int paramIndex) {
    const auto& p = m_params[paramIndex];

    // Label
    int labelY = Theme::GetSize(30) + paramIndex * Theme::GetSize(RowHeight);
    Rect labelRect(Theme::GetSize(Margin), labelY, Theme::GetSize(DialogWidth) - Theme::GetSize(Margin), labelY + Theme::GetSize(18));

    std::wstringstream ss;
    ss << p.label << L": " << p.currentVal;
    DrawTextW(hdc, ss.str().c_str(), -1, reinterpret_cast<RECT*>(&labelRect), DT_SINGLELINE | DT_VCENTER | DT_LEFT);

    // Track
    Rect track = GetSliderTrackRect(paramIndex);
    HBRUSH trackBrush = Theme::SolidBrush(Theme::BorderLight);
    RECT trackRc = track.ToWin32Rect();
    FillRect(hdc, &trackRc, trackBrush);
    DeleteObject(trackBrush);

    // Fill
    float t = static_cast<float>(p.currentVal - p.minVal) / (p.maxVal - p.minVal);
    int fillWidth = static_cast<int>(t * track.Width());
    if (fillWidth > 0) {
        HBRUSH fillBrush = Theme::SolidBrush(Theme::HighlightBlue);
        RECT fillRc = { track.left, track.top, track.left + fillWidth, track.bottom };
        FillRect(hdc, &fillRc, fillBrush);
        DeleteObject(fillBrush);
    }

    // Thumb
    Rect thumb = GetSliderThumbRect(paramIndex);
    HBRUSH thumbBrush = Theme::SolidBrush(Theme::TextPrimary);
    RECT thumbRc = thumb.ToWin32Rect();
    FillRect(hdc, &thumbRc, thumbBrush);
    DeleteObject(thumbBrush);
}

int FilterDialog::HitTestSlider(const Point& pos) const {
    for (size_t i = 0; i < m_params.size(); ++i) {
        Rect track = GetSliderTrackRect(static_cast<int>(i));
        // Expand hit test area vertically
        track.top -= Theme::GetSize(8);
        track.bottom += Theme::GetSize(8);
        if (track.Contains(pos)) return static_cast<int>(i);
    }
    return -1;
}

void FilterDialog::SetSliderValue(int paramIndex, const Point& pos) {
    if (paramIndex < 0 || paramIndex >= static_cast<int>(m_params.size())) return;

    Rect track = GetSliderTrackRect(paramIndex);
    int x = pos.x;
    if (x < track.left) x = track.left;
    if (x > track.right) x = track.right;

    float t = static_cast<float>(x - track.left) / track.Width();
    auto& p = m_params[paramIndex];
    p.currentVal = p.minVal + static_cast<int>(t * (p.maxVal - p.minVal) + 0.5f);
    p.currentVal = std::clamp(p.currentVal, p.minVal, p.maxVal);

    Invalidate();
}

void FilterDialog::OnMouseDown(const Point& pos, MouseButton button) {
    if (button != MouseButton::Left) return;

    int idx = HitTestSlider(pos);
    if (idx >= 0) {
        m_draggingSlider = idx;
        SetSliderValue(idx, pos);
        SetCapture(m_hwnd);
    }
}

void FilterDialog::OnMouseMove(const Point& pos) {
    if (m_draggingSlider >= 0) {
        SetSliderValue(m_draggingSlider, pos);
    }
}

void FilterDialog::OnMouseUp(const Point& pos, MouseButton button) {
    if (button == MouseButton::Left && m_draggingSlider >= 0) {
        m_draggingSlider = -1;
        ReleaseCapture();
    }
}

} // namespace UI
} // namespace VividPic
