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
    /// When set, `pixels` is unused and the frame draws into this memory instead.
    ///
    /// A presentation layer that owns its own surface must not pay for a copy per
    /// frame, so the renderer can be pointed straight at that surface's buffer.
    uint8_t* borrowed = nullptr;

    void Resize(uint32_t newWidth, uint32_t newHeight) {
        width = newWidth;
        height = newHeight;
        borrowed = nullptr;
        pixels.assign(static_cast<size_t>(width) * height * 4, 0);
    }

    /// A frame that draws into memory owned by someone else.
    static RgbaFrame FromBorrowed(uint8_t* data, uint32_t w, uint32_t h) {
        RgbaFrame frame;
        frame.width = w;
        frame.height = h;
        frame.borrowed = data;
        return frame;
    }

    uint8_t* MutableData() { return borrowed ? borrowed : pixels.data(); }
    const uint8_t* Data() const { return borrowed ? borrowed : pixels.data(); }

    bool IsEmpty() const {
        if (width == 0 || height == 0) {
            return true;
        }
        return borrowed ? false : pixels.empty();
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
