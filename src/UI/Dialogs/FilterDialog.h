#pragma once

#include "UI/Core/Window.h"
#include "UI/Widgets/Button.h"
#include <vector>
#include <functional>

namespace VividPic {
namespace UI {

struct FilterParamDef {
    String label;
    int minVal = 0;
    int maxVal = 100;
    int defaultVal = 50;
    int currentVal = 50;
};

class FilterDialog : public Window {
public:
    FilterDialog();
    ~FilterDialog() override = default;

    bool Initialize(const String& title, const std::vector<FilterParamDef>& params);
    bool ShowModal(Window* parent);
    void CloseDialog(bool confirmed);
    void CenterOnParent(Window* parent);

    bool WasConfirmed() const { return m_confirmed; }
    std::vector<int> GetValues() const;

protected:
    void OnPaint(HDC hdc, const Rect& clip) override;
    void OnMouseDown(const Point& pos, MouseButton button) override;
    void OnMouseMove(const Point& pos) override;
    void OnMouseUp(const Point& pos, MouseButton button) override;
    bool OnCreate() override;

    DWORD GetDefaultStyle() const override {
        return WS_POPUP | WS_CAPTION | WS_SYSMENU;
    }
    DWORD GetDefaultExStyle() const override { return WS_EX_DLGMODALFRAME; }

    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) override;

private:
    String m_title;
    std::vector<FilterParamDef> m_params;
    bool m_confirmed = false;

    Ref<Button> m_okButton;
    Ref<Button> m_cancelButton;

    int m_draggingSlider = -1;

    // Layout (base sizes, scaled by Theme::Scale at runtime)
    static constexpr int DialogWidth = 320;
    static constexpr int SliderHeight = 20;
    static constexpr int SliderTrackHeight = 6;
    static constexpr int SliderThumbWidth = 10;
    static constexpr int RowHeight = 48;
    static constexpr int Margin = 20;
    static constexpr int ButtonWidth = 80;
    static constexpr int ButtonHeight = 28;

    int GetDialogHeight() const;
    Rect GetSliderTrackRect(int paramIndex) const;
    Rect GetSliderThumbRect(int paramIndex) const;
    int HitTestSlider(const Point& pos) const;
    void SetSliderValue(int paramIndex, const Point& pos);
    void DrawSlider(HDC hdc, int paramIndex);
};

} // namespace UI
} // namespace VividPic
