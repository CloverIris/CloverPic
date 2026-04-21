#include "Core/BlendMode.h"
#include <cmath>

namespace VividPic {

Color BlendOperations::BlendPixel(const Color& src, const Color& dst, BlendMode mode, float opacity) {
    if (opacity <= 0.0f) return dst;
    if (mode == BlendMode::Normal && opacity >= 1.0f && src.a == 255) return src;
    
    float sr = src.r / 255.0f;
    float sg = src.g / 255.0f;
    float sb = src.b / 255.0f;
    float sa = (src.a / 255.0f) * opacity;
    
    float dr = dst.r / 255.0f;
    float dg = dst.g / 255.0f;
    float db = dst.b / 255.0f;
    float da = dst.a / 255.0f;
    
    float outR, outG, outB, outA;
    
    switch (mode) {
        case BlendMode::Multiply:
            MultiplyBlend(sr, sg, sb, sa, dr, dg, db, da, outR, outG, outB, outA);
            break;
        case BlendMode::Screen:
            ScreenBlend(sr, sg, sb, sa, dr, dg, db, da, outR, outG, outB, outA);
            break;
        case BlendMode::Overlay:
            OverlayBlend(sr, sg, sb, sa, dr, dg, db, da, outR, outG, outB, outA);
            break;
        case BlendMode::Difference:
            DifferenceBlend(sr, sg, sb, sa, dr, dg, db, da, outR, outG, outB, outA);
            break;
        case BlendMode::Add:
            AddBlend(sr, sg, sb, sa, dr, dg, db, da, outR, outG, outB, outA);
            break;
        case BlendMode::Subtract:
            SubtractBlend(sr, sg, sb, sa, dr, dg, db, da, outR, outG, outB, outA);
            break;
        case BlendMode::Darken:
            DarkenBlend(sr, sg, sb, sa, dr, dg, db, da, outR, outG, outB, outA);
            break;
        case BlendMode::Lighten:
            LightenBlend(sr, sg, sb, sa, dr, dg, db, da, outR, outG, outB, outA);
            break;
        case BlendMode::Normal:
        default:
            NormalBlend(sr, sg, sb, sa, dr, dg, db, da, outR, outG, outB, outA);
            break;
    }
    
    return Color(
        static_cast<uint8_t>(std::clamp(outR * 255.0f, 0.0f, 255.0f)),
        static_cast<uint8_t>(std::clamp(outG * 255.0f, 0.0f, 255.0f)),
        static_cast<uint8_t>(std::clamp(outB * 255.0f, 0.0f, 255.0f)),
        static_cast<uint8_t>(std::clamp(outA * 255.0f, 0.0f, 255.0f))
    );
}

Color BlendOperations::AlphaBlend(const Color& src, const Color& dst, float srcOpacity) {
    float sa = (src.a / 255.0f) * srcOpacity;
    float da = dst.a / 255.0f;
    
    float outA = sa + da * (1.0f - sa);
    if (outA < 0.001f) return Color(0, 0, 0, 0);
    
    float outR = (src.r * sa + dst.r * da * (1.0f - sa)) / outA;
    float outG = (src.g * sa + dst.g * da * (1.0f - sa)) / outA;
    float outB = (src.b * sa + dst.b * da * (1.0f - sa)) / outA;
    
    return Color(
        static_cast<uint8_t>(std::clamp(outR, 0.0f, 255.0f)),
        static_cast<uint8_t>(std::clamp(outG, 0.0f, 255.0f)),
        static_cast<uint8_t>(std::clamp(outB, 0.0f, 255.0f)),
        static_cast<uint8_t>(std::clamp(outA * 255.0f, 0.0f, 255.0f))
    );
}

void BlendOperations::NormalBlend(float sr, float sg, float sb, float sa,
                                   float dr, float dg, float db, float da,
                                   float& outR, float& outG, float& outB, float& outA) {
    outA = sa + da * (1.0f - sa);
    if (outA < 0.001f) { outR = outG = outB = 0; return; }
    outR = (sr * sa + dr * da * (1.0f - sa)) / outA;
    outG = (sg * sa + dg * da * (1.0f - sa)) / outA;
    outB = (sb * sa + db * da * (1.0f - sa)) / outA;
}

void BlendOperations::MultiplyBlend(float sr, float sg, float sb, float sa,
                                     float dr, float dg, float db, float da,
                                     float& outR, float& outG, float& outB, float& outA) {
    float br = sr * dr;
    float bg = sg * dg;
    float bb = sb * db;
    NormalBlend(br, bg, bb, sa, dr, dg, db, da, outR, outG, outB, outA);
}

void BlendOperations::ScreenBlend(float sr, float sg, float sb, float sa,
                                   float dr, float dg, float db, float da,
                                   float& outR, float& outG, float& outB, float& outA) {
    float br = 1.0f - (1.0f - sr) * (1.0f - dr);
    float bg = 1.0f - (1.0f - sg) * (1.0f - dg);
    float bb = 1.0f - (1.0f - sb) * (1.0f - db);
    NormalBlend(br, bg, bb, sa, dr, dg, db, da, outR, outG, outB, outA);
}

void BlendOperations::OverlayBlend(float sr, float sg, float sb, float sa,
                                    float dr, float dg, float db, float da,
                                    float& outR, float& outG, float& outB, float& outA) {
    auto overlay = [](float s, float d) {
        return d < 0.5f ? 2.0f * s * d : 1.0f - 2.0f * (1.0f - s) * (1.0f - d);
    };
    NormalBlend(overlay(sr, dr), overlay(sg, dg), overlay(sb, db), sa, dr, dg, db, da, outR, outG, outB, outA);
}

void BlendOperations::DifferenceBlend(float sr, float sg, float sb, float sa,
                                       float dr, float dg, float db, float da,
                                       float& outR, float& outG, float& outB, float& outA) {
    NormalBlend(std::abs(sr - dr), std::abs(sg - dg), std::abs(sb - db), sa, dr, dg, db, da, outR, outG, outB, outA);
}

void BlendOperations::AddBlend(float sr, float sg, float sb, float sa,
                                float dr, float dg, float db, float da,
                                float& outR, float& outG, float& outB, float& outA) {
    NormalBlend(std::min(sr + dr, 1.0f), std::min(sg + dg, 1.0f), std::min(sb + db, 1.0f), sa, dr, dg, db, da, outR, outG, outB, outA);
}

void BlendOperations::SubtractBlend(float sr, float sg, float sb, float sa,
                                     float dr, float dg, float db, float da,
                                     float& outR, float& outG, float& outB, float& outA) {
    NormalBlend(std::max(sr - dr, 0.0f), std::max(sg - dg, 0.0f), std::max(sb - db, 0.0f), sa, dr, dg, db, da, outR, outG, outB, outA);
}

void BlendOperations::DarkenBlend(float sr, float sg, float sb, float sa,
                                   float dr, float dg, float db, float da,
                                   float& outR, float& outG, float& outB, float& outA) {
    NormalBlend(std::min(sr, dr), std::min(sg, dg), std::min(sb, db), sa, dr, dg, db, da, outR, outG, outB, outA);
}

void BlendOperations::LightenBlend(float sr, float sg, float sb, float sa,
                                    float dr, float dg, float db, float da,
                                    float& outR, float& outG, float& outB, float& outA) {
    NormalBlend(std::max(sr, dr), std::max(sg, dg), std::max(sb, db), sa, dr, dg, db, da, outR, outG, outB, outA);
}

} // namespace VividPic
