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
    void SetOnStatusMessage(std::function<void(const wchar_t*)> callback) { m_onStatusMessage = std::move(callback); }
    
protected:
    void OnPaint(HDC hdc, const Rect& clip) override;
    void OnMouseDown(const Point& pos, MouseButton button) override;
    void OnMouseMove(const Point& pos) override;
    void OnMouseLeave() override;
    
    DWORD GetDefaultStyle() const override { return WS_CHILD | WS_VISIBLE; }
    
private:
    struct ToolItem {
        ToolType type;
        wchar_t icon;            // Segoe MDL2 Assets Unicode character
        const wchar_t* tooltip;
        int shortcutKey;         // VK code or char
        bool implemented;        // Whether the tool is fully functional
    };
    
    static constexpr ToolItem Tools[] = {
        { ToolType::Brush,         L'\uE76D', L"笔刷 (B)",         'B', true  },
        { ToolType::Eraser,        L'\uE75C', L"橡皮擦 (E)",       'E', true  },
        { ToolType::Eyedropper,    L'\uE790', L"吸管 (I)",         'I', true  },
        { ToolType::Fill,          L'\uE8B9', L"油漆桶 (G)",       'G', true  },
        { ToolType::Gradient,      L'\uE793', L"渐变",              0,  true  },
        { ToolType::Move,          L'\uE8B0', L"移动 (V)",         'V', true  },
        { ToolType::LassoSelect,   L'\uE7AC', L"套索选择 (L)",      'L', true  },
        { ToolType::RectSelect,    L'\uE7A8', L"矩形选择 (M)",      'M', true  },
        { ToolType::EllipseSelect, L'\uE915', L"椭圆选择",           0,  true  },
        { ToolType::MagicWand,     L'\uE7B3', L"魔棒选择 (W)",      'W', true  },
        { ToolType::Transform,     L'\uE7AD', L"变换 (Ctrl+T)",     0,  true  },
        { ToolType::Text,          L'\uE8D2', L"文字 (T)",          'T', true  },
        { ToolType::Shape,         L'\uE710', L"形状",              0,  true  },
    };
    static constexpr int ToolCount = 13;
    static constexpr int IconSize = 28;
    
    ToolType m_currentTool = ToolType::Brush;
    int m_hoverIndex = -1;
    std::function<void(ToolType)> m_onToolChanged;
    std::function<void(const wchar_t*)> m_onStatusMessage;
    
    int HitTest(const Point& pos) const;
    void DrawToolIcon(HDC hdc, int index, const Rect& rc, bool active, bool hovered);
};

} // namespace UI
} // namespace VividPic
