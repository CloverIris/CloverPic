#pragma once

#include "Utils/Types.h"
#include "Core/Project.h"
#include <iosfwd>
#include <vector>
#include <memory>

namespace CloverPic {

class LayerManager;
class Layer;

// Custom platform-independent VVP v3 binary format.
// Platform adapters provide only file bytes; core owns the project schema.

class ProjectSerializer {
public:
    // Save project with all layers to a .vvp file
    // Returns true on success
    static bool SaveProject(const String& filePath, Project* project, LayerManager* layerManager);

    // Load project from a .vvp file
    // Returns nullptr on failure. On success, LayerManager is populated.
    static Ref<Project> LoadProject(const String& filePath, LayerManager* layerManager);

    // Export flattened image through the platform storage adapter
    static bool ExportPNG(const String& filePath, LayerManager* layerManager);

private:
    static bool WriteLayer(std::ostream& stream, Layer* layer);
    static Ref<Layer> ReadLayer(std::istream& stream, uint32_t canvasWidth, uint32_t canvasHeight);
};

} // namespace CloverPic
