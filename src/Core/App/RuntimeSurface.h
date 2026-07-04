#pragma once

#include "Core/Input/InputEvents.h"
#include "Core/LayerType.h"
#include "Core/Platform/PlatformInterfaces.h"
#include "Utils/Types.h"
#include <vector>

namespace CloverPic::Core {

enum class WorkspaceLaunchKind {
    None,
    NewCanvas,
    OpenProject
};

struct WorkspaceLaunchRequest {
    WorkspaceLaunchKind kind = WorkspaceLaunchKind::None;
    uint32_t width = 1600;
    uint32_t height = 1000;
    float dpi = 350.0f;
    bool transparent = true;
    LayerType initialLayerType = LayerType::Transparent;
    Color backgroundColor = Color(255, 255, 255, 255);
    String rgbProfilePath;
    String cmykProfilePath;
    std::vector<uint8_t> rgbProfileBytes;
    std::vector<uint8_t> cmykProfileBytes;
    String projectPath;
};

class RuntimeSurface {
public:
    virtual ~RuntimeSurface() = default;

    virtual bool Initialize() = 0;
    virtual void Resize(uint32_t width, uint32_t height, float dpiScale) = 0;
    virtual const RgbaFrame& Render(uint64_t nowMs, std::vector<Rect>& outDirtyRects) = 0;
    virtual void HandlePointer(const Input::PointerEvent& event) = 0;
    virtual void HandleKey(const Input::KeyEvent& event) = 0;
    virtual void HandleWheel(int delta, const Point& position) = 0;
    virtual bool NeedsFrame(uint64_t nowMs) const = 0;
    virtual bool WantsQuit() const = 0;
    virtual bool IsWindowDragRegion(const Point&) { return false; }
};

} // namespace CloverPic::Core
