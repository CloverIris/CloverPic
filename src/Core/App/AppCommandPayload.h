#pragma once

#include "Utils/Types.h"
#include <cstdint>

namespace CloverPic::Core {

constexpr uint64_t SwatchBlack = 0x111111FFull;
constexpr uint64_t SwatchRed = 0xDA3838FFull;
constexpr uint64_t SwatchBlue = 0x2A70DCFFull;
constexpr uint64_t SwatchWhite = 0xFFFFFFFFull;
constexpr uint64_t SwatchGreen = 0x2BB673FFull;
constexpr uint64_t SwatchYellow = 0xF5C542FFull;
constexpr uint64_t SwatchPurple = 0x7E57C2FFull;
constexpr uint64_t SwatchTransparent = 0x00000000ull;

enum class BrushParamId : uint16_t {
    Size,
    Opacity,
    Flow,
    Spacing,
    WetMix
};

enum class FilterCommandId : uint16_t {
    Invert,
    Threshold,
    BrightnessContrast,
    GaussianBlur,
    Sharpen,
    HueSaturation
};

enum class ViewOptionId : uint16_t {
    Grid,
    PixelGrid,
    TransparentBackground,
    ColorProfileDisplay
};

enum class WorkspacePanelId : uint16_t {
    Color,
    BrushPreview,
    BrushControl,
    BrushPresets,
    Navigator,
    Layer,
    BrushSize,
    StatusBar
};

enum class SnapModeId : uint16_t {
    Off,
    Parallel,
    Crisscross,
    VanishingPoint,
    Radial,
    Circle,
    Curve,
    CurvedLineEllipse
};

inline uint64_t PackCanvasPreset(uint32_t width, uint32_t height) {
    return (static_cast<uint64_t>(width) << 32) | height;
}

inline uint32_t PresetWidth(uint64_t packed) {
    return static_cast<uint32_t>(packed >> 32);
}

inline uint32_t PresetHeight(uint64_t packed) {
    return static_cast<uint32_t>(packed & 0xFFFFFFFFu);
}

inline uint64_t PackU16Pair(uint32_t a, uint32_t b) {
    return (static_cast<uint64_t>(a & 0xFFFFu) << 16) | static_cast<uint64_t>(b & 0xFFFFu);
}

inline uint16_t PackedFirstU16(uint64_t packed) {
    return static_cast<uint16_t>((packed >> 16) & 0xFFFFu);
}

inline uint16_t PackedSecondU16(uint64_t packed) {
    return static_cast<uint16_t>(packed & 0xFFFFu);
}

inline uint64_t PackBrushParam(BrushParamId id, uint16_t value) {
    return PackU16Pair(static_cast<uint16_t>(id), value);
}

inline BrushParamId BrushParamFromPacked(uint64_t packed) {
    return static_cast<BrushParamId>(PackedFirstU16(packed));
}

inline uint16_t BrushParamValueFromPacked(uint64_t packed) {
    return PackedSecondU16(packed);
}

inline uint64_t PackFilterCommand(FilterCommandId id, uint16_t value = 0) {
    return PackU16Pair(static_cast<uint16_t>(id), value);
}

inline FilterCommandId FilterCommandFromPacked(uint64_t packed) {
    return static_cast<FilterCommandId>(PackedFirstU16(packed));
}

inline uint16_t FilterValueFromPacked(uint64_t packed) {
    return PackedSecondU16(packed);
}

inline uint64_t PackBrushPreset(uint16_t size, uint16_t tip) {
    return PackU16Pair(size, tip);
}

inline uint16_t BrushPresetSizeFromPacked(uint64_t packed) {
    return PackedFirstU16(packed);
}

inline uint16_t BrushPresetTipFromPacked(uint64_t packed) {
    return PackedSecondU16(packed);
}

inline Color ColorFromPackedRgba(uint64_t packed) {
    return Color(
        static_cast<uint8_t>((packed >> 24) & 0xFF),
        static_cast<uint8_t>((packed >> 16) & 0xFF),
        static_cast<uint8_t>((packed >> 8) & 0xFF),
        static_cast<uint8_t>(packed & 0xFF));
}

} // namespace CloverPic::Core
