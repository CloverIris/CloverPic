#pragma once

#include "Utils/Types.h"
#include <vector>
#include <memory>
#include <stack>

namespace CloverPic {
namespace Render {

constexpr uint32_t TILE_SIZE = 256;
constexpr uint32_t TILE_CHANNELS = 4;
constexpr uint32_t TILE_PIXELS = TILE_SIZE * TILE_SIZE;
constexpr uint32_t TILE_BYTES = TILE_PIXELS * TILE_CHANNELS * sizeof(uint16_t); // RGBA10 stored in uint16 lanes

struct Tile {
    alignas(32) uint16_t data[TILE_PIXELS * TILE_CHANNELS];
    bool inUse = false;
    
    void Clear(uint16_t r = 0, uint16_t g = 0, uint16_t b = 0, uint16_t a = 0) {
        for (uint32_t i = 0; i < TILE_PIXELS * TILE_CHANNELS; i += TILE_CHANNELS) {
            data[i] = r;
            data[i + 1] = g;
            data[i + 2] = b;
            data[i + 3] = a;
        }
    }
};

class TilePool {
public:
    static TilePool& GetInstance();
    
    bool Initialize(size_t initialPoolSize = 64);
    void Shutdown();
    
    Tile* AcquireTile();
    void ReleaseTile(Tile* tile);
    
    size_t GetTotalTiles() const { return m_tiles.size(); }
    size_t GetAvailableTiles() const { return m_available.size(); }
    size_t GetUsedTiles() const { return m_tiles.size() - m_available.size(); }
    
private:
    TilePool() = default;
    
    std::vector<std::unique_ptr<Tile>> m_tiles;
    std::stack<Tile*> m_available;
};

} // namespace Render
} // namespace CloverPic
