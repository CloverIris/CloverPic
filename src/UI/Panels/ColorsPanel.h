#pragma once

#include "UI/Core/Window.h"

namespace VividPic {
namespace UI {

class ColorsPanel : public Window {
public:
    ColorsPanel();
    ~ColorsPanel();
    
    void SetOnColorChanged(std::function<void(const Color&)> callback) { m_onColorChanged = callback; }
    Color GetCurrentColor() const { return m_currentColor; }
    void SetCurrentColor(const Color& color);
    
protected:
    void OnPaint(HDC hdc, const Rect& clip) override;
    void OnMouseDown(const Point& pos, MouseButton button) override;
    void OnMouseMove(const Point& pos) override;
    void OnMouseUp(const Point& pos, MouseButton button) override;
    
    DWORD GetDefaultStyle() const override { return WS_CHILD | WS_VISIBLE; }
    
private:
    void DrawSaturationValueSquare(HDC hdc);
    void DrawHueBar(HDC hdc);
    void DrawColorHistory(HDC hdc);
    void DrawCurrentColor(HDC hdc);
    void UpdateColorFromPosition(const Point& pos);
    
    void RebuildSVBitmap();
    void RebuildHueBitmap();
    void CleanupBitmaps();
    
    Color HsvToRgb(float h, float s, float v);
    void RgbToHsv(const Color& rgb, float& h, float& s, float& v);
    
    Color m_currentColor = Color::FromHex(0x000000);
    float m_hue = 0.0f;        // 0-360
    float m_saturation = 0.0f; // 0-1
    float m_value = 0.0f;      // 0-1
    
    static constexpr int SVSquareSize = 140;
    static constexpr int HueBarHeight = 16;
    static constexpr int CurrentColorSize = 32;
    static constexpr int HistoryCount = 16;
    static constexpr int HistoryItemSize = 16;
    
    Color m_history[HistoryCount];
    int m_historyIndex = 0;
    int m_historyCount = 0;
    
    bool m_draggingSV = false;
    bool m_draggingHue = false;
    
    std::function<void(const Color&)> m_onColorChanged;
    
    void AddToHistory(const Color& color);
    
    // DIB pre-rendered bitmaps for smooth painting
    HDC m_svDC = nullptr;
    HBITMAP m_svBitmap = nullptr;
    HBITMAP m_svOldBitmap = nullptr;
    uint32_t* m_svPixels = nullptr;
    int m_svBitmapSize = 0;
    
    HDC m_hueDC = nullptr;
    HBITMAP m_hueBitmap = nullptr;
    HBITMAP m_hueOldBitmap = nullptr;
    uint32_t* m_huePixels = nullptr;
    int m_hueBitmapWidth = 0;
    int m_hueBitmapHeight = 0;
    
    float m_lastHue = -1.0f; // tracks if hue changed to rebuild SV bitmap
};

} // namespace UI
} // namespace VividPic
