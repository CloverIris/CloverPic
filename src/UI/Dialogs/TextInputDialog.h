#pragma once

#include "UI/Core/Window.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/EditBox.h"
#include "UI/Widgets/ComboBox.h"
#include "Utils/Types.h"

namespace VividPic {
namespace UI {

struct TextProperties {
    std::wstring text;
    std::wstring fontFamily;
    float fontSize = 24.0f;
    Color color = Color(255, 255, 255, 255);
};

class TextInputDialog : public Window {
public:
    TextInputDialog();
    
    bool Initialize();
    bool ShowModal(Window* parent, const TextProperties& initial = {});
    void CloseDialog();
    void CenterOnParent(Window* parent);
    
    bool WasConfirmed() const { return m_confirmed; }
    TextProperties GetProperties() const;
    
protected:
    void OnPaint(HDC hdc, const Rect& clip) override;
    bool OnCreate() override;
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) override;
    
    DWORD GetDefaultStyle() const override { return WS_POPUP | WS_CAPTION | WS_SYSMENU; }
    DWORD GetDefaultExStyle() const override { return WS_EX_DLGMODALFRAME; }
    
private:
    void CreateControls();
    void LayoutControls();
    void OnConfirmClicked();
    void OnCancelClicked();
    void OnChooseColorClicked();
    
    Ref<EditBox> m_textEdit;
    Ref<ComboBox> m_fontCombo;
    Ref<EditBox> m_sizeEdit;
    Ref<Button> m_colorButton;
    Ref<Button> m_confirmButton;
    Ref<Button> m_cancelButton;
    
    TextProperties m_properties;
    bool m_confirmed = false;
    
    static constexpr int DialogWidth = 420;
    static constexpr int DialogHeight = 260;
};

} // namespace UI
} // namespace VividPic
