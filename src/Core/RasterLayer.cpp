#include "Core/RasterLayer.h"
#include "Core/SelectionMask.h"
#include "Core/History.h"
#include "Render/TilePool.h"
#include <cmath>
#include <cstring>
#include <algorithm>

// Simple procedural noise texture for brush tip (64x64)
static float s_noiseTexture[64 * 64];
static bool s_noiseInitialized = false;

static void InitializeNoiseTexture() {
    if (s_noiseInitialized) return;
    s_noiseInitialized = true;
    // Simple value noise using a hash function
    auto hash = [](int x, int y) -> float {
        int n = x + y * 57;
        n = (n << 13) ^ n;
        return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f) * 0.5f + 0.5f;
    };
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            s_noiseTexture[y * 64 + x] = hash(x, y);
        }
    }
}

static float SampleNoise(float u, float v) {
    u = std::fmod(std::abs(u), 1.0f) * 63.0f;
    v = std::fmod(std::abs(v), 1.0f) * 63.0f;
    int x0 = static_cast<int>(u);
    int y0 = static_cast<int>(v);
    int x1 = std::min(x0 + 1, 63);
    int y1 = std::min(y0 + 1, 63);
    float fx = u - x0;
    float fy = v - y0;
    float v00 = s_noiseTexture[y0 * 64 + x0];
    float v10 = s_noiseTexture[y0 * 64 + x1];
    float v01 = s_noiseTexture[y1 * 64 + x0];
    float v11 = s_noiseTexture[y1 * 64 + x1];
    return v00 * (1.0f - fx) * (1.0f - fy) + v10 * fx * (1.0f - fy)
         + v01 * (1.0f - fx) * fy + v11 * fx * fy;
}

static float ComputeTipFalloff(float dx, float dy, float radius, VividPic::Render::BrushTipType tipType, float textureScale) {
    float dist = std::sqrt(dx * dx + dy * dy);
    if (dist >= radius) return 0.0f;
    
    switch (tipType) {
        case VividPic::Render::BrushTipType::RoundHard: {
            float t = dist / radius;
            return 1.0f - std::pow(t, 8.0f); // Very sharp edge
        }
        case VividPic::Render::BrushTipType::RoundSoft: {
            float t = dist / radius;
            return std::cos(t * 3.14159265f * 0.5f); // Cosine falloff
        }
        case VividPic::Render::BrushTipType::Flat: {
            // Elliptical flat tip
            float major = radius;
            float minor = radius * 0.35f;
            float d = (dx * dx) / (major * major) + (dy * dy) / (minor * minor);
            if (d >= 1.0f) return 0.0f;
            return 1.0f - std::pow(d, 4.0f);
        }
        case VividPic::Render::BrushTipType::Bristle: {
            // Multiple fine lines
            float base = std::cos(dist / radius * 3.14159265f * 0.5f);
            if (base <= 0.0f) return 0.0f;
            // Add fine streaks based on angle
            float angle = std::atan2(dy, dx);
            float streak = std::sin(angle * 8.0f + dist * 0.5f);
            return base * (0.6f + 0.4f * streak);
        }
        case VividPic::Render::BrushTipType::Texture: {
            float base = std::cos(dist / radius * 3.14159265f * 0.5f);
            if (base <= 0.0f) return 0.0f;
            float nu = (dx / radius + 1.0f) * 0.5f * textureScale;
            float nv = (dy / radius + 1.0f) * 0.5f * textureScale;
            float nval = SampleNoise(nu, nv);
            return base * (0.5f + 0.5f * nval);
        }
    }
    return 0.0f;
}

