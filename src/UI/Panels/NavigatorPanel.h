#pragma once

#include "UI/Core/Window.h"
#include "Core/LayerManager.h"
#include <functional>

namespace VividPic {
namespace UI {

class NavigatorPanel : public Window {
public:
    NavigatorPanel();
    
    void SetLayerManager(LayerManager* manager) { m_layerManager = manager; }
    void SetCanvasViewTransform(float zoom, float panX, float panY, float rotation = 0.0f);
    void SetCanvasSize(uint32_t width, uint32_t height);
    void Refresh();
    
    void SetOnPanChanged(std::function<void(float x, float y)> callback) { m_onPanChanged = std::move(callback); }
    void SetOnZoomChanged(std::function<void(float zoom)> callback) { m_onZoomChanged = std::move(callback); }
    
protected:
    void OnPaint(HDC hdc, const Rect& clip) override;
    void OnMouseDown(const Point& pos, MouseButton button) override;
    void OnMouseMove(const Point& pos) override;
    void OnMouseUp(const Point& pos, MouseButton button) override;
    
    DWORD GetDefaultStyle() const override { return WS_CHILD | WS_VISIBLE; }
    
private:
    void DrawThumbnail(HDC hdc);
    void DrawViewRect(HDC hdc);
    
    LayerManager* m_layerManager = nullptr;
    
    uint32_t m_canvasWidth = 0;
    uint32_t m_canvasHeight = 0;
    float m_zoom = 1.0f;
    float m_panX = 0.0f;
    float m_panY = 0.0f;
    float m_rotation = 0.0f;
    
    bool m_draggingView = false;
    
    std::function<void(float x, float y)> m_onPanChanged;
    std::function<void(float zoom)> m_onZoomChanged;
    
    static constexpr int ThumbMargin = 8;
    static constexpr int ButtonHeight = 24;
    
    // Cached thumbnail
    std::vector<uint8_t> m_thumbnailData;
    bool m_thumbnailDirty = true;
    
    void UpdateThumbnail();
    float GetThumbScale(int availWidth, int availHeight) const;
};

} // namespace UI
} // namespace VividPic
