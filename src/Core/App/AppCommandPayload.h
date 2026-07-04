#pragma once

#include "Utils/Types.h"
#include <cstdint>

namespace CloverPic::Core {

constexpr uint64_t SwatchBlack = 0x111111FFull;
constexpr uint64_t SwatchRed = 0xDA3838FFull;
constexpr uint64_t SwatchBlue = 0x2A70DCFFull;

inline uint64_t PackCanvasPreset(uint32_t width, uint32_t height) {
    return (static_cast<uint64_t>(width) << 32) | height;
}

inline uint32_t PresetWidth(uint64_t packed) {
    return static_cast<uint32_t>(packed >> 32);
}

inline uint32_t PresetHeight(uint64_t packed) {
    return static_cast<uint32_t>(packed & 0xFFFFFFFFu);
}

inline Color ColorFromPackedRgba(uint64_t packed) {
    return Color(
        static_cast<uint8_t>((packed >> 24) & 0xFF),
        static_cast<uint8_t>((packed >> 16) & 0xFF),
        static_cast<uint8_t>((packed >> 8) & 0xFF),
        static_cast<uint8_t>(packed & 0xFF));
}

} // namespace CloverPic::Core