namespace VividPic {

RasterLayer::RasterLayer(const String& name, LayerType type, uint32_t canvasWidth, uint32_t canvasHeight)
    : Layer(name, type, canvasWidth, canvasHeight)
    , m_tileRefCount(std::make_shared<uint32_t>(1)) {
    m_tiles.resize(m_gridHeight);
    for (auto& row : m_tiles) {
        row.resize(m_gridWidth, nullptr);
    }
}

RasterLayer::~RasterLayer() {
    ReleaseAllTiles();
}

void RasterLayer::ReleaseAllTiles() {
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

bool RasterLayer::IsEmpty() const {
    for (const auto& row : m_tiles) {
        for (const auto& tile : row) {
            if (tile != nullptr) return false;
        }
    }
    return true;
}

void RasterLayer::Clear() {
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

Color RasterLayer::GetPixel(uint32_t x, uint32_t y) const {
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

void RasterLayer::SetPixel(uint32_t x, uint32_t y, const Color& color) {
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

void RasterLayer::BeginStroke() {
    if (m_currentUndoItem) {
        CancelStroke();
    }
    m_currentUndoItem = std::make_unique<StrokeUndoItem>(this);
}

void RasterLayer::EndStroke() {
    if (m_currentUndoItem) {
        if (!m_currentUndoItem->IsEmpty()) {
            m_currentUndoItem->CaptureRedoTiles();
            HistoryManager::GetInstance().Push(std::move(m_currentUndoItem));
        }
        m_currentUndoItem.reset();
    }
}

void RasterLayer::CancelStroke() {
    m_currentUndoItem.reset();
}

void RasterLayer::DrawBrushStamp(float cx, float cy, float radius, const Color& color, float opacity,
                                  Render::BrushTipType tipType, float flow, float wetMix,
                                  const SelectionMask* mask) {
    if (m_locked) return;
    if (radius < 0.5f) return;

    InitializeNoiseTexture();

    int x0 = static_cast<int>(std::max(0.0f, cx - radius));
    int y0 = static_cast<int>(std::max(0.0f, cy - radius));
    int x1 = static_cast<int>(std::min(static_cast<float>(m_canvasWidth - 1), cx + radius));
    int y1 = static_cast<int>(std::min(static_cast<float>(m_canvasHeight - 1), cy + radius));

    DetachForWrite();

    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            // Selection mask check
            if (mask && mask->GetPixel(static_cast<uint32_t>(x), static_cast<uint32_t>(y)) == 0) continue;

            float dx = static_cast<float>(x) - cx + 0.5f;
            float dy = static_cast<float>(y) - cy + 0.5f;

            float falloff = ComputeTipFalloff(dx, dy, radius, tipType, 1.0f);
            if (falloff <= 0.0f) continue;

            float alpha = (color.a / 255.0f) * falloff * opacity * flow;
            if (alpha <= 0.01f) continue;
            if (alpha > 1.0f) alpha = 1.0f;

            uint32_t gridX = static_cast<uint32_t>(x) / Render::TILE_SIZE;
            uint32_t gridY = static_cast<uint32_t>(y) / Render::TILE_SIZE;
            uint32_t localX = static_cast<uint32_t>(x) % Render::TILE_SIZE;
            uint32_t localY = static_cast<uint32_t>(y) % Render::TILE_SIZE;

            if (gridX >= m_gridWidth || gridY >= m_gridHeight) continue;

            Render::Tile* tile = AcquireTile(gridX, gridY);
            if (!tile) continue;

            // Undo snapshot: capture tile before first modification in this stroke
            if (m_currentUndoItem && !m_currentUndoItem->HasTile(gridX, gridY)) {
                m_currentUndoItem->CaptureTile(gridX, gridY, tile->data, Render::TILE_BYTES);
            }

            uint32_t idx = (localY * Render::TILE_SIZE + localX) * 4;

            // Read destination
            Color dst(tile->data[idx + 2], tile->data[idx + 1], tile->data[idx], tile->data[idx + 3]);

            // Wet mix: blend brush color with existing pixel color
            Color src = color;
            if (wetMix > 0.0f && dst.a > 0) {
                src.r = static_cast<uint8_t>(src.r * (1.0f - wetMix) + dst.r * wetMix);
                src.g = static_cast<uint8_t>(src.g * (1.0f - wetMix) + dst.g * wetMix);
                src.b = static_cast<uint8_t>(src.b * (1.0f - wetMix) + dst.b * wetMix);
            }

            if (m_protectAlpha) {
                uint8_t oldA = tile->data[idx + 3];
                tile->data[idx + 2] = static_cast<uint8_t>(dst.r + (src.r - dst.r) * alpha);
                tile->data[idx + 1] = static_cast<uint8_t>(dst.g + (src.g - dst.g) * alpha);
                tile->data[idx]     = static_cast<uint8_t>(dst.b + (src.b - dst.b) * alpha);
                tile->data[idx + 3] = oldA;
            } else {
                // Proper alpha blend
                float sa = alpha;
                float da = dst.a / 255.0f;
                float outA = sa + da * (1.0f - sa);
                if (outA > 0.001f) {
                    tile->data[idx + 2] = static_cast<uint8_t>(std::clamp((src.r * sa + dst.r * da * (1.0f - sa)) / outA, 0.0f, 255.0f));
                    tile->data[idx + 1] = static_cast<uint8_t>(std::clamp((src.g * sa + dst.g * da * (1.0f - sa)) / outA, 0.0f, 255.0f));
                    tile->data[idx]     = static_cast<uint8_t>(std::clamp((src.b * sa + dst.b * da * (1.0f - sa)) / outA, 0.0f, 255.0f));
                    tile->data[idx + 3] = static_cast<uint8_t>(std::clamp(outA * 255.0f, 0.0f, 255.0f));
                }
            }
        }
    }
    MarkDirty();
}

Render::Tile* RasterLayer::GetTile(uint32_t gridX, uint32_t gridY) const {
    if (gridX >= m_gridWidth || gridY >= m_gridHeight) return nullptr;
    return m_tiles[gridY][gridX];
}

bool RasterLayer::HasTile(uint32_t gridX, uint32_t gridY) const {
    return GetTile(gridX, gridY) != nullptr;
}

Render::Tile* RasterLayer::AcquireTile(uint32_t gridX, uint32_t gridY) {
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

void RasterLayer::DetachForWrite() {
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

Ref<Layer> RasterLayer::Clone() const {
    auto clone = MakeRef<RasterLayer>(m_name + L" 副本", m_type, m_canvasWidth, m_canvasHeight);
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

void RasterLayer::ImportTile(uint32_t gridX, uint32_t gridY, const uint8_t* srcData) {
    if (gridX >= m_gridWidth || gridY >= m_gridHeight) return;
    DetachForWrite();
    Render::Tile* tile = AcquireTile(gridX, gridY);
    if (tile && srcData) {
        std::memcpy(tile->data, srcData, Render::TILE_BYTES);
        tile->inUse = true;
    }
}

void RasterLayer::UpdateThumbnail() {
    // M3: Simple 64x64 thumbnail from top-left of canvas
    constexpr uint32_t thumbSize = 64;
    m_thumbnail.resize(thumbSize * thumbSize * 4);
    
    uint32_t stepX = m_canvasWidth / thumbSize;
    uint32_t stepY = m_canvasHeight / thumbSize;
    if (stepX == 0) stepX = 1;
    if (stepY == 0) stepY = 1;
    
    bool hasVisible = false;
    for (uint32_t y = 0; y < thumbSize; ++y) {
        for (uint32_t x = 0; x < thumbSize; ++x) {
            Color c = GetPixel(x * stepX, y * stepY);
            uint32_t idx = (y * thumbSize + x) * 4;
            m_thumbnail[idx] = c.b;
            m_thumbnail[idx + 1] = c.g;
            m_thumbnail[idx + 2] = c.r;
            m_thumbnail[idx + 3] = c.a;
            if (c.a > 0) hasVisible = true;
        }
    }
    
    if (!hasVisible) {
        m_thumbnail.clear();
    }
}

} // namespace VividPic
