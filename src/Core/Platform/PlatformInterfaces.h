#pragma once

#include "Utils/Types.h"
#include <cstdint>
#include <vector>

namespace CloverPic::Core {

enum class SurfacePixelFormat {
    Bgra8Unorm
};

struct RgbaFrame {
    uint32_t width = 0;
    uint32_t height = 0;
    SurfacePixelFormat format = SurfacePixelFormat::Bgra8Unorm;
    std::vector<uint8_t> pixels;

    void Resize(uint32_t newWidth, uint32_t newHeight) {
        width = newWidth;
        height = newHeight;
        pixels.assign(static_cast<size_t>(width) * height * 4, 0);
    }

    bool IsEmpty() const {
        return width == 0 || height == 0 || pixels.empty();
    }
};

class ISurfacePresenter {
public:
    virtual ~ISurfacePresenter() = default;
    virtual void Present(const RgbaFrame& frame, const std::vector<Rect>& dirtyRects) = 0;
};

class IPlatformHost {
public:
    virtual ~IPlatformHost() = default;
    virtual Size GetViewportSize() const = 0;
    virtual float GetDpiScale() const = 0;
    virtual void RequestFrame() = 0;
    virtual void RequestQuit() = 0;
};

} // namespace CloverPic::Core
