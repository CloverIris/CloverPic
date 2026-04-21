#include "Core/Layer.h"
#include "Render/TilePool.h"
#include <cmath>
#include <cstring>

namespace VividPic {

Layer::Layer(const String& name, LayerType type, uint32_t canvasWidth, uint32_t canvasHeight)
    : m_name(name), m_type(type), m_canvasWidth(canvasWidth), m_canvasHeight(canvasHeight)
    , m_tileRefCount(std::make_shared<uint32_t>(1)) {
    InitializeGrid();
}

Layer::~Layer() {
    ReleaseAllTiles();
}

void Layer::InitializeGrid() {
    m_gridWidth = (m_canvasWidth + Render::TILE_SIZE - 1) / Render::TILE_SIZE;
    m_gridHeight = (m_canvasHeight + Render::TILE_SIZE - 1) / Render::TILE_SIZE;
    
    m_tiles.resize(m_gridHeight);
    for (auto& row : m_tiles) {
        row.resize(m_gridWidth, nullptr);
    }
}

void Layer::ReleaseAllTiles() {
    if (!m_tileRefCount || *m_tileRefCount == 0) return;
    
    (*m_tileRefCount)--;
    if (*m_tileRefCount == 0) {
        auto& pool = Render::TilePool::GetInstance();
        for (auto& row : m_tiles) {
            for (auto& tile : row) {
                if (tile) {
                    pool.ReleaseTile(tile);
                    tile = nullptr;
                }
            }
        }
    }
}

bool Layer::IsEmpty() const {
    for (const auto& row : m_tiles) {
        for (const auto& tile : row) {
            if (tile != nullptr) return false;
        }
    }
    return true;
}

void Layer::Clear() {
    DetachForWrite();
    for (auto& row : m_tiles) {
        for (auto& tile : row) {
            if (tile) {
                tile->Clear(0, 0, 0, 0);
            }
        }
    }
    MarkDirty();
}

Color Layer::GetPixel(uint32_t x, uint32_t y) const {
    if (x >= m_canvasWidth || y >= m_canvasHeight) return Color(0, 0, 0, 0);
    
    uint32_t gridX = x / Render::TILE_SIZE;
    uint32_t gridY = y / Render::TILE_SIZE;
    uint32_t localX = x % Render::TILE_SIZE;
    uint32_t localY = y % Render::TILE_SIZE;
    
    if (gridX >= m_gridWidth || gridY >= m_gridHeight) return Color(0, 0, 0, 0);
    
    Render::Tile* tile = m_tiles[gridY][gridX];
    if (!tile) return Color(0, 0, 0, 0);
    
    uint32_t idx = (localY * Render::TILE_SIZE + localX) * 4;
    return Color(tile->data[idx + 2], tile->data[idx + 1], tile->data[idx], tile->data[idx + 3]);
}

void Layer::SetPixel(uint32_t x, uint32_t y, const Color& color) {
    if (x >= m_canvasWidth || y >= m_canvasHeight) return;
    if (m_locked) return;
    
    uint32_t gridX = x / Render::TILE_SIZE;
    uint32_t gridY = y / Render::TILE_SIZE;
    uint32_t localX = x % Render::TILE_SIZE;
    uint32_t localY = y % Render::TILE_SIZE;
    
    if (gridX >= m_gridWidth || gridY >= m_gridHeight) return;
    
    DetachForWrite();
    Render::Tile* tile = AcquireTile(gridX, gridY);
    if (!tile) return;
    
    uint32_t idx = (localY * Render::TILE_SIZE + localX) * 4;
    
    if (m_protectAlpha) {
        // Only change RGB, preserve existing alpha
        uint8_t oldA = tile->data[idx + 3];
        tile->data[idx + 2] = color.r;
        tile->data[idx + 1] = color.g;
        tile->data[idx] = color.b;
        tile->data[idx + 3] = oldA;
    } else {
        // Alpha blend
        Color dst(tile->data[idx + 2], tile->data[idx + 1], tile->data[idx], tile->data[idx + 3]);
        Color blended = BlendOperations::AlphaBlend(color, dst, 1.0f);
        tile->data[idx + 2] = blended.r;
        tile->data[idx + 1] = blended.g;
        tile->data[idx] = blended.b;
        tile->data[idx + 3] = blended.a;
    }
    
    MarkDirty();
}

void Layer::DrawBrushStamp(float cx, float cy, float radius, const Color& color, float opacity) {
    if (m_locked) return;
    if (radius < 0.5f) return;
    
    int x0 = static_cast<int>(std::max(0.0f, cx - radius));
    int y0 = static_cast<int>(std::max(0.0f, cy - radius));
    int x1 = static_cast<int>(std::min(static_cast<float>(m_canvasWidth - 1), cx + radius));
    int y1 = static_cast<int>(std::min(static_cast<float>(m_canvasHeight - 1), cy + radius));
    
    float radiusSq = radius * radius;
    
    DetachForWrite();
    
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            float dx = static_cast<float>(x) - cx + 0.5f;
            float dy = static_cast<float>(y) - cy + 0.5f;
            float distSq = dx * dx + dy * dy;
            
            if (distSq <= radiusSq) {
                float falloff = 1.0f - std::sqrt(distSq) / radius;
                falloff = std::clamp(falloff * 2.0f, 0.0f, 1.0f); // Sharper edge
                
                Color stampColor = color;
                stampColor.a = static_cast<uint8_t>(stampColor.a * falloff * opacity);
                
                SetPixel(static_cast<uint32_t>(x), static_cast<uint32_t>(y), stampColor);
            }
        }
    }
}

