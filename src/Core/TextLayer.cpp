#include "Core/TextLayer.h"
#include "Core/Render/TilePool.h"
#include "Core/Text/CoreTextEngine.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace CloverPic {

TextLayer::TextLayer(const String& name, uint32_t canvasWidth, uint32_t canvasHeight)
    : Layer(name, LayerType::Text, canvasWidth, canvasHeight) {
}

void TextLayer::EnsureCache() const {
    if (!m_cache) {
        m_cache = std::make_unique<RasterLayer>(GetName(), LayerType::Text, m_canvasWidth, m_canvasHeight);
    }
}

bool TextLayer::IsEmpty() const {
    EnsureCache();
    return m_cache->IsEmpty();
}

void TextLayer::Clear() {
    EnsureCache();
    m_cache->Clear();
    m_cacheDirty = false;
}

Color TextLayer::GetPixel(uint32_t x, uint32_t y) const {
    RasterizeIfNeeded();
    EnsureCache();
    return m_cache->GetPixel(x, y);
}

void TextLayer::SetPixel(uint32_t x, uint32_t y, const Color& color) {
    (void)x;
    (void)y;
    (void)color;
}

void TextLayer::DrawBrushStamp(float cx, float cy, float radius, const Color& color, float opacity,
                                Render::BrushTipType tipType, float flow, float wetMix,
                                const SelectionMask* mask) {
    (void)cx;
    (void)cy;
    (void)radius;
    (void)color;
    (void)opacity;
    (void)tipType;
    (void)flow;
    (void)wetMix;
    (void)mask;
}

Render::Tile* TextLayer::GetTile(uint32_t gridX, uint32_t gridY) const {
    RasterizeIfNeeded();
    EnsureCache();
    return m_cache->GetTile(gridX, gridY);
}

bool TextLayer::HasTile(uint32_t gridX, uint32_t gridY) const {
    RasterizeIfNeeded();
    EnsureCache();
    return m_cache->HasTile(gridX, gridY);
}

Ref<Layer> TextLayer::Clone() const {
    auto clone = MakeRef<TextLayer>(m_name + L" 鍓湰", m_canvasWidth, m_canvasHeight);
    clone->m_text = m_text;
    clone->m_fontFamily = m_fontFamily;
    clone->m_fontSize = m_fontSize;
    clone->m_textColor = m_textColor;
    clone->m_position = m_position;
    clone->m_rotation = m_rotation;
    clone->m_blendMode = m_blendMode;
    clone->m_opacity = m_opacity;
    clone->m_visible = m_visible;
    clone->m_locked = m_locked;
    clone->m_protectAlpha = m_protectAlpha;
    clone->m_cacheDirty = true;
    return clone;
}

void TextLayer::UpdateThumbnail() {
    RasterizeIfNeeded();
    EnsureCache();
    m_cache->UpdateThumbnail();
    m_thumbnail = m_cache->GetThumbnail();
}

void TextLayer::ImportTile(uint32_t gridX, uint32_t gridY, const uint8_t* srcData) {
    EnsureCache();
    m_cache->ImportTile(gridX, gridY, srcData);
    m_cacheDirty = false;
}

std::vector<uint8_t> TextLayer::SerializePayload() const {
    std::vector<uint8_t> payload;
    auto appendString = [&](const std::wstring& s) {
        uint32_t len = static_cast<uint32_t>(s.length());
        size_t offset = payload.size();
        payload.resize(offset + sizeof(len));
        std::memcpy(payload.data() + offset, &len, sizeof(len));
        if (len > 0) {
            offset = payload.size();
            payload.resize(offset + len * sizeof(wchar_t));
            std::memcpy(payload.data() + offset, s.c_str(), len * sizeof(wchar_t));
        }
    };
    auto appendFloat = [&](float v) {
        size_t offset = payload.size();
        payload.resize(offset + sizeof(v));
        std::memcpy(payload.data() + offset, &v, sizeof(v));
    };
    auto appendU32 = [&](uint32_t v) {
        size_t offset = payload.size();
        payload.resize(offset + sizeof(v));
        std::memcpy(payload.data() + offset, &v, sizeof(v));
    };

    appendString(m_text);
    appendString(m_fontFamily);
    appendFloat(m_fontSize);
    appendU32(m_textColor.ToRGBA());
    appendFloat(static_cast<float>(m_position.x));
    appendFloat(static_cast<float>(m_position.y));
    appendFloat(m_rotation);
    return payload;
}

