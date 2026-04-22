#include "Core/TextLayer.h"
#include "Render/RenderBackend.h"
#include "Render/TilePool.h"
#include <cstring>
#include <cmath>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <algorithm>

namespace VividPic {

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
    (void)x; (void)y; (void)color;
    // TextLayer does not support direct pixel editing (must rasterize first)
    // No-op to prevent accidental cache mutations
}

void TextLayer::DrawBrushStamp(float cx, float cy, float radius, const Color& color, float opacity,
                                Render::BrushTipType tipType, float flow, float wetMix,
                                const SelectionMask* mask) {
    (void)cx; (void)cy; (void)radius; (void)color; (void)opacity;
    (void)tipType; (void)flow; (void)wetMix; (void)mask;
    // Brush strokes are not allowed on text layers
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
    auto clone = MakeRef<TextLayer>(m_name + L" 副本", m_canvasWidth, m_canvasHeight);
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
    // Format: [textLen:uint32][text:UTF-16][fontLen:uint32][font:UTF-16][fontSize:float][color:RGBA32][posX:float][posY:float][rotation:float]
    std::vector<uint8_t> payload;
    auto appendString = [&](const std::wstring& s) {
        uint32_t len = static_cast<uint32_t>(s.length());
        payload.resize(payload.size() + sizeof(len));
        std::memcpy(payload.data() + payload.size() - sizeof(len), &len, sizeof(len));
        if (len > 0) {
            size_t offset = payload.size();
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
    appendFloat(m_position.x);
    appendFloat(m_position.y);
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
    float posX = 0.0f, posY = 0.0f;
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
    
    auto& backend = Render::RenderBackend::GetInstance();
    IDWriteFactory* dwrite = backend.GetWriteFactory();
    IWICImagingFactory* wic = backend.GetWicFactory();
    ID2D1Factory* d2dFactory = backend.GetFactory();
    if (!dwrite || !wic || !d2dFactory) {
        m_cacheDirty = false;
        return;
    }
    
    // Create text format
    IDWriteTextFormat* format = nullptr;
    HRESULT hr = dwrite->CreateTextFormat(
        m_fontFamily.c_str(),
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        m_fontSize,
        L"zh-CN",
        &format
    );
    if (FAILED(hr) || !format) {
        m_cacheDirty = false;
        return;
    }
    
    // Create text layout to measure
    IDWriteTextLayout* layout = nullptr;
    hr = dwrite->CreateTextLayout(
        m_text.c_str(),
        static_cast<uint32_t>(m_text.length()),
        format,
        static_cast<float>(m_canvasWidth),
        static_cast<float>(m_canvasHeight),
        &layout
    );
    if (FAILED(hr) || !layout) {
        format->Release();
        m_cacheDirty = false;
        return;
    }
    
    DWRITE_TEXT_METRICS metrics = {};
    hr = layout->GetMetrics(&metrics);
    if (FAILED(hr)) {
        layout->Release();
        format->Release();
        m_cacheDirty = false;
        return;
    }
    
    uint32_t bmpW = static_cast<uint32_t>(std::ceil(metrics.widthIncludingTrailingWhitespace));
    uint32_t bmpH = static_cast<uint32_t>(std::ceil(metrics.height));
    if (bmpW == 0 || bmpH == 0) {
        layout->Release();
        format->Release();
        m_cacheDirty = false;
        return;
    }
    
    if (bmpW > m_canvasWidth) bmpW = m_canvasWidth;
    if (bmpH > m_canvasHeight) bmpH = m_canvasHeight;
    
    // Create WIC bitmap
    IWICBitmap* wicBitmap = nullptr;
    hr = wic->CreateBitmap(
        bmpW, bmpH,
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapCacheOnDemand,
        &wicBitmap
    );
    if (FAILED(hr) || !wicBitmap) {
        layout->Release();
        format->Release();
        m_cacheDirty = false;
        return;
    }
    
    // Create D2D render target
    ID2D1RenderTarget* rt = nullptr;
    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
    );
    hr = d2dFactory->CreateWicBitmapRenderTarget(wicBitmap, rtProps, &rt);
    if (FAILED(hr) || !rt) {
        wicBitmap->Release();
        layout->Release();
        format->Release();
        m_cacheDirty = false;
        return;
    }
    
    // Render text
    rt->BeginDraw();
    rt->Clear(D2D1::ColorF(0, 0, 0, 0));
    
    ID2D1SolidColorBrush* brush = nullptr;
    D2D1_COLOR_F d2dColor = D2D1::ColorF(
        m_textColor.r / 255.0f,
        m_textColor.g / 255.0f,
        m_textColor.b / 255.0f,
        m_textColor.a / 255.0f
    );
    hr = rt->CreateSolidColorBrush(d2dColor, &brush);
    if (SUCCEEDED(hr) && brush) {
        rt->DrawTextLayout(
            D2D1::Point2F(0, 0),
            layout,
            brush,
            D2D1_DRAW_TEXT_OPTIONS_NONE
        );
        brush->Release();
    }
    
    rt->EndDraw();
    
    // Lock bitmap and copy pixels to tiles
    IWICBitmapLock* lock = nullptr;
    WICRect lockRect = { 0, 0, static_cast<int>(bmpW), static_cast<int>(bmpH) };
    hr = wicBitmap->Lock(&lockRect, WICBitmapLockRead, &lock);
    if (SUCCEEDED(hr) && lock) {
        uint8_t* buffer = nullptr;
        uint32_t bufferSize = 0;
        uint32_t stride = 0;
        lock->GetDataPointer(&bufferSize, &buffer);
        lock->GetStride(&stride);
        
        if (buffer && bufferSize > 0) {
            int32_t startX = m_position.x;
            int32_t startY = m_position.y;
            
            for (uint32_t y = 0; y < bmpH; ++y) {
                int32_t canvasY = startY + static_cast<int32_t>(y);
                if (canvasY < 0 || canvasY >= static_cast<int32_t>(m_canvasHeight)) continue;
                
                for (uint32_t x = 0; x < bmpW; ++x) {
                    int32_t canvasX = startX + static_cast<int32_t>(x);
                    if (canvasX < 0 || canvasX >= static_cast<int32_t>(m_canvasWidth)) continue;
                    
                    uint32_t srcIdx = y * stride + x * 4;
                    uint8_t b = buffer[srcIdx];
                    uint8_t g = buffer[srcIdx + 1];
                    uint8_t r = buffer[srcIdx + 2];
                    uint8_t a = buffer[srcIdx + 3];
                    
                    // Unpremultiply from D2D premultiplied alpha to straight alpha
                    if (a > 0 && a < 255) {
                        float alphaF = a / 255.0f;
                        r = static_cast<uint8_t>(std::min(255.0f, r / alphaF));
                        g = static_cast<uint8_t>(std::min(255.0f, g / alphaF));
                        b = static_cast<uint8_t>(std::min(255.0f, b / alphaF));
                    }
                    
                    if (a > 0) {
                        m_cache->SetPixel(static_cast<uint32_t>(canvasX), static_cast<uint32_t>(canvasY), Color(r, g, b, a));
                    }
                }
            }
        }
        
        lock->Release();
    }
    
    rt->Release();
    wicBitmap->Release();
    layout->Release();
    format->Release();
    
    m_cacheDirty = false;
}

} // namespace VividPic
