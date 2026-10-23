#pragma once

#include "Core/App/WorkspaceEditorFacade.h"
#include "Core/UI/Workspace/WorkspaceUiState.h"
#include "Utils/Types.h"

#include <vector>

namespace CloverPic::Core {

/// One row of the layer list, as the UI needs it.
struct WorkspaceLayerRow {
    String name;
    bool visible = true;
    bool locked = false;
    bool active = false;
    uint8_t opacity = 255;
};

/// Everything the workspace chrome displays, in one value.
///
/// This is the "view model" half of MVVM: it is rebuilt from core state and never
/// holds a window, a surface or a rectangle. The presenter diffs successive
/// snapshots and pushes only what changed, which is why the UI can be refreshed
/// every frame without cost.
struct WorkspaceSnapshot {
    String documentName = L"untitled.cloverpic";
    String statusText = L"READY";
    String activeTool = L"BRUSH";
    int zoomPercent = 100;
    uint32_t canvasWidth = 0;
    uint32_t canvasHeight = 0;
    float brushSize = 1.0f;
    float brushOpacity = 1.0f;
    float brushFlow = 1.0f;
    float brushSpacing = 0.25f;
    std::vector<WorkspaceLayerRow> layers;
    size_t activeLayer = 0;
    bool hasProject = false;
    /// Blend mode names in `BlendMode` order; the popup index *is* the enum value.
    std::vector<String> blendModes;
    uint32_t blendIndex = 0;
};

/// Blend mode names in enum order.
///
/// The popup shows these names, and the selected index is applied as the enum
/// value directly, so a hard-coded list can no longer disagree with the model.
const std::vector<String>& BlendModeNames();

/// Collects a snapshot from the live editor state.
///
/// Deliberately free of any platform or framework type: this is the layer that
/// makes the UI reproducible, testable and free of `Render()` calls.
WorkspaceSnapshot CaptureWorkspaceSnapshot(const WorkspaceEditorFacade& editor,
                                           const WorkspaceUiState& ui,
                                           const String& documentPath);

/// Human-readable tool name used by the status bar.
String ToolDisplayName(ToolType tool);

/// A layer's display label, matching the old UI ("LAYER 1", custom names kept).
String LayerDisplayName(const String& name, size_t index);

} // namespace CloverPic::Core
