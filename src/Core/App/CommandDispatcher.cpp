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
        case AppCommand::ShowSettingsModal:
            handler.OnShowSettingsModal();
            break;
        case AppCommand::CreateCanvasFromForm:
            handler.OnCreateCanvasFromForm();
            break;
        case AppCommand::SwapCanvasOrientation:
            handler.OnSwapCanvasOrientation();
            break;
        case AppCommand::ToggleCanvasTransparency:
            handler.OnToggleCanvasTransparency();
            break;
        case AppCommand::SelectRgbProfile:
            handler.OnSelectRgbProfile(static_cast<size_t>(userData));
            break;
        case AppCommand::SelectCmykProfile:
            handler.OnSelectCmykProfile(static_cast<size_t>(userData));
            break;
        case AppCommand::SetSettingsCategory:
            handler.OnSetSettingsCategory(static_cast<size_t>(userData));
            break;
        case AppCommand::ApplySettings:
            handler.OnApplySettings();
            break;
        case AppCommand::SaveSettings:
            handler.OnSaveSettings();
            break;
        case AppCommand::CreateCanvasPreset:
            handler.OnCreateCanvasPreset(PresetWidth(userData), PresetHeight(userData));
            break;
        case AppCommand::SetCanvasAnchor:
            handler.OnSetCanvasAnchor(static_cast<uint32_t>(userData));
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
        case AppCommand::ShowFiltersModal:
            handler.OnShowFiltersModal();
            break;
        case AppCommand::ShowTextLayerModal:
            handler.OnShowTextLayerModal();
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
        case AppCommand::ShowCanvasSizeModal:
            handler.OnShowCanvasSizeModal();
            break;
        case AppCommand::FlipLayerHorizontal:
            handler.OnFlipLayerHorizontal();
            break;
        case AppCommand::FlipLayerVertical:
            handler.OnFlipLayerVertical();
            break;
        case AppCommand::RotateLayerLeft:
            handler.OnRotateLayerLeft();
            break;
        case AppCommand::RotateLayerRight:
            handler.OnRotateLayerRight();
            break;
        case AppCommand::SelectAll:
            handler.OnSelectAll();
            break;
        case AppCommand::ClearSelection:
            handler.OnClearSelection();
            break;
        case AppCommand::InvertSelection:
            handler.OnInvertSelection();
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
        case AppCommand::ToggleLayerLock:
            handler.OnToggleActiveLayerLock();
            break;
        case AppCommand::ToggleProtectAlpha:
            handler.OnToggleActiveLayerProtectAlpha();
            break;
        case AppCommand::ToggleSolo:
            handler.OnToggleActiveLayerSolo();
            break;
        case AppCommand::DuplicateLayer:
            handler.OnDuplicateLayer();
            break;
        case AppCommand::MergeLayerDown:
            handler.OnMergeLayerDown();
            break;
        case AppCommand::SetLayerOpacity:
            handler.OnSetLayerOpacity(static_cast<uint8_t>(userData & 0xFFu));
            break;
        case AppCommand::SetBlendMode:
            handler.OnSetBlendMode(static_cast<uint32_t>(userData));
            break;
        case AppCommand::SelectLayer:
            handler.OnSelectLayer(static_cast<size_t>(userData));
            break;
        case AppCommand::SetBrushParam:
            handler.OnSetBrushParam(BrushParamFromPacked(userData), BrushParamValueFromPacked(userData));
            break;
        case AppCommand::SetBrushTip:
            handler.OnSetBrushTip(static_cast<uint32_t>(userData));
            break;
        case AppCommand::SetBrushPreset:
            handler.OnSetBrushPreset(BrushPresetSizeFromPacked(userData), BrushPresetTipFromPacked(userData));
            break;
        case AppCommand::ApplyFilter:
            handler.OnApplyFilter(FilterCommandFromPacked(userData), FilterValueFromPacked(userData));
            break;
        case AppCommand::CreateTextLayer:
            handler.OnCreateTextLayer(payload);
            break;
        case AppCommand::SetZoomPreset:
            handler.OnSetZoomPreset(static_cast<uint16_t>(userData & 0xFFFFu));
            break;
        case AppCommand::ZoomIn:
            handler.OnZoomIn();
            break;
        case AppCommand::ZoomOut:
            handler.OnZoomOut();
            break;
        case AppCommand::FitCanvas:
            handler.OnFitCanvas();
            break;
        case AppCommand::ResetView:
            handler.OnResetView();
            break;
        case AppCommand::ToggleViewOption:
            handler.OnToggleViewOption(static_cast<ViewOptionId>(userData));
            break;
        case AppCommand::SetSnapMode:
            handler.OnSetSnapMode(static_cast<SnapModeId>(userData));
            break;
        case AppCommand::TogglePanel:
            handler.OnTogglePanel(static_cast<WorkspacePanelId>(userData));
            break;
        case AppCommand::ToggleLeftSidebar:
            handler.OnToggleLeftSidebar();
            break;
        case AppCommand::ToggleRightSidebar:
            handler.OnToggleRightSidebar();
            break;
        case AppCommand::InitializeLayout:
            handler.OnInitializeLayout();
            break;
        case AppCommand::ShowUnavailable:
            handler.OnShowUnavailable();
            break;
        case AppCommand::CloseWorkspace:
            handler.OnCloseWorkspace();
            break;
        case AppCommand::None:
            break;
    }
}

} // namespace CloverPic::Core
