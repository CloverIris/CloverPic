#include "Core/SelectionMask.h"
#include "Core/Layer.h"
#include <algorithm>
#include <cmath>
#include <stack>

namespace VividPic {

SelectionMask::SelectionMask(uint32_t canvasWidth, uint32_t canvasHeight)
    : m_canvasWidth(canvasWidth), m_canvasHeight(canvasHeight) {
    m_gridWidth = (canvasWidth + TILE_MASK) >> TILE_SHIFT;
    m_gridHeight = (canvasHeight + TILE_MASK) >> TILE_SHIFT;
    m_tiles.resize(m_gridHeight);
    for (auto& row : m_tiles) {
        row.resize(m_gridWidth, nullptr);
    }
}

SelectionMask::~SelectionMask() {
    ReleaseAllTiles();
}

void SelectionMask::ReleaseAllTiles() {
    for (auto& row : m_tiles) {
        for (auto* tile : row) {
            delete tile;
        }
        row.clear();
    }
    m_tiles.clear();
}

SelectionMask::MaskTile* SelectionMask::AcquireTile(uint32_t gridX, uint32_t gridY) {
    if (gridX >= m_gridWidth || gridY >= m_gridHeight) return nullptr;
    auto*& slot = m_tiles[gridY][gridX];
    if (!slot) {
        slot = new MaskTile();
        slot->data.resize(TILE_SIZE * TILE_SIZE, 0);
    }
    return slot;
}

SelectionMask::MaskTile* SelectionMask::GetTile(uint32_t gridX, uint32_t gridY) const {
    if (gridY >= m_tiles.size()) return nullptr;
    if (gridX >= m_tiles[gridY].size()) return nullptr;
    return m_tiles[gridY][gridX];
}

uint8_t SelectionMask::GetPixel(uint32_t x, uint32_t y) const {
    if (x >= m_canvasWidth || y >= m_canvasHeight) return 0;
    uint32_t gx = x >> TILE_SHIFT;
    uint32_t gy = y >> TILE_SHIFT;
    auto* tile = GetTile(gx, gy);
    if (!tile) return 0;
    uint32_t tx = x & TILE_MASK;
    uint32_t ty = y & TILE_MASK;
    return tile->data[ty * TILE_SIZE + tx];
}

void SelectionMask::SetPixel(uint32_t x, uint32_t y, uint8_t value) {
    if (x >= m_canvasWidth || y >= m_canvasHeight) return;
    uint32_t gx = x >> TILE_SHIFT;
    uint32_t gy = y >> TILE_SHIFT;
    auto* tile = AcquireTile(gx, gy);
    if (!tile) return;
    uint32_t tx = x & TILE_MASK;
    uint32_t ty = y & TILE_MASK;
    tile->data[ty * TILE_SIZE + tx] = value;
}

void SelectionMask::Clear() {
    for (auto& row : m_tiles) {
        for (auto* tile : row) {
            if (tile) {
                std::fill(tile->data.begin(), tile->data.end(), 0);
            }
        }
    }
}

void SelectionMask::Invert() {
    for (uint32_t gy = 0; gy < m_gridHeight; ++gy) {
        for (uint32_t gx = 0; gx < m_gridWidth; ++gx) {
            auto* tile = AcquireTile(gx, gy); // ensure tile exists
            if (!tile) continue;
            for (size_t i = 0; i < tile->data.size(); ++i) {
                tile->data[i] = static_cast<uint8_t>(255 - tile->data[i]);
            }
        }
    }
}

bool SelectionMask::IsEmpty() const {
    for (const auto& row : m_tiles) {
        for (const auto* tile : row) {
            if (tile) {
                for (uint8_t v : tile->data) {
                    if (v > 0) return false;
                }
            }
        }
    }
    return true;
}

void SelectionMask::FillRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t value) {
    uint32_t x1 = std::min(x, m_canvasWidth);
    uint32_t y1 = std::min(y, m_canvasHeight);
    uint32_t x2 = std::min(x + w, m_canvasWidth);
    uint32_t y2 = std::min(y + h, m_canvasHeight);
    if (x1 >= x2 || y1 >= y2) return;

    uint32_t gxStart = x1 >> TILE_SHIFT;
    uint32_t gyStart = y1 >> TILE_SHIFT;
    uint32_t gxEnd   = (x2 - 1) >> TILE_SHIFT;
    uint32_t gyEnd   = (y2 - 1) >> TILE_SHIFT;

    for (uint32_t gy = gyStart; gy <= gyEnd; ++gy) {
        uint32_t tyStart = (gy == gyStart) ? (y1 & TILE_MASK) : 0;
        uint32_t tyEnd   = (gy == gyEnd)   ? ((y2 - 1) & TILE_MASK) : TILE_MASK;
        for (uint32_t gx = gxStart; gx <= gxEnd; ++gx) {
            auto* tile = AcquireTile(gx, gy);
            if (!tile) continue;
            uint32_t txStart = (gx == gxStart) ? (x1 & TILE_MASK) : 0;
            uint32_t txEnd   = (gx == gxEnd)   ? ((x2 - 1) & TILE_MASK) : TILE_MASK;
            for (uint32_t ty = tyStart; ty <= tyEnd; ++ty) {
                size_t base = ty * TILE_SIZE;
                for (uint32_t tx = txStart; tx <= txEnd; ++tx) {
                    tile->data[base + tx] = value;
                }
            }
        }
    }
}

