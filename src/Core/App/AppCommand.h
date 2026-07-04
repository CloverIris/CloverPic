#pragma once

#include <cstdint>

namespace CloverPic::Core {

enum class AppCommand : uint32_t {
    None,
    GoHome,
    ShowNewCanvasModal,
    CreateCanvasPreset,
    CloseModal,
    OpenProject,
    OpenRecentProject,
    FocusHomeSearch,
    SaveProject,
    SaveProjectAs,
    ExportPng,
    Undo,
    Redo,
    Quit,
    SelectTool,
    SetColor,
    AddLayer,
    DeleteLayer,
    ToggleLayerVisibility,
    SelectLayer
};

} // namespace CloverPic::Core