void TextLayer::DeserializePayload(const uint8_t* data, size_t len) {
    if (!data || len < 8) return;

    size_t offset = 0;
    auto readString = [&](std::wstring& out) -> bool {
        if (offset + sizeof(uint32_t) > len) return false;
        uint32_t strLen = 0;
        std::memcpy(&strLen, data + offset, sizeof(strLen));
        offset += sizeof(strLen);
        if (offset + strLen * sizeof(wchar_t) > len) return false;
        if (strLen > 0) {
            out.assign(reinterpret_cast<const wchar_t*>(data + offset), strLen);
            offset += strLen * sizeof(wchar_t);
        } else {
            out.clear();
        }
        return true;
    };
    auto readFloat = [&](float& out) -> bool {
        if (offset + sizeof(float) > len) return false;
        std::memcpy(&out, data + offset, sizeof(out));
        offset += sizeof(out);
        return true;
    };
    auto readU32 = [&](uint32_t& out) -> bool {
        if (offset + sizeof(uint32_t) > len) return false;
        std::memcpy(&out, data + offset, sizeof(out));
        offset += sizeof(out);
        return true;
    };

    if (!readString(m_text)) return;
    if (!readString(m_fontFamily)) return;
    if (!readFloat(m_fontSize)) return;

    uint32_t colorRGBA = 0;
    if (!readU32(colorRGBA)) return;
    m_textColor = Color::FromRGBA(colorRGBA);

    float posX = 0.0f;
    float posY = 0.0f;
    if (!readFloat(posX)) return;
    if (!readFloat(posY)) return;
    m_position.x = static_cast<int32_t>(posX);
    m_position.y = static_cast<int32_t>(posY);
    if (!readFloat(m_rotation)) return;

    m_cacheDirty = true;
}

void TextLayer::RasterizeIfNeeded() const {
    if (m_cacheDirty) {
        Rasterize();
    }
}

void TextLayer::Rasterize() const {
    EnsureCache();
    m_cache->Clear();

    if (m_text.empty()) {
        m_cacheDirty = false;
        return;
    }

    TextRasterRequest request;
    request.text = m_text;
    request.fontFamily = m_fontFamily;
    request.fontSize = m_fontSize;
    request.color = m_textColor;
    request.maxWidth = m_canvasWidth;
    request.maxHeight = m_canvasHeight;

    RasterizedTextBitmap bitmap;
    if (!CoreText::CoreTextEngine::Get().RasterizeText(request, bitmap) || bitmap.IsEmpty()) {
        m_cacheDirty = false;
        return;
    }

    for (uint32_t y = 0; y < bitmap.height; ++y) {
        int32_t canvasY = m_position.y + static_cast<int32_t>(y);
        if (canvasY < 0 || canvasY >= static_cast<int32_t>(m_canvasHeight)) continue;

        for (uint32_t x = 0; x < bitmap.width; ++x) {
            int32_t canvasX = m_position.x + static_cast<int32_t>(x);
            if (canvasX < 0 || canvasX >= static_cast<int32_t>(m_canvasWidth)) continue;

            size_t idx = static_cast<size_t>(y) * bitmap.width * 4 + static_cast<size_t>(x) * 4;
            Color c(bitmap.rgbaPixels[idx], bitmap.rgbaPixels[idx + 1], bitmap.rgbaPixels[idx + 2],
                    bitmap.rgbaPixels[idx + 3]);
            if (c.a > 0) {
                m_cache->SetPixel(static_cast<uint32_t>(canvasX), static_cast<uint32_t>(canvasY), c);
            }
        }
    }

    m_cacheDirty = false;
}

} // namespace CloverPic
