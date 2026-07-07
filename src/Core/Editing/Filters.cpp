#include "Core/Editing/Filters.h"
#include "Core/Layers/Layer.h"
#include "Core/Render/TilePool.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <vector>

namespace CloverPic {

namespace {
constexpr uint32_t TileSampleCount = Render::TILE_PIXELS * Render::TILE_CHANNELS;
constexpr float Max10 = 1023.0f;

uint16_t Clamp10(float value) {
    return static_cast<uint16_t>(std::clamp(value, 0.0f, Max10));
}
}

// ------------------------------------------------------------------
// Color space helpers
// ------------------------------------------------------------------

void Filters::RGBToHSV(uint8_t r8, uint8_t g8, uint8_t b8, float& h, float& s, float& v) {
    float r = r8 / 255.0f;
    float g = g8 / 255.0f;
    float b = b8 / 255.0f;

    float maxVal = std::max({r, g, b});
    float minVal = std::min({r, g, b});
    float delta = maxVal - minVal;

    v = maxVal;

    if (delta < 0.0001f) {
        h = 0.0f;
        s = 0.0f;
        return;
    }

    s = delta / maxVal;

    if (maxVal == r) {
        h = 60.0f * (fmod(((g - b) / delta), 6.0f));
    } else if (maxVal == g) {
        h = 60.0f * (((b - r) / delta) + 2.0f);
    } else {
        h = 60.0f * (((r - g) / delta) + 4.0f);
    }

    if (h < 0.0f) h += 360.0f;
}

void Filters::HSVToRGB(float h, float s, float v, uint8_t& r8, uint8_t& g8, uint8_t& b8) {
    h = fmod(h, 360.0f);
    if (h < 0.0f) h += 360.0f;

    float c = v * s;
    float x = c * (1.0f - std::abs(fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;

    float r, g, b;
    if (h < 60.0f)       { r = c; g = x; b = 0; }
    else if (h < 120.0f) { r = x; g = c; b = 0; }
    else if (h < 180.0f) { r = 0; g = c; b = x; }
    else if (h < 240.0f) { r = 0; g = x; b = c; }
    else if (h < 300.0f) { r = x; g = 0; b = c; }
    else                 { r = c; g = 0; b = x; }

    r8 = static_cast<uint8_t>(std::clamp((r + m) * 255.0f, 0.0f, 255.0f));
    g8 = static_cast<uint8_t>(std::clamp((g + m) * 255.0f, 0.0f, 255.0f));
    b8 = static_cast<uint8_t>(std::clamp((b + m) * 255.0f, 0.0f, 255.0f));
}

// ------------------------------------------------------------------
// Gaussian kernel
// ------------------------------------------------------------------

void Filters::BuildGaussianKernel(int radius, std::vector<float>& kernel) {
    int size = 2 * radius + 1;
    kernel.resize(size);
    float sigma = std::max(radius / 2.0f, 1.0f);
    float sum = 0.0f;
    for (int i = 0; i < size; ++i) {
        float x = static_cast<float>(i - radius);
        kernel[i] = std::exp(-(x * x) / (2.0f * sigma * sigma));
        sum += kernel[i];
    }
    for (int i = 0; i < size; ++i) {
        kernel[i] /= sum;
    }
}

// ------------------------------------------------------------------
// Point operations
// ------------------------------------------------------------------

void Filters::ApplyBrightnessContrast(Layer* layer, int brightness, int contrast) {
    if (!layer) return;

    float b = brightness / 100.0f * Max10;
    float c = (contrast + 100.0f) / 100.0f;

    uint32_t gw = layer->GetGridWidth();
    uint32_t gh = layer->GetGridHeight();

    for (uint32_t gy = 0; gy < gh; ++gy) {
        for (uint32_t gx = 0; gx < gw; ++gx) {
            Render::Tile* tile = layer->GetTile(gx, gy);
            if (!tile) continue;

            for (uint32_t i = 0; i < TileSampleCount; i += Render::TILE_CHANNELS) {
                float rf = tile->data[i];
                float gf = tile->data[i + 1];
                float bf = tile->data[i + 2];
                // Apply contrast then brightness
                bf = (bf - 512.0f) * c + 512.0f + b;
                gf = (gf - 512.0f) * c + 512.0f + b;
                rf = (rf - 512.0f) * c + 512.0f + b;

                tile->data[i]     = Clamp10(rf);
                tile->data[i + 1] = Clamp10(gf);
                tile->data[i + 2] = Clamp10(bf);
                // Alpha unchanged
            }
        }
    }

    layer->MarkDirty();
}

void Filters::ApplyHueSaturation(Layer* layer, int hueShift, int saturation) {
    if (!layer) return;

    float satScale = saturation / 100.0f;

    uint32_t gw = layer->GetGridWidth();
    uint32_t gh = layer->GetGridHeight();

    for (uint32_t gy = 0; gy < gh; ++gy) {
        for (uint32_t gx = 0; gx < gw; ++gx) {
            Render::Tile* tile = layer->GetTile(gx, gy);
            if (!tile) continue;

            for (uint32_t i = 0; i < TileSampleCount; i += Render::TILE_CHANNELS) {
                uint8_t r = Color10::To8(tile->data[i]);
                uint8_t g = Color10::To8(tile->data[i + 1]);
                uint8_t b = Color10::To8(tile->data[i + 2]);
                float h, s, v;
                RGBToHSV(r, g, b, h, s, v);

                h += hueShift;
                s *= satScale;
                s = std::clamp(s, 0.0f, 1.0f);

                HSVToRGB(h, s, v, r, g, b);

                tile->data[i]     = Color10::From8(r);
                tile->data[i + 1] = Color10::From8(g);
                tile->data[i + 2] = Color10::From8(b);
                // Alpha unchanged
            }
        }
    }

    layer->MarkDirty();
}

void Filters::ApplyInvert(Layer* layer) {
    if (!layer) return;

    uint32_t gw = layer->GetGridWidth();
    uint32_t gh = layer->GetGridHeight();

    for (uint32_t gy = 0; gy < gh; ++gy) {
        for (uint32_t gx = 0; gx < gw; ++gx) {
            Render::Tile* tile = layer->GetTile(gx, gy);
            if (!tile) continue;

            for (uint32_t i = 0; i < TileSampleCount; i += Render::TILE_CHANNELS) {
                tile->data[i]     = 1023 - tile->data[i];     // R
                tile->data[i + 1] = 1023 - tile->data[i + 1]; // G
                tile->data[i + 2] = 1023 - tile->data[i + 2]; // B
                // Alpha unchanged
            }
        }
    }

    layer->MarkDirty();
}

void Filters::ApplyThreshold(Layer* layer, uint8_t threshold) {
    if (!layer) return;

    uint32_t gw = layer->GetGridWidth();
    uint32_t gh = layer->GetGridHeight();

    for (uint32_t gy = 0; gy < gh; ++gy) {
        for (uint32_t gx = 0; gx < gw; ++gx) {
            Render::Tile* tile = layer->GetTile(gx, gy);
            if (!tile) continue;

            const uint16_t threshold10 = Color10::From8(threshold);
            for (uint32_t i = 0; i < TileSampleCount; i += Render::TILE_CHANNELS) {
                uint16_t r = tile->data[i];
                uint16_t g = tile->data[i + 1];
                uint16_t b = tile->data[i + 2];

                // Luminance
                uint16_t lum = Clamp10(0.299f * r + 0.587f * g + 0.114f * b);
                uint16_t val = (lum > threshold10) ? 1023 : 0;

                tile->data[i]     = val;
                tile->data[i + 1] = val;
                tile->data[i + 2] = val;
                // Alpha unchanged
            }
        }
    }

    layer->MarkDirty();
}

// ------------------------------------------------------------------
// Gaussian Blur
// ------------------------------------------------------------------

void Filters::ApplyGaussianBlur(Layer* layer, int radius) {
    if (!layer || radius < 1) return;

    const uint32_t width = layer->GetCanvasWidth();
    const uint32_t height = layer->GetCanvasHeight();
    if (width == 0 || height == 0) return;

    radius = std::clamp(radius, 1, 24);
    std::vector<Color> source(static_cast<size_t>(width) * height);
    std::vector<Color> temp(source.size());
    std::vector<Color> result(source.size());
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            source[static_cast<size_t>(y) * width + x] = layer->GetPixel(x, y);
        }
    }

    std::vector<float> kernel;
    BuildGaussianKernel(radius, kernel);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            float r = 0, g = 0, b = 0, a = 0;
            for (int k = -radius; k <= radius; ++k) {
                const int sx = std::clamp<int>(static_cast<int>(x) + k, 0, static_cast<int>(width) - 1);
                const Color c = source[static_cast<size_t>(y) * width + static_cast<uint32_t>(sx)];
                const float w = kernel[static_cast<size_t>(k + radius)];
                r += c.r * w; g += c.g * w; b += c.b * w; a += c.a * w;
            }
            temp[static_cast<size_t>(y) * width + x] = Color(
                static_cast<uint8_t>(std::clamp(r, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(g, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(b, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(a, 0.0f, 255.0f)));
        }
    }
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            float r = 0, g = 0, b = 0, a = 0;
            for (int k = -radius; k <= radius; ++k) {
                const int sy = std::clamp<int>(static_cast<int>(y) + k, 0, static_cast<int>(height) - 1);
                const Color c = temp[static_cast<size_t>(static_cast<uint32_t>(sy)) * width + x];
                const float w = kernel[static_cast<size_t>(k + radius)];
                r += c.r * w; g += c.g * w; b += c.b * w; a += c.a * w;
            }
            result[static_cast<size_t>(y) * width + x] = Color(
                static_cast<uint8_t>(std::clamp(r, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(g, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(b, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(a, 0.0f, 255.0f)));
        }
    }

    layer->Clear();
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            layer->SetPixel(x, y, result[static_cast<size_t>(y) * width + x]);
        }
    }

    layer->MarkDirty();
}

// ------------------------------------------------------------------
// Sharpen (Unsharp Mask)
// ------------------------------------------------------------------

void Filters::ApplySharpen(Layer* layer, int amount) {
    if (!layer || amount <= 0) return;

    const uint32_t width = layer->GetCanvasWidth();
    const uint32_t height = layer->GetCanvasHeight();
    if (width == 0 || height == 0) return;

    std::vector<Color> original(static_cast<size_t>(width) * height);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            original[static_cast<size_t>(y) * width + x] = layer->GetPixel(x, y);
        }
    }
    ApplyGaussianBlur(layer, 1);

    const float amt = amount / 100.0f;
    std::vector<Color> blurred(static_cast<size_t>(width) * height);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            blurred[static_cast<size_t>(y) * width + x] = layer->GetPixel(x, y);
        }
    }
    layer->Clear();
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const size_t idx = static_cast<size_t>(y) * width + x;
            const Color o = original[idx];
            const Color bl = blurred[idx];
            Color out(
                static_cast<uint8_t>(std::clamp(o.r + (o.r - bl.r) * amt, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(o.g + (o.g - bl.g) * amt, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(o.b + (o.b - bl.b) * amt, 0.0f, 255.0f)),
                o.a);
            layer->SetPixel(x, y, out);
        }
    }

    layer->MarkDirty();
}

} // namespace CloverPic
