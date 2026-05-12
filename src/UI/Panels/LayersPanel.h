#pragma once

#include "UI/Core/Window.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/ScrollView.h"
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
    void OnMouseMove(const Point& pos) override;
    void OnMouseUp(const Point& pos, MouseButton button) override;
    void OnMouseLeave() override;
    void OnSize(const Size& newSize) override;
    void OnMouseWheel(int delta) override;
    void OnKeyDown(uint32_t keyCode) override;
    void OnChar(wchar_t ch) override;

    DWORD GetDefaultStyle() const override { return WS_CHILD | WS_VISIBLE; }

private:
    void DrawLayerItem(HDC hdc, int index, const Rect& rect);
    void DrawBlendDropdown(HDC hdc, int x, int y, int width);
    void DrawBlendDropdownList(HDC hdc, int x, int y, int width);
    void DrawOpacitySlider(HDC hdc, int x, int y, int width, uint8_t opacity);
    void DrawToolbar(HDC hdc);
    void DrawRenameField(HDC hdc, const Rect& nameRc);

    static const wchar_t* GetBlendModeName(BlendMode mode);
    static wchar_t GetLayerIcon(int btnType, bool state); // 0=eye, 1=lock, 2=solo

    LayerManager* m_layerManager = nullptr;

    // Layout
    static constexpr int ItemHeight = 48;
    static constexpr int ThumbSize = 32;
    static constexpr int DropdownHeight = 22;
    static constexpr int BtnWidth = 20;
    static constexpr int BtnGap = 4;

    int GetLayerListY() const;
    Rect GetLayerItemRect(int layerIndex) const;
    void GetLayerItemButtonRects(int layerIndex, Rect& outSolo, Rect& outPA, Rect& outEye, Rect& outLock) const;
    int HitTestLayer(const Point& pos) const;
    int HitTestButton(const Point& pos, int layerIndex) const; // 0=visibility, 1=lock, 2=protectAlpha, 3=solo
    bool HitTestBlendDropdown(const Point& pos) const;
    int HitTestBlendItem(const Point& pos) const;
    int HitTestOpacitySlider(const Point& pos) const;
    int HitTestToolbarButton(const Point& pos) const; // 0=new, 1=duplicate, 2=merge, 3=delete

    void SetOpacityFromSliderPos(int x, int trackX, int trackWidth);

    // Blend mode dropdown state
    bool m_blendDropdownOpen = false;
    int m_blendHoverIndex = -1;

    // Opacity slider state
    bool m_opacityDragging = false;

    // Layer drag reorder state
    bool m_draggingLayer = false;
    int m_dragLayerIndex = -1;
    int m_dragTargetIndex = -1;
    int m_dragStartY = 0;

    // Hover state
    int m_hoverLayerIndex = -1;
    int m_hoverToolbarBtn = -1;

    // Rename inline edit
    bool m_renamingLayer = false;
    int m_renameLayerIndex = -1;
    String m_renameText;
    int m_renameCursorPos = 0;
    uint64_t m_lastClickTime = 0;
    int m_lastClickLayer = -1;
    void StartRename(int layerIndex);
    void EndRename(bool apply);
    void RenameInsertChar(wchar_t ch);
    void RenameDeleteChar();
    void RenameMoveCursor(int delta);

    // Opacity value inline edit
    bool m_opacityEditing = false;
    String m_opacityText;
    int m_opacityCursorPos = 0;
    void StartOpacityEdit();
    void EndOpacityEdit(bool apply);

    ScrollView m_scrollView;
    bool m_scrollbarDragging = false;
    Rect GetListViewportRect() const;
    void UpdateScrollView();

    static constexpr int BlendModeCount = 18;
    static const BlendMode BlendModes[BlendModeCount];
};

} // namespace UI
} // namespace VividPic
