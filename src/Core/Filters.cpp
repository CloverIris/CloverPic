#include "Core/Filters.h"
#include "Core/Layer.h"
#include "Render/TilePool.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <vector>

namespace VividPic {

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

    float b = brightness / 100.0f * 255.0f;
    float c = (contrast + 100.0f) / 100.0f;

    uint32_t gw = layer->GetGridWidth();
    uint32_t gh = layer->GetGridHeight();

    for (uint32_t gy = 0; gy < gh; ++gy) {
        for (uint32_t gx = 0; gx < gw; ++gx) {
            Render::Tile* tile = layer->GetTile(gx, gy);
            if (!tile) continue;

            for (uint32_t i = 0; i < Render::TILE_BYTES; i += 4) {
                float bf = tile->data[i];
                float gf = tile->data[i + 1];
                float rf = tile->data[i + 2];
                float a = tile->data[i + 3];

                // Apply contrast then brightness
                bf = (bf - 128.0f) * c + 128.0f + b;
                gf = (gf - 128.0f) * c + 128.0f + b;
                rf = (rf - 128.0f) * c + 128.0f + b;

                tile->data[i]     = static_cast<uint8_t>(std::clamp(bf, 0.0f, 255.0f));
                tile->data[i + 1] = static_cast<uint8_t>(std::clamp(gf, 0.0f, 255.0f));
                tile->data[i + 2] = static_cast<uint8_t>(std::clamp(rf, 0.0f, 255.0f));
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

            for (uint32_t i = 0; i < Render::TILE_BYTES; i += 4) {
                uint8_t b = tile->data[i];
                uint8_t g = tile->data[i + 1];
                uint8_t r = tile->data[i + 2];
                uint8_t a = tile->data[i + 3];

                float h, s, v;
                RGBToHSV(r, g, b, h, s, v);

                h += hueShift;
                s *= satScale;
                s = std::clamp(s, 0.0f, 1.0f);

                HSVToRGB(h, s, v, r, g, b);

                tile->data[i]     = b;
                tile->data[i + 1] = g;
                tile->data[i + 2] = r;
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

            for (uint32_t i = 0; i < Render::TILE_BYTES; i += 4) {
                tile->data[i]     = 255 - tile->data[i];     // B
                tile->data[i + 1] = 255 - tile->data[i + 1]; // G
                tile->data[i + 2] = 255 - tile->data[i + 2]; // R
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

            for (uint32_t i = 0; i < Render::TILE_BYTES; i += 4) {
                uint8_t b = tile->data[i];
                uint8_t g = tile->data[i + 1];
                uint8_t r = tile->data[i + 2];

                // Luminance
                uint8_t lum = static_cast<uint8_t>(0.299f * r + 0.587f * g + 0.114f * b);
                uint8_t val = (lum > threshold) ? 255 : 0;

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
// Convolution helpers
// ------------------------------------------------------------------

// Fill a padded buffer from layer pixels (clamp-to-edge for boundaries)
static void FillPaddedBuffer(Layer* layer, uint32_t tileGX, uint32_t tileGY,
                             int radius, uint8_t* padded, int paddedSize) {
    uint32_t canvasW = layer->GetCanvasWidth();
    uint32_t canvasH = layer->GetCanvasHeight();

    for (int py = 0; py < paddedSize; ++py) {
        for (int px = 0; px < paddedSize; ++px) {
            int canvasX = static_cast<int>(tileGX * Render::TILE_SIZE) + px - radius;
            int canvasY = static_cast<int>(tileGY * Render::TILE_SIZE) + py - radius;

            // Clamp to canvas bounds
            if (canvasX < 0) canvasX = 0;
            if (canvasY < 0) canvasY = 0;
            if (canvasX >= static_cast<int>(canvasW)) canvasX = static_cast<int>(canvasW) - 1;
            if (canvasY >= static_cast<int>(canvasH)) canvasY = static_cast<int>(canvasH) - 1;

            Color c = layer->GetPixel(static_cast<uint32_t>(canvasX), static_cast<uint32_t>(canvasY));
            int idx = (py * paddedSize + px) * 4;
            padded[idx]     = c.b;
            padded[idx + 1] = c.g;
            padded[idx + 2] = c.r;
            padded[idx + 3] = c.a;
        }
    }
}

// Separable Gaussian blur on a padded buffer, writes result back to center region
static void GaussianBlurPadded(const uint8_t* padded, uint8_t* temp, int paddedSize,
                               int radius, const std::vector<float>& kernel,
                               uint8_t* outTile) {
    // Horizontal pass: padded -> temp
    for (int y = 0; y < paddedSize; ++y) {
        for (int x = radius; x < paddedSize - radius; ++x) {
            float b = 0, g = 0, r = 0;
            for (int k = -radius; k <= radius; ++k) {
                int idx = (y * paddedSize + (x + k)) * 4;
                float w = kernel[k + radius];
                b += padded[idx] * w;
                g += padded[idx + 1] * w;
                r += padded[idx + 2] * w;
            }
            int outIdx = (y * paddedSize + x) * 4;
            temp[outIdx]     = static_cast<uint8_t>(std::clamp(b, 0.0f, 255.0f));
            temp[outIdx + 1] = static_cast<uint8_t>(std::clamp(g, 0.0f, 255.0f));
            temp[outIdx + 2] = static_cast<uint8_t>(std::clamp(r, 0.0f, 255.0f));
            temp[outIdx + 3] = padded[outIdx + 3]; // Alpha unchanged
        }
    }

    // Vertical pass: temp -> outTile (center region only)
    for (int ty = 0; ty < static_cast<int>(Render::TILE_SIZE); ++ty) {
        for (int tx = 0; tx < static_cast<int>(Render::TILE_SIZE); ++tx) {
            float b = 0, g = 0, r = 0;
            int srcY = ty + radius;
            int srcX = tx + radius;
            for (int k = -radius; k <= radius; ++k) {
                int idx = ((srcY + k) * paddedSize + srcX) * 4;
                float w = kernel[k + radius];
                b += temp[idx] * w;
                g += temp[idx + 1] * w;
                r += temp[idx + 2] * w;
            }
            int outIdx = (ty * Render::TILE_SIZE + tx) * 4;
            outTile[outIdx]     = static_cast<uint8_t>(std::clamp(b, 0.0f, 255.0f));
            outTile[outIdx + 1] = static_cast<uint8_t>(std::clamp(g, 0.0f, 255.0f));
            outTile[outIdx + 2] = static_cast<uint8_t>(std::clamp(r, 0.0f, 255.0f));
            // Alpha unchanged
        }
    }
}

// ------------------------------------------------------------------
// Gaussian Blur
// ------------------------------------------------------------------

void Filters::ApplyGaussianBlur(Layer* layer, int radius) {
    if (!layer || radius < 1) return;

    uint32_t gw = layer->GetGridWidth();
    uint32_t gh = layer->GetGridHeight();

    std::vector<float> kernel;
    BuildGaussianKernel(radius, kernel);

    int paddedSize = static_cast<int>(Render::TILE_SIZE) + 2 * radius;
    size_t paddedBytes = static_cast<size_t>(paddedSize) * paddedSize * 4;
    std::vector<uint8_t> padded(paddedBytes);
    std::vector<uint8_t> temp(paddedBytes);
    std::vector<uint8_t> result(Render::TILE_BYTES);

    for (uint32_t gy = 0; gy < gh; ++gy) {
        for (uint32_t gx = 0; gx < gw; ++gx) {
            Render::Tile* tile = layer->GetTile(gx, gy);
            if (!tile) continue;

            FillPaddedBuffer(layer, gx, gy, radius, padded.data(), paddedSize);
            GaussianBlurPadded(padded.data(), temp.data(), paddedSize, radius, kernel, result.data());

            // Copy result back to tile
            std::memcpy(tile->data, result.data(), Render::TILE_BYTES);
        }
    }

    layer->MarkDirty();
}

// ------------------------------------------------------------------
// Sharpen (Unsharp Mask)
// ------------------------------------------------------------------

void Filters::ApplySharpen(Layer* layer, int amount) {
    if (!layer || amount <= 0) return;

    uint32_t gw = layer->GetGridWidth();
    uint32_t gh = layer->GetGridHeight();

    int radius = 1;
    std::vector<float> kernel;
    BuildGaussianKernel(radius, kernel);

    int paddedSize = static_cast<int>(Render::TILE_SIZE) + 2 * radius;
    size_t paddedBytes = static_cast<size_t>(paddedSize) * paddedSize * 4;
    std::vector<uint8_t> padded(paddedBytes);
    std::vector<uint8_t> temp(paddedBytes);
    std::vector<uint8_t> blurred(Render::TILE_BYTES);

    float amt = amount / 100.0f;

    for (uint32_t gy = 0; gy < gh; ++gy) {
        for (uint32_t gx = 0; gx < gw; ++gx) {
            Render::Tile* tile = layer->GetTile(gx, gy);
            if (!tile) continue;

            // Get original
            uint8_t original[Render::TILE_BYTES];
            std::memcpy(original, tile->data, Render::TILE_BYTES);

            // Blur
            FillPaddedBuffer(layer, gx, gy, radius, padded.data(), paddedSize);
            GaussianBlurPadded(padded.data(), temp.data(), paddedSize, radius, kernel, blurred.data());

            // Unsharp mask: result = original + (original - blurred) * amount
            for (uint32_t i = 0; i < Render::TILE_BYTES; i += 4) {
                float bOrig = original[i];
                float gOrig = original[i + 1];
                float rOrig = original[i + 2];

                float bBlur = blurred[i];
                float gBlur = blurred[i + 1];
                float rBlur = blurred[i + 2];

                float bResult = bOrig + (bOrig - bBlur) * amt;
                float gResult = gOrig + (gOrig - gBlur) * amt;
                float rResult = rOrig + (rOrig - rBlur) * amt;

                tile->data[i]     = static_cast<uint8_t>(std::clamp(bResult, 0.0f, 255.0f));
                tile->data[i + 1] = static_cast<uint8_t>(std::clamp(gResult, 0.0f, 255.0f));
                tile->data[i + 2] = static_cast<uint8_t>(std::clamp(rResult, 0.0f, 255.0f));
                // Alpha unchanged
            }
        }
    }

    layer->MarkDirty();
}

} // namespace VividPic
