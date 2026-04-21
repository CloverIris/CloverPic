#include "Render/BrushEngine.h"
#include <cmath>
#include <algorithm>

namespace VividPic {
namespace Render {

BrushEngine& BrushEngine::GetInstance() {
    static BrushEngine instance;
    return instance;
}

float BrushEngine::ApplyPressureCurve(float pressure) const {
    pressure = std::clamp(pressure, 0.0f, 1.0f);
    
    switch (m_pressureCurve) {
        case 1: // S-curve
            return pressure * pressure * (3.0f - 2.0f * pressure);
        case 2: // Hard
            return pressure < 0.5f ? 0.0f : 1.0f;
        case 0: // Linear
        default:
            return pressure;
    }
}

std::vector<BrushStamp> BrushEngine::GenerateStamps(float x1, float y1, float p1,
                                                       float x2, float y2, float p2) {
    std::vector<BrushStamp> stamps;
    
    float dx = x2 - x1;
    float dy = y2 - y1;
    float distance = std::sqrt(dx * dx + dy * dy);
    
    if (distance < 0.001f) {
        // Single stamp at endpoint
        stamps.push_back({ x2, y2, ApplyPressureCurve(p2) });
        return stamps;
    }
    
    float stepSize = m_size * m_spacing;
    int numSteps = static_cast<int>(distance / stepSize);
    if (numSteps < 1) numSteps = 1;
    
    float stepX = dx / numSteps;
    float stepY = dy / numSteps;
    
    for (int i = 0; i <= numSteps; ++i) {
        float t = static_cast<float>(i) / numSteps;
        float px = x1 + stepX * i;
        float py = y1 + stepY * i;
        float pressure = p1 + (p2 - p1) * t;
        
        stamps.push_back({ px, py, ApplyPressureCurve(pressure) });
    }
    
    return stamps;
}

} // namespace Render
} // namespace VividPic
