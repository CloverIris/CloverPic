#include "UI/Dialogs/TextInputDialog.h"
#include "UI/Core/Theme.h"
#include "Render/FontManager.h"
#include <commdlg.h>
#include <sstream>

namespace VividPic {
namespace UI {

TextInputDialog::TextInputDialog() = default;

bool TextInputDialog::Initialize() {
    return true;
}

bool TextInputDialog::ShowModal(Window* parent, const TextProperties& initial) {
    m_properties = initial;
    m_confirmed = false;
    
    if (!m_hwnd) {
        Rect bounds(0, 0, DialogWidth, DialogHeight);
        DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
        DWORD exStyle = WS_EX_DLGMODALFRAME;
        if (!Create(L"编辑文本", bounds, parent, style, exStyle)) {
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

void TextInputDialog::CenterOnParent(Window* parent) {
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

LRESULT TextInputDialog::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CLOSE:
            OnCancelClicked();
            return 0;
    }
    return Window::HandleMessage(msg, wParam, lParam);
}

void TextInputDialog::CloseDialog() {
    SetVisible(false);
    PostMessage(m_hwnd, WM_NULL, 0, 0);
}

bool TextInputDialog::OnCreate() {
    CreateControls();
    LayoutControls();
    return true;
}

void TextInputDialog::CreateControls() {
    // Text edit
    m_textEdit = MakeRef<EditBox>();
    m_textEdit->Create(L"", Rect(0, 0, 1, 1), this);
    m_textEdit->SetText(m_properties.text);
    
    // Font combo
    m_fontCombo = MakeRef<ComboBox>();
    m_fontCombo->Create(L"", Rect(0, 0, 1, 1), this);
    
    auto& fm = FontManager::GetInstance();
    if (!fm.IsInitialized()) {
        fm.Initialize();
    }
    int selectedFont = 0;
    const auto& families = fm.GetFontFamilies();
    for (size_t i = 0; i < families.size(); ++i) {
        m_fontCombo->AddItem(families[i]);
        if (families[i] == m_properties.fontFamily) {
            selectedFont = static_cast<int>(i);
        }
    }
    m_fontCombo->SetSelectedIndex(selectedFont);
    
    // Size edit
    m_sizeEdit = MakeRef<EditBox>();
    m_sizeEdit->Create(L"", Rect(0, 0, 1, 1), this);
    m_sizeEdit->SetNumericOnly(true);
    std::wstringstream ss;
    ss << static_cast<int>(m_properties.fontSize);
    m_sizeEdit->SetText(ss.str());
    
    // Color button
    m_colorButton = MakeRef<Button>();
    m_colorButton->Create(L"选择颜色...", Rect(0, 0, 1, 1), this);
    m_colorButton->SetCallback([this]() { OnChooseColorClicked(); });
    
    // Confirm / Cancel
    m_confirmButton = MakeRef<Button>();
    m_confirmButton->Create(L"确定", Rect(0, 0, 1, 1), this);
    m_confirmButton->SetCallback([this]() { OnConfirmClicked(); });
    
    m_cancelButton = MakeRef<Button>();
    m_cancelButton->Create(L"取消", Rect(0, 0, 1, 1), this);
    m_cancelButton->SetCallback([this]() { OnCancelClicked(); });
}

void TextInputDialog::LayoutControls() {
    int margin = 20;
    int rowH = 32;
    int labelW = 80;
    int inputX = margin + labelW;
    int inputW = DialogWidth - inputX - margin;
    
    int y = margin;
    
    // Text row
    m_textEdit->SetBounds(Rect(inputX, y, inputX + inputW, y + rowH));
    y += rowH + 12;
    
    // Font row
    m_fontCombo->SetBounds(Rect(inputX, y, inputX + inputW, y + rowH));
    y += rowH + 12;
    
    // Size row
    m_sizeEdit->SetBounds(Rect(inputX, y, inputX + 80, y + rowH));
    m_colorButton->SetBounds(Rect(inputX + 100, y, inputX + 200, y + rowH));
    y += rowH + 20;
    
    // Buttons
    int btnW = 80;
    int btnH = 32;
    int btnY = DialogHeight - margin - btnH;
    m_confirmButton->SetBounds(Rect(DialogWidth - margin - btnW * 2 - 10, btnY, DialogWidth - margin - btnW - 10, btnY + btnH));
    m_cancelButton->SetBounds(Rect(DialogWidth - margin - btnW, btnY, DialogWidth - margin, btnY + btnH));
}

void TextInputDialog::OnPaint(HDC hdc, const Rect& clip) {
    Rect client = GetClientBounds();
    
    // Background
    HBRUSH bgBrush = Theme::SolidBrush(Theme::PanelBackground);
    RECT rc = client.ToWin32Rect();
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);
    
    // Labels
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::TextPrimary);
    HFONT font = Theme::GetFont(Theme::FontID::Label);
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
    
    int margin = 20;
    int rowH = 32;
    int labelW = 80;
    int y = margin;
    
    RECT labelRc = { margin, y, margin + labelW, y + rowH };
    DrawTextW(hdc, L"文本:", -1, &labelRc, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
    y += rowH + 12;
    
    labelRc = { margin, y, margin + labelW, y + rowH };
    DrawTextW(hdc, L"字体:", -1, &labelRc, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
    y += rowH + 12;
    
    labelRc = { margin, y, margin + labelW, y + rowH };
    DrawTextW(hdc, L"大小:", -1, &labelRc, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
    
    SelectObject(hdc, oldFont);
}

void TextInputDialog::OnConfirmClicked() {
    m_properties.text = m_textEdit->GetText();
    m_properties.fontFamily = m_fontCombo->GetSelectedItem();
    
    String sizeStr = m_sizeEdit->GetText();
    if (!sizeStr.empty()) {
        m_properties.fontSize = static_cast<float>(_wtoi(sizeStr.c_str()));
        if (m_properties.fontSize < 1.0f) m_properties.fontSize = 1.0f;
        if (m_properties.fontSize > 1000.0f) m_properties.fontSize = 1000.0f;
    }
    
    m_confirmed = true;
    CloseDialog();
}

void TextInputDialog::OnCancelClicked() {
    m_confirmed = false;
    CloseDialog();
}

void TextInputDialog::OnChooseColorClicked() {
    static COLORREF customColors[16] = {0};
    
    CHOOSECOLOR cc = {};
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = m_hwnd;
    cc.rgbResult = RGB(m_properties.color.r, m_properties.color.g, m_properties.color.b);
    cc.lpCustColors = customColors;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
    
    if (ChooseColor(&cc)) {
        m_properties.color = Color(
            GetRValue(cc.rgbResult),
            GetGValue(cc.rgbResult),
            GetBValue(cc.rgbResult),
            255
        );
    }
}

TextProperties TextInputDialog::GetProperties() const {
    return m_properties;
}

} // namespace UI
} // namespace VividPic
