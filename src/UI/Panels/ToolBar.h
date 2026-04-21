#pragma once

#include "UI/Core/Window.h"
#include "UI/Core/ToolType.h"
#include <functional>

namespace VividPic {
namespace UI {

class ToolBar : public Window {
public:
    ToolBar();
    
    void SetCurrentTool(ToolType tool);
    ToolType GetCurrentTool() const { return m_currentTool; }
    void SetOnToolChanged(std::function<void(ToolType)> callback) { m_onToolChanged = std::move(callback); }
    
protected:
    void OnPaint(HDC hdc, const Rect& clip) override;
    void OnMouseDown(const Point& pos, MouseButton button) override;
    void OnMouseMove(const Point& pos) override;
    void OnMouseLeave() override;
    
    DWORD GetDefaultStyle() const override { return WS_CHILD | WS_VISIBLE; }
    
private:
    struct ToolItem {
        ToolType type;
        const wchar_t* icon;     // Unicode symbol or text
        const wchar_t* tooltip;
        int shortcutKey;         // VK code or char
    };
    
    static constexpr ToolItem Tools[] = {
        { ToolType::Brush,        L"笔", L"笔刷 (B)",        'B' },
        { ToolType::Eraser,       L"橡", L"橡皮擦 (E)",      'E' },
        { ToolType::Eyedropper,   L"吸", L"吸管 (I)",        'I' },
        { ToolType::Fill,         L"填", L"油漆桶 (G)",      'G' },
        { ToolType::Gradient,     L"渐", L"渐变",             0 },
        { ToolType::Move,         L"移", L"移动 (V)",        'V' },
        { ToolType::LassoSelect,  L"套", L"套索选择 (L)",     'L' },
        { ToolType::RectSelect,   L"矩", L"矩形选择 (M)",     'M' },
        { ToolType::EllipseSelect,L"椭", L"椭圆选择",          0 },
        { ToolType::MagicWand,    L"魔", L"魔棒选择 (W)",     'W' },
        { ToolType::Transform,    L"变", L"变换 (Ctrl+T)",    0 },
        { ToolType::Text,         L"文", L"文字 (T)",         'T' },
        { ToolType::Shape,        L"形", L"形状",             0 },
    };
    static constexpr int ToolCount = 13;
    static constexpr int IconSize = 32;
    
    ToolType m_currentTool = ToolType::Brush;
    int m_hoverIndex = -1;
    std::function<void(ToolType)> m_onToolChanged;
    
    int HitTest(const Point& pos) const;
    void DrawToolIcon(HDC hdc, int index, const Rect& rc, bool active, bool hovered);
};

} // namespace UI
} // namespace VividPic
