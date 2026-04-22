#pragma once

#include "Utils/Types.h"
#include "Core/Project.h"
#include <vector>
#include <memory>

namespace VividPic {

class LayerManager;
class Layer;

// Custom binary format for .vvp project files
// Format:
//   [Magic]      char[4] = "VVP1"
//   [Version]    uint16_t = 1
//   [Reserved]   uint16_t = 0
//   [CanvasW]    uint32_t
//   [CanvasH]    uint32_t
//   [DPI]        float
//   [ColorMode]  uint8_t
//   [ActiveLayer]uint32_t
//   [LayerCount] uint32_t
//   For each layer:
//     [NameLen]      uint32_t
//     [Name]         UTF-16LE (NameLen bytes)
//     [Type]         uint8_t
//     [BlendMode]    uint8_t
//     [Opacity]      uint8_t
//     [Visible]      uint8_t
//     [Locked]       uint8_t
//     [ProtectAlpha] uint8_t
//     [GridW]        uint32_t
//     [GridH]        uint32_t
//     [TileCount]    uint32_t
//     For each tile:
//       [GridX]      uint32_t
//       [GridY]      uint32_t
//       [TileData]   uint8_t[256*256*4]

class ProjectSerializer {
public:
    // Save project with all layers to a .vvp file
    // Returns true on success
    static bool SaveProject(const String& filePath, Project* project, LayerManager* layerManager);

    // Load project from a .vvp file
    // Returns nullptr on failure. On success, LayerManager is populated.
    static Ref<Project> LoadProject(const String& filePath, LayerManager* layerManager);

    // Export flattened image to PNG using WIC
    static bool ExportPNG(const String& filePath, LayerManager* layerManager);

private:
    static bool WriteLayer(std::ostream& stream, Layer* layer);
    static Ref<Layer> ReadLayerV1(std::istream& stream, uint32_t canvasWidth, uint32_t canvasHeight);
    static Ref<Layer> ReadLayerV2(std::istream& stream, uint32_t canvasWidth, uint32_t canvasHeight);
};

} // namespace VividPic
