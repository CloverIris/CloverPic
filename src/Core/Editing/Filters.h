#pragma once

#include <cstdint>
#include <vector>

namespace CloverPic {

class Layer;

class Filters {
public:
    // Point operations (per-pixel, no neighbor access needed)
    static void ApplyBrightnessContrast(Layer* layer, int brightness, int contrast);
    static void ApplyHueSaturation(Layer* layer, int hueShift, int saturation);
    static void ApplyInvert(Layer* layer);
    static void ApplyThreshold(Layer* layer, uint8_t threshold);

    // Convolution operations (neighbor access via padded temp buffer)
    static void ApplyGaussianBlur(Layer* layer, int radius);
    static void ApplySharpen(Layer* layer, int amount);

private:
    static void RGBToHSV(uint8_t r, uint8_t g, uint8_t b, float& h, float& s, float& v);
    static void HSVToRGB(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b);
    static void BuildGaussianKernel(int radius, std::vector<float>& kernel);
};

} // namespace CloverPic
