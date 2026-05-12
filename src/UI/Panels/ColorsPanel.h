#pragma once

#include "UI/Core/Window.h"

namespace VividPic {
namespace UI {

class ColorsPanel : public Window {
public:
    ColorsPanel();
    ~ColorsPanel();
    
    void SetOnColorChanged(std::function<void(const Color&)> callback) { m_onColorChanged = callback; }
    Color GetCurrentColor() const { return m_foregroundActive ? m_foregroundColor : m_backgroundColor; }
    void SetCurrentColor(const Color& color);
    
    Color GetForegroundColor() const { return m_foregroundColor; }
    Color GetBackgroundColor() const { return m_backgroundColor; }
    void SetForegroundColor(const Color& color);
    void SetBackgroundColor(const Color& color);

protected:
    void OnPaint(HDC hdc, const Rect& clip) override;
    void OnMouseDown(const Point& pos, MouseButton button) override;
    void OnMouseMove(const Point& pos) override;
    void OnMouseUp(const Point& pos, MouseButton button) override;
    void OnKeyDown(uint32_t keyCode) override;
    void OnChar(wchar_t ch) override;
    void OnMouseLeave() override;
    
    DWORD GetDefaultStyle() const override { return WS_CHILD | WS_VISIBLE; }
    
private:
    // Layout helpers
    Rect GetSVRect() const;
    Rect GetHueRect() const;
    Rect GetFgSwatchRect() const;
    Rect GetBgSwatchRect() const;
    Rect GetSwapButtonRect() const;
    Rect GetHexRect() const;
    Rect GetHistoryRect(int index) const;
    
    void DrawSVCursor(HDC hdc);
    void DrawHueCursor(HDC hdc);
    void DrawColorInfo(HDC hdc);
    void DrawHexField(HDC hdc);
    void DrawColorHistory(HDC hdc);
    
    void UpdateColorFromPosition(const Point& pos);
    void SetActiveColor(const Color& color);
    void SwapColors();
    void SetForegroundActive(bool active);
    
    void RebuildSVBitmap();
    void RebuildHueBitmap();
    void CleanupBitmaps();
    
    Color HsvToRgb(float h, float s, float v);
    void RgbToHsv(const Color& rgb, float& h, float& s, float& v);
    
    void AddToHistory(const Color& color);
    
    // HEX inline edit
    void StartHexEdit();
    void EndHexEdit(bool apply);
    void HexInsertChar(wchar_t ch);
    void HexDeleteChar();
    void HexMoveCursor(int delta);
    
    // Colors
    Color m_foregroundColor = Color::FromHex(0x000000);
    Color m_backgroundColor = Color::FromHex(0xFFFFFF);
    bool m_foregroundActive = true;
    
    // HSV of active color
    float m_hue = 0.0f;
    float m_saturation = 0.0f;
    float m_value = 0.0f;
    
    static constexpr int SVSquareSize = 140;
    static constexpr int HueBarWidth = 16;
    static constexpr int SwatchSize = 32;
    static constexpr int HistoryCount = 16;
    static constexpr int HistoryItemSize = 24;
    
    Color m_history[HistoryCount];
    int m_historyIndex = 0;
    int m_historyCount = 0;
    
    bool m_draggingSV = false;
    bool m_draggingHue = false;
    
    // Hover
    int m_hoverHistoryIndex = -1;
    bool m_hoverFgSwatch = false;
    bool m_hoverBgSwatch = false;
    bool m_hoverSwapBtn = false;
    
    // HEX edit
    bool m_hexEditing = false;
    String m_hexText;
    int m_hexCursorPos = 0;
    
    std::function<void(const Color&)> m_onColorChanged;
    
    // DIB pre-rendered bitmaps
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
    
    float m_lastHue = -1.0f;
};

} // namespace UI
} // namespace VividPic