void SelectionMask::FillEllipse(uint32_t cx, uint32_t cy, uint32_t rx, uint32_t ry, uint8_t value) {
    if (rx == 0 || ry == 0) return;
    uint32_t x1 = (cx > rx) ? (cx - rx) : 0;
    uint32_t y1 = (cy > ry) ? (cy - ry) : 0;
    uint32_t x2 = std::min(cx + rx + 1, m_canvasWidth);
    uint32_t y2 = std::min(cy + ry + 1, m_canvasHeight);

    float rxSq = static_cast<float>(rx * rx);
    float rySq = static_cast<float>(ry * ry);

    for (uint32_t y = y1; y < y2; ++y) {
        float dy = static_cast<float>(y) - static_cast<float>(cy);
        float dySq = dy * dy;
        for (uint32_t x = x1; x < x2; ++x) {
            float dx = static_cast<float>(x) - static_cast<float>(cx);
            float dxSq = dx * dx;
            if (dxSq / rxSq + dySq / rySq <= 1.0f) {
                SetPixel(x, y, value);
            }
        }
    }
}

// Simple scanline polygon fill (even-odd rule)
static void ScanlineFill(int y, const std::vector<std::pair<float, float>>& xIntersections,
                         std::function<void(int x1, int x2)> fillRow) {
    std::vector<float> xs;
    xs.reserve(xIntersections.size());
    for (auto [y0, x] : xIntersections) {
        if (static_cast<int>(y0) == y) xs.push_back(x);
    }
    std::sort(xs.begin(), xs.end());
    for (size_t i = 0; i + 1 < xs.size(); i += 2) {
        int x1 = static_cast<int>(std::ceil(xs[i]));
        int x2 = static_cast<int>(std::floor(xs[i + 1]));
        if (x2 >= x1) fillRow(x1, x2);
    }
}

void SelectionMask::FillPolygon(const std::vector<Point>& points, uint8_t value) {
    if (points.size() < 3) return;

    // Compute bounding box
    int minX = points[0].x, maxX = points[0].x;
    int minY = points[0].y, maxY = points[0].y;
    for (const auto& p : points) {
        minX = std::min(minX, static_cast<int>(p.x));
        maxX = std::max(maxX, static_cast<int>(p.x));
        minY = std::min(minY, static_cast<int>(p.y));
        maxY = std::max(maxY, static_cast<int>(p.y));
    }
    minX = std::max(minX, 0);
    minY = std::max(minY, 0);
    maxX = std::min(maxX, static_cast<int>(m_canvasWidth) - 1);
    maxY = std::min(maxY, static_cast<int>(m_canvasHeight) - 1);
    if (minX > maxX || minY > maxY) return;

    // For each scanline, find intersections with all edges
    for (int y = minY; y <= maxY; ++y) {
        std::vector<float> intersections;
        size_t n = points.size();
        for (size_t i = 0; i < n; ++i) {
            const auto& a = points[i];
            const auto& b = points[(i + 1) % n];
            // Check if edge straddles this y
            int ay = static_cast<int>(a.y);
            int by = static_cast<int>(b.y);
            if ((ay <= y && by > y) || (by <= y && ay > y)) {
                // Interpolate x at y
                float t = (static_cast<float>(y) - a.y) / (b.y - a.y);
                float x = a.x + t * (b.x - a.x);
                intersections.push_back(x);
            }
        }
        if (intersections.size() < 2) continue;
        std::sort(intersections.begin(), intersections.end());
        for (size_t i = 0; i + 1 < intersections.size(); i += 2) {
            int x1 = static_cast<int>(std::ceil(intersections[i]));
            int x2 = static_cast<int>(std::floor(intersections[i + 1]));
            x1 = std::max(x1, minX);
            x2 = std::min(x2, maxX);
            for (int x = x1; x <= x2; ++x) {
                SetPixel(static_cast<uint32_t>(x), static_cast<uint32_t>(y), value);
            }
        }
    }
}

