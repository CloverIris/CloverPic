#include "Core/App/CommandDispatcher.h"
#include "Core/App/AppCommandPayload.h"

namespace CloverPic::Core {

void CommandDispatcher::Dispatch(Handler& handler, AppCommand command, uint64_t userData, const String& payload) const {
    switch (command) {
        case AppCommand::GoHome:
            handler.OnGoHome();
            break;
        case AppCommand::ShowNewCanvasModal:
            handler.OnShowNewCanvasModal();
            break;
        case AppCommand::CreateCanvasPreset:
            handler.OnCreateCanvasPreset(PresetWidth(userData), PresetHeight(userData));
            break;
        case AppCommand::CloseModal:
            handler.OnCloseModal();
            break;
        case AppCommand::OpenProject:
            handler.OnOpenProject();
            break;
        case AppCommand::OpenRecentProject:
            handler.OnOpenRecentProject(payload);
            break;
        case AppCommand::FocusHomeSearch:
            handler.OnFocusHomeSearch();
            break;
        case AppCommand::SaveProject:
            handler.OnSaveProject(false);
            break;
        case AppCommand::SaveProjectAs:
            handler.OnSaveProject(true);
            break;
        case AppCommand::ExportPng:
            handler.OnExportPng();
            break;
        case AppCommand::Undo:
            handler.OnUndo();
            break;
        case AppCommand::Redo:
            handler.OnRedo();
            break;
        case AppCommand::Quit:
            handler.OnQuit();
            break;
        case AppCommand::SelectTool:
            handler.OnSelectTool(static_cast<ToolType>(userData));
            break;
        case AppCommand::SetColor:
            handler.OnSetColor(ColorFromPackedRgba(userData));
            break;
        case AppCommand::AddLayer:
            handler.OnAddLayer();
            break;
        case AppCommand::DeleteLayer:
            handler.OnDeleteLayer();
            break;
        case AppCommand::ToggleLayerVisibility:
            handler.OnToggleActiveLayerVisibility();
            break;
        case AppCommand::SelectLayer:
            handler.OnSelectLayer(static_cast<size_t>(userData));
            break;
        case AppCommand::None:
            break;
    }
}

} // namespace CloverPic::Core
