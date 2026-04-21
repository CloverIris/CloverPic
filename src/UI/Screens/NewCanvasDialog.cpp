#include "UI/Screens/NewCanvasDialog.h"
#include "Core/MemoryAdvisor.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace VividPic {
namespace UI {

NewCanvasDialog::NewCanvasDialog() = default;

bool NewCanvasDialog::Initialize() {
    Rect bounds(200, 150, 200 + DialogWidth, 150 + DialogHeight);
    if (!Create(L"新建画布", bounds, nullptr)) {
        return false;
    }
    return true;
}

bool NewCanvasDialog::ShowModal(Window* parent) {
    if (parent) {
        EnableWindow(parent->GetHandle(), FALSE);
        ::SetParent(m_hwnd, parent->GetHandle());
    }
    
    SetVisible(true);
    SetFocus();
    
    // Local message loop for true modal behavior
    MSG msg;
    while (IsVisible() && GetMessage(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessage(m_hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    
    if (parent) {
        EnableWindow(parent->GetHandle(), TRUE);
        SetForegroundWindow(parent->GetHandle());
    }
    
    return m_confirmed;
}

void NewCanvasDialog::CloseDialog() {
    SetVisible(false);
    // Post a dummy message to break the modal loop
    PostMessage(m_hwnd, WM_NULL, 0, 0);
}

bool NewCanvasDialog::OnCreate() {
    // Paper presets
    m_presets = {
        { L"自定义", 0, 0, L"px" },
        { L"A4 (300dpi)", 21.0f, 29.7f, L"cm" },
        { L"A4 (350dpi)", 21.0f, 29.7f, L"cm" },
        { L"A3 (300dpi)", 29.7f, 42.0f, L"cm" },
        { L"B5 (300dpi)", 17.6f, 25.0f, L"cm" },
        { L"US Letter", 8.5f, 11.0f, L"inch" },
        { L"明信片", 10.0f, 14.8f, L"cm" },
        { L"Web (1920x1080)", 1920, 1080, L"px" },
        { L"HD (1280x720)", 1280, 720, L"px" },
    };
    
    CreateControls();
    LayoutControls();
    UpdateCalculations();
    return true;
}

void NewCanvasDialog::CreateControls() {
    m_widthEdit = MakeRef<EditBox>();
    m_widthEdit->SetNumericOnly(true);
    m_widthEdit->SetText(L"1600");
    m_widthEdit->SetOnChanged([this]() { OnWidthChanged(); });
    
    m_heightEdit = MakeRef<EditBox>();
    m_heightEdit->SetNumericOnly(true);
    m_heightEdit->SetText(L"1200");
    m_heightEdit->SetOnChanged([this]() { OnHeightChanged(); });
    
    m_dpiEdit = MakeRef<EditBox>();
    m_dpiEdit->SetNumericOnly(true);
    m_dpiEdit->SetText(L"350");
    m_dpiEdit->SetOnChanged([this]() { OnDpiChanged(); });
    
    m_unitCombo = MakeRef<ComboBox>();
    m_unitCombo->AddItem(L"px");
    m_unitCombo->AddItem(L"cm");
    m_unitCombo->AddItem(L"mm");
    m_unitCombo->AddItem(L"inch");
    m_unitCombo->SetOnSelectionChanged([this](int idx) { OnUnitChanged(idx); });
    
    m_presetCombo = MakeRef<ComboBox>();
    for (const auto& preset : m_presets) {
        m_presetCombo->AddItem(preset.name);
    }
    m_presetCombo->SetOnSelectionChanged([this](int idx) { OnPresetChanged(idx); });
    
    m_bgColorCombo = MakeRef<ComboBox>();
    m_bgColorCombo->AddItem(L"白色");
    m_bgColorCombo->AddItem(L"透明");
    m_bgColorCombo->AddItem(L"黑色");
    m_bgColorCombo->SetOnSelectionChanged([this](int idx) {
        switch (idx) {
            case 0: m_settings.bgColor = Color::FromHex(0xFFFFFF); m_settings.transparent = false; break;
            case 1: m_settings.transparent = true; break;
            case 2: m_settings.bgColor = Color::FromHex(0x000000); m_settings.transparent = false; break;
        }
    });
    
    m_layerCombo = MakeRef<ComboBox>();
    m_layerCombo->AddItem(L"彩色图层");
    m_layerCombo->AddItem(L"灰度图层");
    m_layerCombo->AddItem(L"透明图层");
    m_layerCombo->SetOnSelectionChanged([this](int idx) {
        switch (idx) {
            case 0: m_settings.initialLayer = LayerType::Color; break;
            case 1: m_settings.initialLayer = LayerType::Grayscale; break;
            case 2: m_settings.initialLayer = LayerType::Transparent; break;
        }
    });
    
    m_swapButton = MakeRef<Button>();
    m_swapButton->SetText(L"⇄ 交换");
    m_swapButton->SetCallback([this]() { SwapDimensions(); });
    
    m_createButton = MakeRef<Button>();
    m_createButton->SetText(L"创建");
    m_createButton->SetCallback([this]() { OnCreateClicked(); });
    
    m_cancelButton = MakeRef<Button>();
    m_cancelButton->SetText(L"取消");
    m_cancelButton->SetCallback([this]() { OnCancelClicked(); });
}

void NewCanvasDialog::LayoutControls() {
    int x = Margin + LabelWidth + 8;
    int y = Margin + 20;
    int row = 0;
    
    auto addRow = [&](Window* ctrl, int width = InputWidth) {
        ctrl->Create(L"", Rect(x, y + row * RowHeight, x + width, y + row * RowHeight + 24), this);
        row++;
    };
    
    addRow(m_presetCombo.get(), InputWidth + 60);
    row++; // gap
    addRow(m_widthEdit.get());
    addRow(m_heightEdit.get());
    addRow(m_swapButton.get(), 80);
    addRow(m_unitCombo.get(), 80);
    addRow(m_dpiEdit.get());
    row++; // gap
    addRow(m_bgColorCombo.get());
    addRow(m_layerCombo.get());
    row++;
    
    // Buttons at bottom
    int btnY = DialogHeight - Margin - 32;
    m_createButton->Create(L"", Rect(DialogWidth - Margin - 160, btnY, DialogWidth - Margin - 80, btnY + 28), this);
    m_cancelButton->Create(L"", Rect(DialogWidth - Margin - 70, btnY, DialogWidth - Margin, btnY + 28), this);
}

void NewCanvasDialog::OnPaint(HDC hdc, const Rect& clip) {
    Rect client = GetClientBounds();
    
    // Background
    HBRUSH bgBrush = Theme::SolidBrush(Theme::BackgroundDark);
    RECT rc = client.ToWin32Rect();
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);
    
    // Title
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::TextPrimary);
    HFONT titleFont = CreateFontW(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, titleFont));
    RECT titleRc = { Margin, Margin, client.Width() - Margin, Margin + 24 };
    DrawTextW(hdc, L"新建画布", -1, &titleRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    SelectObject(hdc, oldFont);
    DeleteObject(titleFont);
    
    // Labels
    HFONT labelFont = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    oldFont = static_cast<HFONT>(SelectObject(hdc, labelFont));
    SetTextColor(hdc, Theme::TextSecondary);
    
    int labelX = Margin;
    int y = Margin + 20;
    int row = 0;
    auto drawLabel = [&](const wchar_t* text) {
        RECT lr = { labelX, y + row * RowHeight, labelX + LabelWidth, y + row * RowHeight + 24 };
        DrawTextW(hdc, text, -1, &lr, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
        row++;
    };
    
    drawLabel(L"预设:");
    row++;
    drawLabel(L"宽度:");
    drawLabel(L"高度:");
    row++;
    drawLabel(L"单位:");
    drawLabel(L"DPI:");
    row++;
    drawLabel(L"背景:");
    drawLabel(L"图层:");
    
    SelectObject(hdc, oldFont);
    DeleteObject(labelFont);
    
    // Info area
    row += 2;
    int infoY = y + row * RowHeight;
    SetTextColor(hdc, Theme::TextPrimary);
    HFONT infoFont = CreateFontW(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    oldFont = static_cast<HFONT>(SelectObject(hdc, infoFont));
    
    RECT infoRc = { Margin, infoY, client.Width() - Margin, infoY + 20 };
    DrawTextW(hdc, m_infoText.c_str(), -1, &infoRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    
    SelectObject(hdc, oldFont);
    DeleteObject(infoFont);
    
    // Memory gauge
    UpdateMemoryGauge(hdc);
}

void NewCanvasDialog::UpdateMemoryGauge(HDC hdc) {
    int gaugeY = DialogHeight - Margin - 70;
    int gaugeX = Margin;
    int gaugeWidth = DialogWidth - Margin * 2;
    int gaugeHeight = 20;
    
    // Background bar
    HBRUSH bgBrush = Theme::SolidBrush(Theme::BorderDark);
    RECT bgRc = { gaugeX, gaugeY, gaugeX + gaugeWidth, gaugeY + gaugeHeight };
    FillRect(hdc, &bgRc, bgBrush);
    DeleteObject(bgBrush);
    
    // Fill bar based on status
    uint32_t fillColor;
    switch (m_memoryStatus) {
        case MemoryStatus::Safe: fillColor = Theme::SuccessGreen; break;
        case MemoryStatus::Warning: fillColor = Theme::WarningYellow; break;
        case MemoryStatus::Danger: fillColor = Theme::DangerRed; break;
        default: fillColor = Theme::SuccessGreen;
    }
    
    float ratio = 0.0f;
    auto advice = MemoryAdvisor::GetInstance().CalculateAdvice(m_settings.width, m_settings.height, 1);
    if (advice.usableBudget > 0) {
        ratio = static_cast<float>(advice.totalEstimatedUsage) / static_cast<float>(advice.usableBudget);
    }
    ratio = std::clamp(ratio, 0.0f, 1.0f);
    
    int fillWidth = static_cast<int>(gaugeWidth * ratio);
    if (fillWidth > 0) {
        HBRUSH fillBrush = Theme::SolidBrush(fillColor);
        RECT fillRc = { gaugeX, gaugeY, gaugeX + fillWidth, gaugeY + gaugeHeight };
        FillRect(hdc, &fillRc, fillBrush);
        DeleteObject(fillBrush);
    }
    
    // Border
    HPEN borderPen = CreatePen(PS_SOLID, 1, Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
    HBRUSH nullBrush = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, nullBrush));
    Rectangle(hdc, gaugeX, gaugeY, gaugeX + gaugeWidth, gaugeY + gaugeHeight);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(borderPen);
    
    // Text
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::TextPrimary);
    HFONT font = CreateFontW(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
    
    RECT textRc = { gaugeX + 4, gaugeY, gaugeX + gaugeWidth - 4, gaugeY + gaugeHeight };
    DrawTextW(hdc, m_memoryText.c_str(), -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    
    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

void NewCanvasDialog::UpdateCalculations() {
    float w = 0, h = 0, dpi = 350.0f;
    
    try {
        w = std::stof(m_widthEdit->GetText());
        h = std::stof(m_heightEdit->GetText());
        dpi = std::stof(m_dpiEdit->GetText());
    } catch (...) {
        return;
    }
    
    if (w <= 0 || h <= 0 || dpi <= 0) return;
    
    String unit = m_unitCombo->GetSelectedItem();
    
    uint32_t pxW = static_cast<uint32_t>(ConvertToPixels(w, unit, dpi));
    uint32_t pxH = static_cast<uint32_t>(ConvertToPixels(h, unit, dpi));
    
    m_settings.width = pxW;
    m_settings.height = pxH;
    m_settings.dpi = dpi;
    
    // Info text
    std::wostringstream oss;
    oss << L"纪录: " << pxW << L" px * " << pxH << L" px (" << static_cast<int>(dpi) << L" dpi)";
    m_infoText = oss.str();
    
    // Memory calculation
    auto advice = MemoryAdvisor::GetInstance().CalculateAdvice(pxW, pxH, 1);
    m_memoryStatus = MemoryAdvisor::GetInstance().CheckStatus(advice.totalEstimatedUsage);
    
    std::wostringstream memOss;
    memOss << L"预估: " << MemoryAdvisor::FormatBytes(advice.totalEstimatedUsage)
           << L" / 可用: " << MemoryAdvisor::FormatBytes(advice.usableBudget);
    if (m_memoryStatus == MemoryStatus::Danger) {
        memOss << L" [超出内存限制]";
    } else if (m_memoryStatus == MemoryStatus::Warning) {
        memOss << L" [接近上限]";
    }
    m_memoryText = memOss.str();
    
    // Enable/disable create button
    if (m_createButton) {
        // We can't easily disable the button visually in M2, but we can block the action
    }
    
    Invalidate();
}

float NewCanvasDialog::ConvertToPixels(float value, const String& unit, float dpi) {
    if (unit == L"px") return value;
    if (unit == L"cm") return value / 2.54f * dpi;
    if (unit == L"mm") return value / 25.4f * dpi;
    if (unit == L"inch") return value * dpi;
    return value;
}

void NewCanvasDialog::OnWidthChanged() { UpdateCalculations(); }
void NewCanvasDialog::OnHeightChanged() { UpdateCalculations(); }
void NewCanvasDialog::OnDpiChanged() { UpdateCalculations(); }

void NewCanvasDialog::OnPresetChanged(int index) {
    if (index < 0 || index >= static_cast<int>(m_presets.size())) return;
    
    const auto& preset = m_presets[index];
    if (preset.width == 0 && preset.height == 0) return; // Custom
    
    // Find unit index
    int unitIdx = 0;
    for (int i = 0; i < 4; ++i) {
        if (m_unitCombo->GetSelectedItem() == preset.unit) {
            unitIdx = i;
            break;
        }
    }
    m_unitCombo->SetSelectedIndex(unitIdx);
    
    m_widthEdit->SetText(std::to_wstring(static_cast<int>(preset.width)));
    m_heightEdit->SetText(std::to_wstring(static_cast<int>(preset.height)));
    
    UpdateCalculations();
}

void NewCanvasDialog::OnUnitChanged(int index) {
    UpdateCalculations();
}

void NewCanvasDialog::SwapDimensions() {
    String temp = m_widthEdit->GetText();
    m_widthEdit->SetText(m_heightEdit->GetText());
    m_heightEdit->SetText(temp);
    UpdateCalculations();
}

void NewCanvasDialog::OnCreateClicked() {
    if (m_memoryStatus == MemoryStatus::Danger) {
        MessageBoxW(m_hwnd, L"当前设置超出内存安全限制，无法创建此画布。\n请降低分辨率或减少尺寸。",
                    L"内存不足", MB_OK | MB_ICONWARNING);
        return;
    }
    
    m_confirmed = true;
    CloseDialog();
    
    if (m_onConfirm) {
        m_onConfirm(m_settings);
    }
}

void NewCanvasDialog::OnCancelClicked() {
    m_confirmed = false;
    CloseDialog();
    
    if (m_onCancel) {
        m_onCancel();
    }
}

} // namespace UI
} // namespace VividPic
