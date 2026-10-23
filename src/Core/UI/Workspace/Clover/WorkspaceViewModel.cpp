#include "Core/UI/Workspace/Clover/WorkspaceViewModel.h"

namespace CloverPic::Core {
namespace {

String FileNameOf(const String& path) {
    if (path.empty()) {
        return L"untitled.cloverpic";
    }
    const size_t slash = path.find_last_of(L"\\/");
    return slash == String::npos ? path : path.substr(slash + 1);
}

} // namespace

String ToolDisplayName(ToolType tool) {
    switch (tool) {
    case ToolType::Brush: return L"BRUSH";
    case ToolType::Eraser: return L"ERASER";
    case ToolType::Fill: return L"FILL";
    case ToolType::Eyedropper: return L"EYEDROPPER";
    case ToolType::Gradient: return L"GRADIENT";
    case ToolType::Move: return L"MOVE";
    case ToolType::LassoSelect: return L"LASSO";
    case ToolType::RectSelect: return L"SELECT";
    case ToolType::EllipseSelect: return L"ELLIPSE";
    case ToolType::MagicWand: return L"WAND";
    case ToolType::Transform: return L"TRANSFORM";
    case ToolType::Text: return L"TEXT";
    default: return L"BRUSH";
    }
}

String LayerDisplayName(const String& name, size_t index) {
    if (!name.empty()) {
        return name;
    }
    return L"LAYER " + std::to_wstring(index + 1);
}

WorkspaceSnapshot CaptureWorkspaceSnapshot(const WorkspaceEditorFacade& editor,
                                           const WorkspaceUiState& ui,
                                           const String& documentPath) {
    WorkspaceSnapshot snapshot;
    snapshot.documentName = FileNameOf(documentPath);
    snapshot.statusText = ui.statusText;
    snapshot.hasProject = editor.HasProject();
    snapshot.activeTool = ToolDisplayName(editor.GetTool());
    snapshot.brushSize = editor.GetBrushSize();
    snapshot.brushOpacity = editor.GetBrushOpacity();
    snapshot.brushFlow = editor.GetBrushFlow();
    snapshot.brushSpacing = editor.GetBrushSpacing();

    if (const LayerManager* layers = editor.GetLayerManager()) {
        snapshot.activeLayer = layers->GetActiveLayerIndex();
        snapshot.layers.reserve(layers->GetLayerCount());
        for (size_t index = 0; index < layers->GetLayerCount(); ++index) {
            const Ref<Layer> layer = const_cast<LayerManager*>(layers)->GetLayer(index);
            if (!layer) {
                continue;
            }
            WorkspaceLayerRow row;
            row.name = LayerDisplayName(layer->GetName(), index);
            row.visible = layer->IsVisible();
            row.locked = layer->IsLocked();
            row.active = index == snapshot.activeLayer;
            row.opacity = layer->GetOpacity();
            snapshot.layers.push_back(std::move(row));
        }
    }

    snapshot.canvasWidth = editor.GetCanvasWidth();
    snapshot.canvasHeight = editor.GetCanvasHeight();
    snapshot.zoomPercent = static_cast<int>(editor.GetZoom() * 100.0f + 0.5f);
    return snapshot;
}

} // namespace CloverPic::Core
