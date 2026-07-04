#pragma once

#include "Utils/Types.h"
#include "Core/LayerType.h"
#include "Core/BlendMode.h"
#include "Core/Render/TilePool.h"
#include "Core/Render/BrushPreset.h"

#include <vector>
#include <memory>

namespace CloverPic {

class Layer {
public:
    virtual ~Layer() = default;

    // Type
    virtual LayerType GetType() const = 0;

    // Metadata
    String GetName() const { return m_name; }
    void SetName(const String& name) { m_name = name; }

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
    virtual bool IsEmpty() const = 0;
    virtual void Clear() = 0;
    virtual Color GetPixel(uint32_t x, uint32_t y) const = 0;
    virtual void SetPixel(uint32_t x, uint32_t y, const Color& color) = 0;

    // Brush stamp
    virtual void DrawBrushStamp(float cx, float cy, float radius, const Color& color, float opacity,
                                 Render::BrushTipType tipType, float flow, float wetMix,
                                 const class SelectionMask* mask = nullptr) = 0;

    // Stroke-based undo support
    virtual void BeginStroke() {}
    virtual void EndStroke() {}
    virtual void CancelStroke() {}
    virtual bool IsInStroke() const { return false; }

    // Tile access for compositing
    uint32_t GetGridWidth() const { return m_gridWidth; }
    uint32_t GetGridHeight() const { return m_gridHeight; }
    virtual Render::Tile* GetTile(uint32_t gridX, uint32_t gridY) const = 0;
    virtual bool HasTile(uint32_t gridX, uint32_t gridY) const = 0;

    // Canvas dimensions
    uint32_t GetCanvasWidth() const { return m_canvasWidth; }
    uint32_t GetCanvasHeight() const { return m_canvasHeight; }

    // Dirty flag for compositing optimization
    bool IsDirty() const { return m_dirty; }
    void SetDirty(bool dirty) { m_dirty = dirty; }
    void MarkDirty() { m_dirty = true; }
    void ClearDirty() { m_dirty = false; }

    // Clone this layer
    virtual Ref<Layer> Clone() const = 0;

    // Thumbnail generation
    virtual void UpdateThumbnail() = 0;
    const std::vector<uint8_t>& GetThumbnail() const { return m_thumbnail; }

    // Direct tile import for deserialization
    virtual void ImportTile(uint32_t gridX, uint32_t gridY, const uint8_t* srcData) = 0;

    // Extensible source payload for text/vector layer resources in .cloverpic.
    virtual std::vector<uint8_t> SerializePayload() const { return {}; }
    virtual void DeserializePayload(const uint8_t*, size_t) {}

    // For text/vector layers: regenerate raster cache from source data
    virtual void RasterizeIfNeeded() const {}

protected:
    Layer(const String& name, LayerType type, uint32_t canvasWidth, uint32_t canvasHeight);

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

    bool m_dirty = true;
    std::vector<uint8_t> m_thumbnail; // RGBA32 thumbnail data

    void InitializeGrid();
};

} // namespace CloverPic
