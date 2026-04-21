#pragma once

#include "Utils/Types.h"
#include <windows.h>

namespace VividPic {
namespace UI {

// -------------------------------------------------------------------------
// Visual theme constants + unified drawing helpers
// Based on PRD Section 7 + reference screenshot (MediBang Paint style)
// -------------------------------------------------------------------------

struct Theme {
    // Backgrounds
    static constexpr uint32_t BackgroundDark     = 0x2B2B2B;
    static constexpr uint32_t PanelBackground    = 0x3C3C3C;
    static constexpr uint32_t PanelHeader        = 0x363636;
    static constexpr uint32_t ButtonDefault      = 0x4A4A4A;
    static constexpr uint32_t ButtonHover        = 0x5A5A5A;
    static constexpr uint32_t ButtonPressed      = 0x404040;
    static constexpr uint32_t CanvasOuter        = 0x4A4A4A;
    static constexpr uint32_t InputBackground    = 0x2E2E2E;
    static constexpr uint32_t DropdownBackground = 0x454545;
    static constexpr uint32_t TooltipBackground  = 0x333333;
    
    // Accents
    static constexpr uint32_t HighlightBlue      = 0x0078D7;
    static constexpr uint32_t HighlightHover     = 0x1C97EA;
    static constexpr uint32_t HighlightActive    = 0x005A9E;
    static constexpr uint32_t SuccessGreen       = 0x2ECC71;
    static constexpr uint32_t WarningYellow      = 0xF1C40F;
    static constexpr uint32_t DangerRed          = 0xE74C3C;
    
    // Text
    static constexpr uint32_t TextPrimary        = 0xE0E0E0;
    static constexpr uint32_t TextSecondary      = 0xA0A0A0;
    static constexpr uint32_t TextDisabled       = 0x707070;
    static constexpr uint32_t TextInverse        = 0xFFFFFF;
    static constexpr uint32_t TextLabel          = 0xC0C0C0;
    
    // Borders / Separators
    static constexpr uint32_t BorderLight        = 0x555555;
    static constexpr uint32_t BorderDark         = 0x222222;
    static constexpr uint32_t SeparatorDark      = 0x1E1E1E;
    static constexpr uint32_t SeparatorLight     = 0x505050;
    
    // Sliders
    static constexpr uint32_t SliderTrack        = 0x555555;
    static constexpr uint32_t SliderFill         = 0x0078D7;
    static constexpr uint32_t SliderThumb        = 0xE0E0E0;
    static constexpr uint32_t SliderThumbHover   = 0xFFFFFF;
    
    // Checkboxes
    static constexpr uint32_t CheckBoxBg         = 0x3C3C3C;
    static constexpr uint32_t CheckBoxBorder     = 0x666666;
    static constexpr uint32_t CheckBoxCheck      = 0x0078D7;
    
    // Brush color tags
    static constexpr uint32_t BrushTagPen        = 0xE74C3C; // Red = dip pen
    static constexpr uint32_t BrushTagSpecial    = 0x2ECC71; // Green = special
    static constexpr uint32_t BrushTagBrush      = 0xF1C40F; // Yellow = brush
    static constexpr uint32_t BrushTagEffect     = 0x95A5A6; // Gray = effect
    
    // -----------------------------------------------------------------
    // DPI / UI Scale
    // -----------------------------------------------------------------
    static float Scale;
    static float GetFontSize(float base) { return base * Scale; }
    static int   GetSize(int base) { return static_cast<int>(base * Scale); }
    
    // -----------------------------------------------------------------
    // Font specification
    // -----------------------------------------------------------------
    enum class FontID {
        Title,       // 28pt Bold  - HomeScreen title
        Menu,        // 12pt Normal - Menu bar
        Toolbar,     // 11pt Normal - Toolbar buttons
        PanelTitle,  // 12pt Bold  - Panel header titles
        Label,       // 11pt Normal - General labels
        Value,       // 11pt Normal - Value displays
        Small,       // 10pt Normal - Secondary info
        Button       // 12pt Normal - Button text
    };
    
    static HFONT GetFont(FontID id);
    
    // -----------------------------------------------------------------
    // Drawing helpers (all operate in pixel coordinates, NOT scaled)
    // Callers should pass already-scaled coordinates.
    // -----------------------------------------------------------------
    
    // Solid brush factory (caller must DeleteObject)
    static HBRUSH SolidBrush(uint32_t hex) {
        return CreateSolidBrush(RGB(
            (hex >> 16) & 0xFF,
            (hex >> 8) & 0xFF,
            hex & 0xFF
        ));
    }
    
    // Pen factory (caller must DeleteObject)
    static HPEN Pen(uint32_t hex, int width = 1) {
        return CreatePen(PS_SOLID, width, RGB(
            (hex >> 16) & 0xFF,
            (hex >> 8) & 0xFF,
            hex & 0xFF
        ));
    }
    
    // Vertical gradient fill (top -> bottom color)
    static void DrawGradientRect(HDC hdc, const Rect& rc, uint32_t topColor, uint32_t bottomColor);
    
    // Bevel border (raised or recessed)
    static void DrawBevelRect(HDC hdc, const Rect& rc, uint32_t highlight, uint32_t shadow, bool raised = true);
    
    // Rounded rectangle fill
    static void DrawRoundRect(HDC hdc, const Rect& rc, int radius, uint32_t fillColor, uint32_t borderColor = 0);
    
    // Checkbox
    static void DrawCheckBox(HDC hdc, const Rect& rc, bool checked, bool hovered, const wchar_t* label = nullptr);
    
    // Slider (horizontal)
    static void DrawSlider(HDC hdc, const Rect& rc, float value01, bool hovered);
    
    // Tooltip background
    static void DrawTooltip(HDC hdc, const Rect& rc);
    
    // Color conversion
    static Color ColorFromHex(uint32_t hex) {
        return Color::FromHex(hex);
    }
};

} // namespace UI
} // namespace VividPic
