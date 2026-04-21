#pragma once

#include "Utils/Types.h"
#include <vector>

namespace VividPic {
namespace Render {

struct BrushStamp {
    float x = 0.0f;
    float y = 0.0f;
    float pressure = 1.0f;
};

class BrushEngine {
public:
    static BrushEngine& GetInstance();
    
    // Brush parameters
    void SetSize(float size) { m_size = std::max(1.0f, size); }
    float GetSize() const { return m_size; }
    
    void SetOpacity(float opacity) { m_opacity = std::clamp(opacity, 0.0f, 1.0f); }
    float GetOpacity() const { return m_opacity; }
    
    void SetColor(const Color& color) { m_color = color; }
    Color GetColor() const { return m_color; }
    
    void SetSpacing(float spacing) { m_spacing = std::max(0.01f, spacing); }
    float GetSpacing() const { return m_spacing; }
    
    // Pressure curve: 0=linear, 1=S-curve, 2=hard
    void SetPressureCurve(int curve) { m_pressureCurve = curve; }
    
    // Generate stamps along a line segment
    std::vector<BrushStamp> GenerateStamps(float x1, float y1, float p1,
                                            float x2, float y2, float p2);
    
    // Apply pressure curve
    float ApplyPressureCurve(float pressure) const;
    
private:
    BrushEngine() = default;
    
    float m_size = 10.0f;
    float m_opacity = 1.0f;
    Color m_color = Color::FromHex(0x000000);
    float m_spacing = 0.25f; // spacing as fraction of brush diameter
    int m_pressureCurve = 0; // 0=linear
};

} // namespace Render
} // namespace VividPic
