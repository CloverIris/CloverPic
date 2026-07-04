#pragma once

#include "Utils/Types.h"
#include <vector>

namespace CloverPic {
namespace Render {

enum class BrushTipType {
    RoundHard,
    RoundSoft,
    Flat,
    Bristle,
    Texture
};

struct BrushPreset {
    String name;
    BrushTipType tipType = BrushTipType::RoundHard;
    float size = 10.0f;
    float opacity = 1.0f;
    float flow = 1.0f;
    float spacing = 0.25f;
    float wetMix = 0.0f;
    int pressureCurve = 0; // 0=linear, 1=S, 2=hard
    Color color = Color::FromHex(0x000000);
    float textureScale = 1.0f;
};

} // namespace Render
} // namespace CloverPic
