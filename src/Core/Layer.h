#pragma once

#include "Utils/Types.h"
#include "Core/BlendMode.h"
#include "Render/TilePool.h"
#include "Render/BrushPreset.h"

namespace VividPic {

enum class LayerType {
    Color,
    Grayscale,
    Transparent,
    Group,
    Text,
    Vector,
    Adjustment
};

} // namespace VividPic
#include <vector>
#include <memory>
#include <optional>

namespace VividPic {

class Layer {
public:
    Layer(const String& name, LayerType type, uint32_t canvasWidth, uint32_t canvasHeight);
    ~Layer();
    
    // Metadata
    String GetName() const { return m_name; }
    void SetName(const String& name) { m_name = name; }
    
    LayerType GetType() const { return m_type; }
    BlendMode GetBlendMode() const { return m_blendMode; }
    void SetBlendMode(BlendMode mode) { m_blendMode = mode; }
    
    uint8_t GetOpacity() const { return m_opacity; }
    void SetOpacity(uint8_t opacity) { m_opacity = opacity; }
    
    bool IsVisible() const { return m_visible; }
    void SetVisible(bool visible) { m_visible = visible; }
    
    bool IsLocked() const { return m_locked; }
    void SetLocked(bool locked) { m_locked = locked; }
    
    bool IsProtectAlpha() const { return m_protectAlpha; }
    void SetProtectAlpha(bool protect) { m_protectAlpha = protect; }
    
    // Pixel access
    bool IsEmpty() const;
    void Clear();
    
    // Read pixel at canvas coordinates (returns transparent if empty)
    Color GetPixel(uint32_t x, uint32_t y) const;
    
    // Write pixel at canvas coordinates (acquires tiles as needed)
    void SetPixel(uint32_t x, uint32_t y, const Color& color);
    
    // Draw a brush stamp with tip shape, flow and wet mix
    void DrawBrushStamp(float cx, float cy, float radius, const Color& color, float opacity,
                         Render::BrushTipType tipType, float flow, float wetMix);
    
    // Stroke-based undo support
    void BeginStroke();
    void EndStroke();
    void CancelStroke();
    bool IsInStroke() const { return m_currentUndoItem != nullptr; }
    
    // Tile access for compositing
    uint32_t GetGridWidth() const { return m_gridWidth; }
    uint32_t GetGridHeight() const { return m_gridHeight; }
    Render::Tile* GetTile(uint32_t gridX, uint32_t gridY) const;
    bool HasTile(uint32_t gridX, uint32_t gridY) const;
    
    // Canvas dimensions
    uint32_t GetCanvasWidth() const { return m_canvasWidth; }
    uint32_t GetCanvasHeight() const { return m_canvasHeight; }
    
    // Dirty flag for compositing optimization
    bool IsDirty() const { return m_dirty; }
    void SetDirty(bool dirty) { m_dirty = dirty; }
    void MarkDirty() { m_dirty = true; }
    void ClearDirty() { m_dirty = false; }
    
    // Clone this layer (Copy-on-Write: shares tile pointers)
    Ref<Layer> Clone() const;
    
    // Thumbnail generation (async in future, sync for M3)
    void UpdateThumbnail();
    const std::vector<uint8_t>& GetThumbnail() const { return m_thumbnail; }
    
private:
    String m_name;
    LayerType m_type;
    BlendMode m_blendMode = BlendMode::Normal;
    uint8_t m_opacity = 255;
    bool m_visible = true;
    bool m_locked = false;
    bool m_protectAlpha = false;
    
    uint32_t m_canvasWidth = 0;
    uint32_t m_canvasHeight = 0;
    uint32_t m_gridWidth = 0;
    uint32_t m_gridHeight = 0;
    
    // 2D grid of tile pointers [gridY][gridX]
    std::vector<std::vector<Render::Tile*>> m_tiles;
    
    // Reference count for Copy-on-Write (shared among clones)
    std::shared_ptr<uint32_t> m_tileRefCount;
    
    bool m_dirty = true;
    std::vector<uint8_t> m_thumbnail; // RGBA32 thumbnail data
    
    // Current stroke undo item (active during a drawing stroke)
    std::unique_ptr<class StrokeUndoItem> m_currentUndoItem;
    
    void InitializeGrid();
    void ReleaseAllTiles();
    Render::Tile* AcquireTile(uint32_t gridX, uint32_t gridY);
    void DetachForWrite(); // Copy-on-Write: clone tiles if shared
};

} // namespace VividPic
