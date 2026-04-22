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
    // Blend formula: compute blended RGB from two unpremultiplied colors
    // Output is unpremultiplied RGB (alpha handled separately by Over)
    static void BlendFormula(float sr, float sg, float sb,
                             float dr, float dg, float db,
                             BlendMode mode,
                             float& br, float& bg, float& bb);
    
    // Alpha over-composite: blend src over dst using source alpha
    // bsr/bsg/bsb = blended source RGB (unpremultiplied)
    // sa = source alpha
    // dr/dg/db = destination RGB (unpremultiplied)
    // da = destination alpha
    static void Over(float bsr, float bsg, float bsb, float sa,
                     float dr, float dg, float db, float da,
                     float& outR, float& outG, float& outB, float& outA);
};

} // namespace VividPic
