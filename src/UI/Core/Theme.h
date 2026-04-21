#pragma once

#include "Utils/Types.h"

namespace VividPic {
namespace UI {

// HomeScreen & Workspace visual theme constants
// Based on PRD Section 7
struct Theme {
    // Backgrounds
    static constexpr uint32_t BackgroundDark     = 0x2B2B2B;
    static constexpr uint32_t PanelBackground    = 0x3C3C3C;
    static constexpr uint32_t ButtonDefault      = 0x4A4A4A;
    static constexpr uint32_t ButtonHover        = 0x5A5A5A;
    static constexpr uint32_t ButtonPressed      = 0x404040;
    static constexpr uint32_t CanvasOuter        = 0x4A4A4A;
    
    // Accents
    static constexpr uint32_t HighlightBlue      = 0x0078D7;
    static constexpr uint32_t HighlightHover     = 0x1C97EA;
    static constexpr uint32_t SuccessGreen       = 0x2ECC71;
    static constexpr uint32_t WarningYellow      = 0xF1C40F;
    static constexpr uint32_t DangerRed          = 0xE74C3C;
    
    // Text
    static constexpr uint32_t TextPrimary        = 0xE0E0E0;
    static constexpr uint32_t TextSecondary      = 0xA0A0A0;
    static constexpr uint32_t TextDisabled       = 0x707070;
    static constexpr uint32_t TextInverse        = 0xFFFFFF;
    
    // Borders
    static constexpr uint32_t BorderLight        = 0x555555;
    static constexpr uint32_t BorderDark         = 0x222222;
    
    // Brush color tags
    static constexpr uint32_t BrushTagPen        = 0xE74C3C; // Red = dip pen
    static constexpr uint32_t BrushTagSpecial    = 0x2ECC71; // Green = special
    static constexpr uint32_t BrushTagBrush      = 0xF1C40F; // Yellow = brush
    static constexpr uint32_t BrushTagEffect     = 0x95A5A6; // Gray = effect
    
    // DPI / UI Scale
    static float Scale;
    static float GetFontSize(float base) { return base * Scale; }
    static int   GetSize(int base) { return static_cast<int>(base * Scale); }
    
    // Utility
    static Color ColorFromHex(uint32_t hex) {
        return Color::FromHex(hex);
    }
    
    static HBRUSH SolidBrush(uint32_t hex) {
        return CreateSolidBrush(RGB(
            (hex >> 16) & 0xFF,
            (hex >> 8) & 0xFF,
            hex & 0xFF
        ));
    }
};

} // namespace UI
} // namespace VividPic