void SelectionMask::FloodFill(uint32_t seedX, uint32_t seedY, const Layer* sourceLayer,
                              int tolerance, uint8_t value) {
    if (!sourceLayer || seedX >= m_canvasWidth || seedY >= m_canvasHeight) return;

    Color target = sourceLayer->GetPixel(seedX, seedY);

    std::vector<bool> visited(static_cast<size_t>(m_canvasWidth) * m_canvasHeight, false);
    auto idx = [&](uint32_t x, uint32_t y) -> size_t {
        return static_cast<size_t>(y) * m_canvasWidth + x;
    };

    auto match = [&](const Color& c) -> bool {
        return std::abs(static_cast<int>(c.r) - static_cast<int>(target.r)) <= tolerance &&
               std::abs(static_cast<int>(c.g) - static_cast<int>(target.g)) <= tolerance &&
               std::abs(static_cast<int>(c.b) - static_cast<int>(target.b)) <= tolerance &&
               std::abs(static_cast<int>(c.a) - static_cast<int>(target.a)) <= tolerance;
    };

    std::vector<std::pair<uint32_t, uint32_t>> stack;
    stack.reserve(1024);
    stack.push_back({seedX, seedY});
    visited[idx(seedX, seedY)] = true;

    while (!stack.empty()) {
        auto [cx, cy] = stack.back();
        stack.pop_back();
        SetPixel(cx, cy, value);

        auto push = [&](uint32_t nx, uint32_t ny) {
            if (nx >= m_canvasWidth || ny >= m_canvasHeight) return;
            size_t i = idx(nx, ny);
            if (visited[i]) return;
            Color c = sourceLayer->GetPixel(nx, ny);
            if (!match(c)) return;
            visited[i] = true;
            stack.push_back({nx, ny});
        };

        if (cx + 1 < m_canvasWidth) push(cx + 1, cy);
        if (cx > 0) push(cx - 1, cy);
        if (cy + 1 < m_canvasHeight) push(cx, cy + 1);
        if (cy > 0) push(cx, cy - 1);
    }
}

void SelectionMask::GetBoundaryPixels(std::vector<std::pair<int, int>>& outPixels, int step) const {
    outPixels.clear();
    if (IsEmpty()) return;

    for (uint32_t y = 0; y < m_canvasHeight; y += step) {
        for (uint32_t x = 0; x < m_canvasWidth; x += step) {
            uint8_t v = GetPixel(x, y);
            if (v == 0) continue;
            // Check if any neighbor is unselected (boundary)
            bool boundary = false;
            if (x == 0 || GetPixel(x - 1, y) == 0) boundary = true;
            if (x + 1 >= m_canvasWidth || GetPixel(x + 1, y) == 0) boundary = true;
            if (y == 0 || GetPixel(x, y - 1) == 0) boundary = true;
            if (y + 1 >= m_canvasHeight || GetPixel(x, y + 1) == 0) boundary = true;
            if (boundary) {
                outPixels.push_back({static_cast<int>(x), static_cast<int>(y)});
            }
        }
    }
}

bool SelectionMask::MaskTile::IsEmpty() const {
    for (uint8_t v : data) {
        if (v > 0) return false;
    }
    return true;
}

bool SelectionMask::MaskTile::IsFull() const {
    for (uint8_t v : data) {
        if (v < 255) return false;
    }
    return true;
}

} // namespace VividPic
