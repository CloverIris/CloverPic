#pragma once

#include "Utils/Types.h"
#include <vector>
#include <memory>

namespace CloverPic {

// -------------------------------------------------------------------------
// SelectionMask — per-pixel 0..255 mask, stored in sparse 256x256 tiles.
// Used by all selection tools and as a constraint for brush/fill/gradient.
// -------------------------------------------------------------------------

class SelectionMask {
public:
    SelectionMask(uint32_t canvasWidth, uint32_t canvasHeight);
    ~SelectionMask();

    uint32_t GetWidth() const { return m_canvasWidth; }
    uint32_t GetHeight() const { return m_canvasHeight; }

    // Get mask value at pixel (0 = unselected, 255 = fully selected)
    uint8_t GetPixel(uint32_t x, uint32_t y) const;

    // Set mask value
    void SetPixel(uint32_t x, uint32_t y, uint8_t value);

    // Clear entire mask
    void Clear();

    // Invert mask
    void Invert();

    // Fill a rectangle (hard edge, no feather)
    void FillRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t value = 255);

    // Fill an axis-aligned ellipse
    void FillEllipse(uint32_t cx, uint32_t cy, uint32_t rx, uint32_t ry, uint8_t value = 255);

    // Fill a polygon (scanline algorithm). Points are canvas coords.
    void FillPolygon(const std::vector<Point>& points, uint8_t value = 255);

    // Flood fill from seed point with color tolerance (MagicWand)
    // If sourceLayer is nullptr, uses current composite (not implemented yet, pass layer)
    void FloodFill(uint32_t seedX, uint32_t seedY, const class Layer* sourceLayer,
                   int tolerance, uint8_t value = 255);

    // True if any pixel is selected
    bool IsEmpty() const;

    // For marching-ants rendering: iterate edge pixels
    // Returns a list of (x,y) coordinates that lie on the boundary of the selection
    void GetBoundaryPixels(std::vector<std::pair<int, int>>& outPixels, int step = 2) const;

private:
    struct MaskTile {
        std::vector<uint8_t> data; // 256*256 bytes, row-major
        bool IsEmpty() const;
        bool IsFull() const;
    };

    uint32_t m_canvasWidth = 0;
    uint32_t m_canvasHeight = 0;
    uint32_t m_gridWidth = 0;
    uint32_t m_gridHeight = 0;

    // Sparse grid: nullptr = all zeros
    std::vector<std::vector<MaskTile*>> m_tiles;

    MaskTile* AcquireTile(uint32_t gridX, uint32_t gridY);
    MaskTile* GetTile(uint32_t gridX, uint32_t gridY) const;
    void ReleaseAllTiles();

    static constexpr uint32_t TILE_SHIFT = 8;
    static constexpr uint32_t TILE_SIZE = 1u << TILE_SHIFT; // 256
    static constexpr uint32_t TILE_MASK = TILE_SIZE - 1;
};

} // namespace CloverPic
