#pragma once

#include "Core/Layer.h"
#include "Core/RasterLayer.h"
#include <memory>

namespace CloverPic {

// TextLayer: stores editable text properties with a raster cache for compositing.
// The raster cache is regenerated from source data when dirty.
class TextLayer : public Layer {
public:
    TextLayer(const String& name, uint32_t canvasWidth, uint32_t canvasHeight);
    ~TextLayer() override = default;

    LayerType GetType() const override { return LayerType::Text; }

    // Source properties
    void SetText(const std::wstring& text) { m_text = text; m_cacheDirty = true; MarkDirty(); }
    const std::wstring& GetText() const { return m_text; }

    void SetFontFamily(const std::wstring& family) { m_fontFamily = family; m_cacheDirty = true; MarkDirty(); }
    const std::wstring& GetFontFamily() const { return m_fontFamily; }

    void SetFontSize(float size) { m_fontSize = size; m_cacheDirty = true; MarkDirty(); }
    float GetFontSize() const { return m_fontSize; }

    void SetTextColor(const Color& color) { m_textColor = color; m_cacheDirty = true; MarkDirty(); }
    Color GetTextColor() const { return m_textColor; }

    void SetPosition(const Point& pos) { m_position = pos; m_cacheDirty = true; MarkDirty(); }
    Point GetPosition() const { return m_position; }

    void SetRotation(float rotation) { m_rotation = rotation; m_cacheDirty = true; MarkDirty(); }
    float GetRotation() const { return m_rotation; }

    // Pixel access (delegates to cache)
    bool IsEmpty() const override;
    void Clear() override;
    Color GetPixel(uint32_t x, uint32_t y) const override;
    void SetPixel(uint32_t x, uint32_t y, const Color& color) override;

    // Brush stamp: no-op for text layers (must rasterize first)
    void DrawBrushStamp(float cx, float cy, float radius, const Color& color, float opacity,
                         Render::BrushTipType tipType, float flow, float wetMix,
                         const class SelectionMask* mask = nullptr) override;

    // Stroke undo: no-op
    void BeginStroke() override {}
    void EndStroke() override {}
    void CancelStroke() override {}
    bool IsInStroke() const override { return false; }

    // Tile access (delegates to cache)
    Render::Tile* GetTile(uint32_t gridX, uint32_t gridY) const override;
    bool HasTile(uint32_t gridX, uint32_t gridY) const override;

    Ref<Layer> Clone() const override;
    void UpdateThumbnail() override;
    void ImportTile(uint32_t gridX, uint32_t gridY, const uint8_t* srcData) override;

    // VVP v2 payload
    std::vector<uint8_t> SerializePayload() const override;
    void DeserializePayload(const uint8_t* data, size_t len) override;

    // Regenerate raster cache from source data through the core soft text engine.
    void RasterizeIfNeeded() const override;
    void Rasterize() const; // Force rasterize

private:
    std::wstring m_text;
    std::wstring m_fontFamily = L"Segoe UI";
    float m_fontSize = 24.0f;
    Color m_textColor = Color(255, 255, 255, 255);
    Point m_position = {0, 0};
    float m_rotation = 0.0f;

    mutable std::unique_ptr<RasterLayer> m_cache;
    mutable bool m_cacheDirty = true;

    void EnsureCache() const;
};

} // namespace CloverPic
