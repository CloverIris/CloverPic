#include "Core/Layers/LayerManager.h"
#include "Core/Layers/RasterLayer.h"
#include "Core/Layers/TextLayer.h"
#include "Core/Render/TilePool.h"
#include <algorithm>

namespace CloverPic {

void LayerManager::Initialize(uint32_t canvasWidth, uint32_t canvasHeight) {
    Shutdown();
    m_canvasWidth = canvasWidth;
    m_canvasHeight = canvasHeight;
    m_soloLayerIndex = static_cast<size_t>(-1);
    m_compositeDirty = true;
    
    // Ensure tile pool is ready
    auto& pool = Render::TilePool::GetInstance();
    if (pool.GetTotalTiles() == 0) {
        pool.Initialize(256);
    }
}

void LayerManager::Shutdown() {
    m_layers.clear();
    m_activeLayerIndex = 0;
    m_soloLayerIndex = static_cast<size_t>(-1);
    m_canvasWidth = 0;
    m_canvasHeight = 0;
    m_compositeDirty = true;
}

Ref<Layer> LayerManager::AddLayer(const String& name, LayerType type) {
    Ref<Layer> layer;
    if (type == LayerType::Text) {
        layer = MakeRef<TextLayer>(name, m_canvasWidth, m_canvasHeight);
    } else {
        layer = MakeRef<RasterLayer>(name, type, m_canvasWidth, m_canvasHeight);
    }
    if (auto rasterLayer = std::dynamic_pointer_cast<RasterLayer>(layer)) {
        rasterLayer->SetHistoryManager(m_historyManager);
    }
    m_layers.push_back(layer);
    m_activeLayerIndex = m_layers.size() - 1;
    m_compositeDirty = true;
    return layer;
}

void LayerManager::AddLayer(Ref<Layer> layer) {
    if (!layer) return;
    if (auto rasterLayer = std::dynamic_pointer_cast<RasterLayer>(layer)) {
        rasterLayer->SetHistoryManager(m_historyManager);
    }
    m_layers.push_back(layer);
    m_activeLayerIndex = m_layers.size() - 1;
    m_compositeDirty = true;
}

void LayerManager::DeleteLayer(size_t index) {
    if (index >= m_layers.size()) return;
    m_layers.erase(m_layers.begin() + index);
    if (m_activeLayerIndex >= m_layers.size() && m_layers.size() > 0) {
        m_activeLayerIndex = m_layers.size() - 1;
    }
    m_compositeDirty = true;
}

void LayerManager::DuplicateLayer(size_t index) {
    if (index >= m_layers.size()) return;
    auto clone = m_layers[index]->Clone();
    clone->SetName(m_layers[index]->GetName() + L" 拷贝");
    m_layers.insert(m_layers.begin() + index, clone);
    m_activeLayerIndex = index;
    m_compositeDirty = true;
}

void LayerManager::MergeDown(size_t index) {
    if (index == 0 || index >= m_layers.size()) return;
    auto upper = m_layers[index];
    auto lower = m_layers[index - 1];
    
    uint32_t gridW = upper->GetGridWidth();
    uint32_t gridH = upper->GetGridHeight();
    float opacity = upper->GetOpacity() / 255.0f;
    BlendMode mode = upper->GetBlendMode();
    
    for (uint32_t gy = 0; gy < gridH; ++gy) {
        for (uint32_t gx = 0; gx < gridW; ++gx) {
            Render::Tile* upperTile = upper->GetTile(gx, gy);
            if (!upperTile) continue;
            
            Render::Tile* lowerTile = lower->GetTile(gx, gy);
            if (!lowerTile) {
                // Create a transparent tile for merging
                lower->SetPixel(gx * Render::TILE_SIZE, gy * Render::TILE_SIZE, Color(0,0,0,0));
                lowerTile = lower->GetTile(gx, gy);
                if (!lowerTile) continue;
            }
            
            for (uint32_t ty = 0; ty < Render::TILE_SIZE; ++ty) {
                for (uint32_t tx = 0; tx < Render::TILE_SIZE; ++tx) {
                    uint32_t idx = (ty * Render::TILE_SIZE + tx) * Render::TILE_CHANNELS;
                    Color src = Color10(upperTile->data[idx], upperTile->data[idx + 1],
                                        upperTile->data[idx + 2], upperTile->data[idx + 3]).ToColor();
                    if (src.a == 0) continue;
                    
                    Color dst = Color10(lowerTile->data[idx], lowerTile->data[idx + 1],
                                        lowerTile->data[idx + 2], lowerTile->data[idx + 3]).ToColor();
                    
                    Color blended = BlendOperations::BlendPixel(src, dst, mode, opacity);
                    Color10 blended10 = Color10::FromColor(blended);
                    lowerTile->data[idx] = blended10.r;
                    lowerTile->data[idx + 1] = blended10.g;
                    lowerTile->data[idx + 2] = blended10.b;
                    lowerTile->data[idx + 3] = blended10.a;
                }
            }
        }
    }
    
    // Delete upper layer
    m_layers.erase(m_layers.begin() + index);
    if (m_activeLayerIndex >= m_layers.size() && m_layers.size() > 0) {
        m_activeLayerIndex = m_layers.size() - 1;
    }
    m_compositeDirty = true;
}

void LayerManager::MoveLayer(size_t fromIndex, size_t toIndex) {
    if (fromIndex >= m_layers.size() || toIndex >= m_layers.size() || fromIndex == toIndex) return;
    auto layer = m_layers[fromIndex];
    m_layers.erase(m_layers.begin() + fromIndex);
    m_layers.insert(m_layers.begin() + toIndex, layer);
    if (m_activeLayerIndex == fromIndex) {
        m_activeLayerIndex = toIndex;
    } else if (fromIndex < m_activeLayerIndex && toIndex >= m_activeLayerIndex) {
        --m_activeLayerIndex;
    } else if (fromIndex > m_activeLayerIndex && toIndex <= m_activeLayerIndex) {
        ++m_activeLayerIndex;
    }
    if (m_soloLayerIndex == fromIndex) {
        m_soloLayerIndex = toIndex;
    } else if (m_soloLayerIndex != static_cast<size_t>(-1)) {
        if (fromIndex < m_soloLayerIndex && toIndex >= m_soloLayerIndex) {
            --m_soloLayerIndex;
        } else if (fromIndex > m_soloLayerIndex && toIndex <= m_soloLayerIndex) {
            ++m_soloLayerIndex;
        }
    }
    m_compositeDirty = true;
}

Ref<Layer> LayerManager::GetLayer(size_t index) {
    if (index >= m_layers.size()) return nullptr;
    return m_layers[index];
}

void LayerManager::SetActiveLayer(size_t index) {
    if (index < m_layers.size()) {
        m_activeLayerIndex = index;
    }
}

Ref<Layer> LayerManager::GetActiveLayer() {
    if (m_activeLayerIndex >= m_layers.size()) return nullptr;
    return m_layers[m_activeLayerIndex];
}

void LayerManager::ToggleLayerVisibility(size_t index) {
    auto layer = GetLayer(index);
    if (layer) {
        layer->SetVisible(!layer->IsVisible());
        m_compositeDirty = true;
    }
}

void LayerManager::ToggleLayerLock(size_t index) {
    auto layer = GetLayer(index);
    if (layer) {
        layer->SetLocked(!layer->IsLocked());
    }
}

void LayerManager::ToggleSolo(size_t index) {
    if (index >= m_layers.size()) return;
    if (m_soloLayerIndex == index) {
        m_soloLayerIndex = static_cast<size_t>(-1);
    } else {
        m_soloLayerIndex = index;
    }
    m_compositeDirty = true;
}

bool LayerManager::NeedsComposite() const {
    if (m_compositeDirty) return true;
    for (const auto& layer : m_layers) {
        if (layer->IsVisible() && layer->IsDirty()) return true;
    }
    return false;
}

void LayerManager::CompositeToBuffer(uint8_t* outBuffer, uint32_t width, uint32_t height) {
    if (!outBuffer || width == 0 || height == 0) return;
    
    // Clear buffer to transparent
    size_t totalPixels = static_cast<size_t>(width) * height * 4;
    std::fill(outBuffer, outBuffer + totalPixels, 0);
    
    // Composite each visible layer (bottom to top)
    for (size_t li = 0; li < m_layers.size(); ++li) {
        auto& layer = m_layers[li];
        if (!layer->IsVisible()) continue;
        if (m_soloLayerIndex != static_cast<size_t>(-1) && li != m_soloLayerIndex) continue;
        
        float opacity = layer->GetOpacity() / 255.0f;
        BlendMode mode = layer->GetBlendMode();
        
        uint32_t gridW = layer->GetGridWidth();
        uint32_t gridH = layer->GetGridHeight();
        
        for (uint32_t gy = 0; gy < gridH; ++gy) {
            for (uint32_t gx = 0; gx < gridW; ++gx) {
                Render::Tile* tile = layer->GetTile(gx, gy);
                if (!tile) continue;
                
                uint32_t tileBaseX = gx * Render::TILE_SIZE;
                uint32_t tileBaseY = gy * Render::TILE_SIZE;
                
                for (uint32_t ty = 0; ty < Render::TILE_SIZE; ++ty) {
                    for (uint32_t tx = 0; tx < Render::TILE_SIZE; ++tx) {
                        uint32_t canvasX = tileBaseX + tx;
                        uint32_t canvasY = tileBaseY + ty;
                        
                        if (canvasX >= width || canvasY >= height) continue;
                        if (canvasX >= m_canvasWidth || canvasY >= m_canvasHeight) continue;
                        
                        uint32_t tileIdx = (ty * Render::TILE_SIZE + tx) * Render::TILE_CHANNELS;
                        Color src = Color10(tile->data[tileIdx], tile->data[tileIdx + 1],
                                            tile->data[tileIdx + 2], tile->data[tileIdx + 3]).ToColor();
                        
                        if (src.a == 0) continue;
                        
                        uint32_t bufIdx = (canvasY * width + canvasX) * 4;
                        Color dst(outBuffer[bufIdx + 2], outBuffer[bufIdx + 1], 
                                  outBuffer[bufIdx], outBuffer[bufIdx + 3]);
                        
                        Color blended = BlendOperations::BlendPixel(src, dst, mode, opacity);
                        
                        outBuffer[bufIdx] = blended.b;
                        outBuffer[bufIdx + 1] = blended.g;
                        outBuffer[bufIdx + 2] = blended.r;
                        outBuffer[bufIdx + 3] = blended.a;
                    }
                }
            }
        }
        
        layer->ClearDirty();
    }
    
    m_compositeDirty = false;
}

} // namespace CloverPic
