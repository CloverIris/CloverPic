#pragma once

#include "UI/Core/Window.h"
#include "UI/Widgets/Button.h"
#include "Core/LayerManager.h"
#include "Core/BlendMode.h"

namespace VividPic {
namespace UI {

class LayersPanel : public Window {
public:
    LayersPanel();

    void SetLayerManager(LayerManager* manager) { m_layerManager = manager; }
    void Refresh();

protected:
    void OnPaint(HDC hdc, const Rect& clip) override;
    void OnMouseDown(const Point& pos, MouseButton button) override;
    void OnSize(const Size& newSize) override;

    DWORD GetDefaultStyle() const override { return WS_CHILD | WS_VISIBLE; }

private:
    void DrawLayerItem(HDC hdc, int index, const Rect& rect);
    void DrawBlendDropdown(HDC hdc, int x, int y, int width);
    void DrawBlendDropdownList(HDC hdc, int x, int y, int width);

    static const wchar_t* GetBlendModeName(BlendMode mode);

    LayerManager* m_layerManager = nullptr;

    // Layout
    static constexpr int ItemHeight = 48;
    static constexpr int ThumbSize = 32;
    static constexpr int DropdownHeight = 22;

    int HitTestLayer(const Point& pos) const;
    int HitTestButton(const Point& pos, int layerIndex) const; // 0=visibility, 1=lock
    bool HitTestBlendDropdown(const Point& pos) const;
    int HitTestBlendItem(const Point& pos) const;

    // Blend mode dropdown state
    bool m_blendDropdownOpen = false;
    int m_blendHoverIndex = -1;

    static constexpr int BlendModeCount = 8;
    static const BlendMode BlendModes[BlendModeCount];
};

} // namespace UI
} // namespace VividPic
