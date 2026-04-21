#include "Render/TilePool.h"

namespace VividPic {
namespace Render {

TilePool& TilePool::GetInstance() {
    static TilePool instance;
    return instance;
}

bool TilePool::Initialize(size_t initialPoolSize) {
    Shutdown();
    
    m_tiles.reserve(initialPoolSize);
    for (size_t i = 0; i < initialPoolSize; ++i) {
        auto tile = std::make_unique<Tile>();
        tile->Clear();
        m_available.push(tile.get());
        m_tiles.push_back(std::move(tile));
    }
    
    return true;
}

void TilePool::Shutdown() {
    while (!m_available.empty()) {
        m_available.pop();
    }
    m_tiles.clear();
}

Tile* TilePool::AcquireTile() {
    if (m_available.empty()) {
        // Grow pool by 50%
        size_t growSize = m_tiles.empty() ? 16 : m_tiles.size() / 2;
        for (size_t i = 0; i < growSize; ++i) {
            auto tile = std::make_unique<Tile>();
            tile->Clear();
            m_available.push(tile.get());
            m_tiles.push_back(std::move(tile));
        }
    }
    
    Tile* tile = m_available.top();
    m_available.pop();
    tile->inUse = true;
    return tile;
}

void TilePool::ReleaseTile(Tile* tile) {
    if (!tile) return;
    tile->inUse = false;
    tile->Clear();
    m_available.push(tile);
}

} // namespace Render
} // namespace VividPic
