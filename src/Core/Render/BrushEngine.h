#pragma once

#include "Utils/Types.h"
#include "Core/Render/BrushPreset.h"
#include <vector>

namespace CloverPic {
namespace Render {

struct BrushStamp {
    float x = 0.0f;
    float y = 0.0f;
    float pressure = 1.0f;
    float tiltX = 0.0f;
    float tiltY = 0.0f;
    float rotation = 0.0f;
};

class BrushEngine {
public:
    BrushEngine() = default;

    // Brush parameters
    void SetSize(float size) { m_size = std::max(1.0f, size); }
    float GetSize() const { return m_size; }

    void SetOpacity(float opacity) { m_opacity = std::clamp(opacity, 0.0f, 1.0f); }
    float GetOpacity() const { return m_opacity; }

    void SetColor(const Color& color) { m_color = color; }
    Color GetColor() const { return m_color; }

    void SetSpacing(float spacing) { m_spacing = std::max(0.01f, spacing); }
    float GetSpacing() const { return m_spacing; }

    // Tip type
    void SetTipType(BrushTipType type) { m_tipType = type; }
    BrushTipType GetTipType() const { return m_tipType; }

    // Flow (ink concentration per stamp)
    void SetFlow(float flow) { m_flow = std::clamp(flow, 0.0f, 1.0f); }
    float GetFlow() const { return m_flow; }

    // Wet mix (color blending with existing pixels)
    void SetWetMix(float wetMix) { m_wetMix = std::clamp(wetMix, 0.0f, 1.0f); }
    float GetWetMix() const { return m_wetMix; }

    // Texture scale
    void SetTextureScale(float scale) { m_textureScale = std::max(0.1f, scale); }
    float GetTextureScale() const { return m_textureScale; }

    // Pressure curve: 0=linear, 1=S-curve, 2=hard
    void SetPressureCurve(int curve) { m_pressureCurve = curve; }
    int GetPressureCurve() const { return m_pressureCurve; }

    // Apply pressure curve
    float ApplyPressureCurve(float pressure) const;

    // Generate stamps along a line segment
    std::vector<BrushStamp> GenerateStamps(float x1, float y1, float p1,
                                            float x2, float y2, float p2);

private:
    float m_size = 10.0f;
    float m_opacity = 1.0f;
    Color m_color = Color::FromHex(0x000000);
    float m_spacing = 0.25f;
    int m_pressureCurve = 0;

    BrushTipType m_tipType = BrushTipType::RoundHard;
    float m_flow = 1.0f;
    float m_wetMix = 0.0f;
    float m_textureScale = 1.0f;
};

} // namespace Render
} // namespace CloverPic