Render::Tile* Layer::GetTile(uint32_t gridX, uint32_t gridY) const {
    if (gridX >= m_gridWidth || gridY >= m_gridHeight) return nullptr;
    return m_tiles[gridY][gridX];
}

bool Layer::HasTile(uint32_t gridX, uint32_t gridY) const {
    return GetTile(gridX, gridY) != nullptr;
}

Render::Tile* Layer::AcquireTile(uint32_t gridX, uint32_t gridY) {
    if (gridX >= m_gridWidth || gridY >= m_gridHeight) return nullptr;
    
    Render::Tile*& tile = m_tiles[gridY][gridX];
    if (!tile) {
        auto& pool = Render::TilePool::GetInstance();
        tile = pool.AcquireTile();
        if (tile) {
            tile->Clear(0, 0, 0, 0);
        }
    }
    return tile;
}

void Layer::DetachForWrite() {
    if (!m_tileRefCount || *m_tileRefCount <= 1) return;
    
    // We are shared, need to clone all tiles
    auto& pool = Render::TilePool::GetInstance();
    
    for (auto& row : m_tiles) {
        for (auto& tile : row) {
            if (tile) {
                Render::Tile* newTile = pool.AcquireTile();
                if (newTile) {
                    std::memcpy(newTile->data, tile->data, Render::TILE_BYTES);
                    newTile->inUse = true;
                }
                tile = newTile;
            }
        }
    }
    
    (*m_tileRefCount)--;
    m_tileRefCount = std::make_shared<uint32_t>(1);
}

Ref<Layer> Layer::Clone() const {
    auto clone = MakeRef<Layer>(m_name + L" 副本", m_type, m_canvasWidth, m_canvasHeight);
    clone->m_blendMode = m_blendMode;
    clone->m_opacity = m_opacity;
    clone->m_visible = m_visible;
    clone->m_locked = m_locked;
    clone->m_protectAlpha = m_protectAlpha;
    
    // Share tile pointers (Copy-on-Write)
    clone->m_tiles = m_tiles;
    clone->m_tileRefCount = m_tileRefCount;
    (*clone->m_tileRefCount)++;
    
    return clone;
}

void Layer::UpdateThumbnail() {
    // M3: Simple 64x64 thumbnail from top-left of canvas
    constexpr uint32_t thumbSize = 64;
    m_thumbnail.resize(thumbSize * thumbSize * 4);
    
    uint32_t stepX = m_canvasWidth / thumbSize;
    uint32_t stepY = m_canvasHeight / thumbSize;
    if (stepX == 0) stepX = 1;
    if (stepY == 0) stepY = 1;
    
    for (uint32_t y = 0; y < thumbSize; ++y) {
        for (uint32_t x = 0; x < thumbSize; ++x) {
            Color c = GetPixel(x * stepX, y * stepY);
            uint32_t idx = (y * thumbSize + x) * 4;
            m_thumbnail[idx] = c.b;
            m_thumbnail[idx + 1] = c.g;
            m_thumbnail[idx + 2] = c.r;
            m_thumbnail[idx + 3] = c.a;
        }
    }
}

} // namespace VividPic
