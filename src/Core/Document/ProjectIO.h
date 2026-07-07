#pragma once

#include "Utils/Types.h"
#include "Core/Document/Project.h"
#include <iosfwd>
#include <vector>
#include <memory>

namespace CloverPic {

class LayerManager;
class Layer;

// Core-owned CloverPic v1 chunked project container (.cloverpic).
// Platform adapters provide only file bytes and image codec services.

class ProjectSerializer {
public:
    // Save project with all layers to a .cloverpic file
    // Returns true on success
    static bool SaveProject(const String& filePath, Project* project, LayerManager* layerManager);

    // Load project from a .cloverpic file
    // Returns nullptr on failure. On success, LayerManager is populated.
    static Ref<Project> LoadProject(const String& filePath, LayerManager* layerManager);

    // Export flattened image through the platform storage adapter
    static bool ExportPNG(const String& filePath, LayerManager* layerManager);

private:
    static bool WriteLayer(std::ostream& stream, Layer* layer);
    static Ref<Layer> ReadLayer(std::istream& stream, uint32_t canvasWidth, uint32_t canvasHeight);
};

} // namespace CloverPic
