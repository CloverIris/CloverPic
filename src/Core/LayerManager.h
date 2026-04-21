#pragma once

#include "Core/Layer.h"
#include "Render/TilePool.h"
#include <vector>
#include <memory>

namespace VividPic {

class LayerManager {
public:
    static LayerManager& GetInstance();
    
    // Initialize with canvas dimensions
    void Initialize(uint32_t canvasWidth, uint32_t canvasHeight);
    void Shutdown();
    
    // Canvas dimensions
    uint32_t GetCanvasWidth() const { return m_canvasWidth; }
    uint32_t GetCanvasHeight() const { return m_canvasHeight; }
    
    // Layer operations
    Ref<Layer> AddLayer(const String& name, LayerType type);
    void DeleteLayer(size_t index);
    void MoveLayer(size_t fromIndex, size_t toIndex);
    Ref<Layer> GetLayer(size_t index);
    size_t GetLayerCount() const { return m_layers.size(); }
    
    // Active layer
    size_t GetActiveLayerIndex() const { return m_activeLayerIndex; }
    void SetActiveLayer(size_t index);
    Ref<Layer> GetActiveLayer();
    
    // Visibility / locking helpers
    void ToggleLayerVisibility(size_t index);
    void ToggleLayerLock(size_t index);
    
    // Compositing
    // Composites all visible layers into a flat RGBA buffer
    // Caller must provide a buffer of size width*height*4
    void CompositeToBuffer(uint8_t* outBuffer, uint32_t width, uint32_t height);
    
    // Check if compositing is needed
    bool NeedsComposite() const;
    void MarkCompositeDirty() { m_compositeDirty = true; }
    void ClearCompositeDirty() { m_compositeDirty = false; }
    
private:
    LayerManager() = default;
    
    uint32_t m_canvasWidth = 0;
    uint32_t m_canvasHeight = 0;
    std::vector<Ref<Layer>> m_layers;
    size_t m_activeLayerIndex = 0;
    bool m_compositeDirty = true;
    
    void CompositeTile(Render::Tile* outTile, uint32_t gridX, uint32_t gridY);
};

} // namespace VividPic
