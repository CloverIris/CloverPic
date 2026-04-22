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
    
    float br = sr, bg = sg, bb = sb;
    if (mode != BlendMode::Normal) {
        BlendFormula(sr, sg, sb, dr, dg, db, mode, br, bg, bb);
    }
    
    float outR, outG, outB, outA;
    Over(br, bg, bb, sa, dr, dg, db, da, outR, outG, outB, outA);
    
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

void BlendOperations::BlendFormula(float sr, float sg, float sb,
                                    float dr, float dg, float db,
                                    BlendMode mode,
                                    float& br, float& bg, float& bb) {
    switch (mode) {
        case BlendMode::Multiply:
            br = sr * dr;
            bg = sg * dg;
            bb = sb * db;
            break;
        case BlendMode::Screen:
            br = 1.0f - (1.0f - sr) * (1.0f - dr);
            bg = 1.0f - (1.0f - sg) * (1.0f - dg);
            bb = 1.0f - (1.0f - sb) * (1.0f - db);
            break;
        case BlendMode::Overlay: {
            auto overlay = [](float s, float d) {
                return d < 0.5f ? 2.0f * s * d : 1.0f - 2.0f * (1.0f - s) * (1.0f - d);
            };
            br = overlay(sr, dr);
            bg = overlay(sg, dg);
            bb = overlay(sb, db);
            break;
        }
        case BlendMode::Difference:
            br = std::abs(sr - dr);
            bg = std::abs(sg - dg);
            bb = std::abs(sb - db);
            break;
        case BlendMode::Add:
            br = std::min(sr + dr, 1.0f);
            bg = std::min(sg + dg, 1.0f);
            bb = std::min(sb + db, 1.0f);
            break;
        case BlendMode::Subtract:
            br = std::max(sr - dr, 0.0f);
            bg = std::max(sg - dg, 0.0f);
            bb = std::max(sb - db, 0.0f);
            break;
        case BlendMode::Darken:
            br = std::min(sr, dr);
            bg = std::min(sg, dg);
            bb = std::min(sb, db);
            break;
        case BlendMode::Lighten:
            br = std::max(sr, dr);
            bg = std::max(sg, dg);
            bb = std::max(sb, db);
            break;
        default:
            br = sr; bg = sg; bb = sb;
            break;
    }
}

void BlendOperations::Over(float bsr, float bsg, float bsb, float sa,
                            float dr, float dg, float db, float da,
                            float& outR, float& outG, float& outB, float& outA) {
    outA = sa + da * (1.0f - sa);
    if (outA < 0.001f) { outR = outG = outB = 0; return; }
    outR = (bsr * sa + dr * da * (1.0f - sa)) / outA;
    outG = (bsg * sa + dg * da * (1.0f - sa)) / outA;
    outB = (bsb * sa + db * da * (1.0f - sa)) / outA;
}

} // namespace VividPic
