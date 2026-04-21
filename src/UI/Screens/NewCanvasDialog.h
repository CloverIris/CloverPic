#pragma once

#include "UI/Core/Window.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/EditBox.h"
#include "UI/Widgets/ComboBox.h"
#include "Core/Project.h"
#include "Core/Layer.h"
#include "Core/MemoryAdvisor.h"
#include <functional>

namespace VividPic {
namespace UI {

struct CanvasSettings {
    uint32_t width = 1600;
    uint32_t height = 1200;
    float dpi = 350.0f;
    String unit = L"px";
    Color bgColor = Color::FromHex(0xFFFFFF);
    bool transparent = false;
    LayerType initialLayer = LayerType::Color;
};

class NewCanvasDialog : public Window {
public:
    NewCanvasDialog();
    ~NewCanvasDialog() override = default;
    
    bool Initialize();
    // Returns true if user clicked Create, false if cancelled
    bool ShowModal(Window* parent);
    void CloseDialog();
    void CenterOnParent(Window* parent);
    
    bool WasConfirmed() const { return m_confirmed; }
    CanvasSettings GetSettings() const { return m_settings; }
    
    void SetOnConfirm(std::function<void(const CanvasSettings&)> callback) { m_onConfirm = callback; }
    void SetOnCancel(Callback callback) { m_onCancel = callback; }
    
protected:
    void OnPaint(HDC hdc, const Rect& clip) override;
    bool OnCreate() override;
    
    DWORD GetDefaultStyle() const override { 
        return WS_POPUP | WS_CAPTION | WS_SYSMENU; 
    }
    DWORD GetDefaultExStyle() const override { return WS_EX_DLGMODALFRAME; }
    
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) override;
    
private:
    void CreateControls();
    void LayoutControls();
    void UpdateCalculations();
    void UpdateMemoryGauge(HDC hdc);
    void DrawLabel(HDC hdc, const String& text, int x, int y, int w, int h);
    
    // Input controls
    Ref<EditBox> m_widthEdit;
    Ref<EditBox> m_heightEdit;
    Ref<EditBox> m_dpiEdit;
    Ref<ComboBox> m_unitCombo;
    Ref<ComboBox> m_presetCombo;
    Ref<ComboBox> m_bgColorCombo;
    Ref<ComboBox> m_layerCombo;
    Ref<Button> m_swapButton;
    Ref<Button> m_createButton;
    Ref<Button> m_cancelButton;
    
    // State
    CanvasSettings m_settings;
    bool m_confirmed = false;
    MemoryStatus m_memoryStatus = MemoryStatus::Safe;
    String m_infoText;
    String m_memoryText;
    
    std::function<void(const CanvasSettings&)> m_onConfirm;
    Callback m_onCancel;
    
    // Layout
    static constexpr int LabelWidth = 100;
    static constexpr int InputWidth = 120;
    static constexpr int RowHeight = 32;
    static constexpr int Margin = 20;
    static constexpr int DialogWidth = 480;
    static constexpr int DialogHeight = 520;
    
    // Paper presets
    struct PaperPreset {
        String name;
        float width;
        float height;
        String unit;
    };
    std::vector<PaperPreset> m_presets;
    
    void ApplyPreset(int index);
    void OnWidthChanged();
    void OnHeightChanged();
    void OnDpiChanged();
    void OnPresetChanged(int index);
    void OnUnitChanged(int index);
    void OnCreateClicked();
    void OnCancelClicked();
    void OnCloseClicked();
    void SwapDimensions();
    
    float ConvertToPixels(float value, const String& unit, float dpi);
};

} // namespace UI
} // namespace VividPic
