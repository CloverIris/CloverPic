#pragma once

#include "Utils/Types.h"
#include "Core/Layer.h"
#include <vector>
#include <string>
#include <chrono>

namespace VividPic {

enum class ColorMode {
    RGB,
    Grayscale
};

struct Canvas {
    uint32_t widthPx = 0;
    uint32_t heightPx = 0;
    float dpi = 350.0f;
    ColorMode colorMode = ColorMode::RGB;
    String rgbProfile;
    String cmykProfile;
};

// LayerType, BlendMode, and Layer class are now defined in Core/Layer.h and Core/BlendMode.h

struct ProjectMetadata {
    String name;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point modifiedAt;
    String thumbnailHash;
};

class Project {
public:
    Project(const String& name);
    
    String GetName() const { return m_metadata.name; }
    void SetName(const String& name) { m_metadata.name = name; }
    
    Canvas& GetCanvas() { return m_canvas; }
    const Canvas& GetCanvas() const { return m_canvas; }
    
    ProjectMetadata& GetMetadata() { return m_metadata; }
    
private:
    Canvas m_canvas;
    ProjectMetadata m_metadata;
};

} // namespace VividPic
