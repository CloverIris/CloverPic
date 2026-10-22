#pragma once

#include <cstdint>
#include <vector>

namespace CloverPic::Platform::Mac::Png {

// The .cloverpic container stores raster layers as 16 bit PNG carrying
// RGBA10 data, tagged with `validBits=10`. CoreGraphics bitmap contexts only
// support premultiplied alpha, which destroys the precision of those values
// (a 10 bit channel premultiplied against a small alpha loses most of its
// bits). The adapter therefore owns a small, explicit PNG codec so project
// bytes round-trip exactly, mirroring what WIC does on Windows.

// Encodes straight (non-premultiplied) BGRA8 display pixels as an 8 bit RGBA
// PNG. Used for image export.
bool EncodeBgra8(uint32_t width, uint32_t height,
                 const std::vector<uint8_t>& bgraPixels, std::vector<uint8_t>& outBytes);

// Encodes straight RGBA16 pixels as a 16 bit RGBA PNG.
bool EncodeRgba16(uint32_t width, uint32_t height,
                  const std::vector<uint16_t>& rgba16Pixels, std::vector<uint8_t>& outBytes);

// Decodes a PNG into straight RGBA16 pixels. Supports non-interlaced 8/16 bit
// grayscale, grayscale+alpha, RGB and RGBA images, which covers what WIC can
// produce for CloverPic resources.
bool DecodeRgba16(const std::vector<uint8_t>& bytes, uint32_t& outWidth, uint32_t& outHeight,
                  std::vector<uint16_t>& outRgba16Pixels);

} // namespace CloverPic::Platform::Mac::Png
