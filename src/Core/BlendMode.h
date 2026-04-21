#pragma once

#include "Utils/Types.h"
#include <cstdint>
#include <algorithm>

namespace VividPic {

enum class BlendMode {
    Normal,
    Multiply,
    Screen,
    Overlay,
    Difference,
    Add,
    Subtract,
    Darken,
    Lighten,
    ColorDodge,
    ColorBurn,
    HardLight,
    SoftLight,
    Exclusion,
    Hue,
    Saturation,
    Color,
    Luminosity
};

class BlendOperations {
public:
    // Blend a single pixel (src over dst) with given blend mode and opacity
    // src: source color (layer pixel)
    // dst: destination color (already composited background)
    // opacity: layer opacity 0.0-1.0
    // Returns blended color
    static Color BlendPixel(const Color& src, const Color& dst, BlendMode mode, float opacity);
    
    // Blend two RGBA colors with Normal mode and given opacity
    static Color AlphaBlend(const Color& src, const Color& dst, float srcOpacity);
    
private:
    // Individual blend mode functions (operate on normalized RGB 0.0-1.0)
    static void NormalBlend(float sr, float sg, float sb, float sa,
                            float dr, float dg, float db, float da,
                            float& outR, float& outG, float& outB, float& outA);
    static void MultiplyBlend(float sr, float sg, float sb, float sa,
                               float dr, float dg, float db, float da,
                               float& outR, float& outG, float& outB, float& outA);
    static void ScreenBlend(float sr, float sg, float sb, float sa,
                             float dr, float dg, float db, float da,
                             float& outR, float& outG, float& outB, float& outA);
    static void OverlayBlend(float sr, float sg, float sb, float sa,
                              float dr, float dg, float db, float da,
                              float& outR, float& outG, float& outB, float& outA);
    static void DifferenceBlend(float sr, float sg, float sb, float sa,
                                 float dr, float dg, float db, float da,
                                 float& outR, float& outG, float& outB, float& outA);
    static void AddBlend(float sr, float sg, float sb, float sa,
                          float dr, float dg, float db, float da,
                          float& outR, float& outG, float& outB, float& outA);
    static void SubtractBlend(float sr, float sg, float sb, float sa,
                               float dr, float dg, float db, float da,
                               float& outR, float& outG, float& outB, float& outA);
    static void DarkenBlend(float sr, float sg, float sb, float sa,
                             float dr, float dg, float db, float da,
                             float& outR, float& outG, float& outB, float& outA);
    static void LightenBlend(float sr, float sg, float sb, float sa,
                              float dr, float dg, float db, float da,
                              float& outR, float& outG, float& outB, float& outA);
};

} // namespace VividPic
