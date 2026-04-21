#pragma once

#include "UI/Core/Window.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/ComboBox.h"
#include "Core/LayerManager.h"

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
    void DrawOpacitySlider(HDC hdc, int x, int y, int width, float opacity);
    
    LayerManager* m_layerManager = nullptr;
    
    // Layout
    static constexpr int ItemHeight = 48;
    static constexpr int ThumbSize = 32;
    
    int HitTestLayer(const Point& pos) const;
    int HitTestButton(const Point& pos, int layerIndex) const; // 0=visibility, 1=lock
    
    // Cached buttons
    Ref<Button> m_btnNewLayer;
    Ref<Button> m_btnDelete;
    Ref<Button> m_btnMoveUp;
    Ref<Button> m_btnMoveDown;
    Ref<Button> m_btnMerge;
    Ref<ComboBox> m_blendCombo;
};

} // namespace UI
} // namespace VividPic
