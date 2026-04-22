#pragma once

#include "Core/Layer.h"
#include "Core/History.h"
#include "Render/BrushPreset.h"

namespace VividPic {

class RasterLayer : public Layer {
public:
    RasterLayer(const String& name, LayerType type, uint32_t canvasWidth, uint32_t canvasHeight);
    ~RasterLayer() override;

    LayerType GetType() const override { return m_type; }

    // Pixel access
    bool IsEmpty() const override;
    void Clear() override;
    Color GetPixel(uint32_t x, uint32_t y) const override;
    void SetPixel(uint32_t x, uint32_t y, const Color& color) override;

    // Brush stamp
    void DrawBrushStamp(float cx, float cy, float radius, const Color& color, float opacity,
                         Render::BrushTipType tipType, float flow, float wetMix,
                         const class SelectionMask* mask = nullptr) override;

    // Stroke-based undo support
    void BeginStroke() override;
    void EndStroke() override;
    void CancelStroke() override;
    bool IsInStroke() const override { return m_currentUndoItem != nullptr; }

    // Tile access for compositing
    Render::Tile* GetTile(uint32_t gridX, uint32_t gridY) const override;
    bool HasTile(uint32_t gridX, uint32_t gridY) const override;

    // Clone this layer (Copy-on-Write: shares tile pointers)
    Ref<Layer> Clone() const override;

    // Thumbnail generation
    void UpdateThumbnail() override;

    // Direct tile import for deserialization (bypasses alpha blending)
    void ImportTile(uint32_t gridX, uint32_t gridY, const uint8_t* srcData) override;

private:
    // 2D grid of tile pointers [gridY][gridX]
    std::vector<std::vector<Render::Tile*>> m_tiles;

    // Reference count for Copy-on-Write (shared among clones)
    std::shared_ptr<uint32_t> m_tileRefCount;

    // Current stroke undo item (active during a drawing stroke)
    std::unique_ptr<StrokeUndoItem> m_currentUndoItem;

    void ReleaseAllTiles();
    Render::Tile* AcquireTile(uint32_t gridX, uint32_t gridY);
    void DetachForWrite(); // Copy-on-Write: clone tiles if shared
};

} // namespace VividPic
