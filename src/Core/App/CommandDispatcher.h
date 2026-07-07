#pragma once

#include "Core/App/AppCommand.h"
#include "Core/App/AppCommandPayload.h"
#include "Core/App/ToolType.h"
#include "Utils/Types.h"
#include <cstdint>

namespace CloverPic::Core {

class CommandDispatcher {
public:
    class Handler {
    public:
        virtual ~Handler() = default;
        virtual void OnGoHome() = 0;
        virtual void OnShowNewCanvasModal() = 0;
        virtual void OnShowSettingsModal() {}
        virtual void OnCreateCanvasFromForm() {}
        virtual void OnSwapCanvasOrientation() {}
        virtual void OnToggleCanvasTransparency() {}
        virtual void OnSelectRgbProfile(size_t) {}
        virtual void OnSelectCmykProfile(size_t) {}
        virtual void OnSetSettingsCategory(size_t) {}
        virtual void OnApplySettings() {}
        virtual void OnSaveSettings() {}
        virtual void OnCreateCanvasPreset(uint32_t width, uint32_t height) = 0;
        virtual void OnSetCanvasAnchor(uint32_t) {}
        virtual void OnCloseModal() = 0;
        virtual void OnOpenProject() = 0;
        virtual void OnOpenRecentProject(const String& path) = 0;
        virtual void OnFocusHomeSearch() = 0;
        virtual void OnShowFiltersModal() {}
        virtual void OnShowTextLayerModal() {}
        virtual void OnSaveProject(bool saveAs) = 0;
        virtual void OnExportPng() = 0;
        virtual void OnUndo() = 0;
        virtual void OnRedo() = 0;
        virtual void OnShowCanvasSizeModal() {}
        virtual void OnFlipLayerHorizontal() {}
        virtual void OnFlipLayerVertical() {}
        virtual void OnRotateLayerLeft() {}
        virtual void OnRotateLayerRight() {}
        virtual void OnSelectAll() {}
        virtual void OnClearSelection() {}
        virtual void OnInvertSelection() {}
        virtual void OnQuit() = 0;
        virtual void OnSelectTool(ToolType tool) = 0;
        virtual void OnSetColor(const Color& color) = 0;
        virtual void OnAddLayer() = 0;
        virtual void OnDeleteLayer() = 0;
        virtual void OnToggleActiveLayerVisibility() = 0;
        virtual void OnToggleActiveLayerLock() {}
        virtual void OnToggleActiveLayerProtectAlpha() {}
        virtual void OnToggleActiveLayerSolo() {}
        virtual void OnDuplicateLayer() {}
        virtual void OnMergeLayerDown() {}
        virtual void OnSetLayerOpacity(uint8_t) {}
        virtual void OnSetBlendMode(uint32_t) {}
        virtual void OnSelectLayer(size_t index) = 0;
        virtual void OnSetBrushParam(BrushParamId, uint16_t) {}
        virtual void OnSetBrushTip(uint32_t) {}
        virtual void OnSetBrushPreset(uint16_t, uint16_t) {}
        virtual void OnApplyFilter(FilterCommandId, uint16_t) {}
        virtual void OnCreateTextLayer(const String&) {}
        virtual void OnSetZoomPreset(uint16_t) {}
        virtual void OnZoomIn() {}
        virtual void OnZoomOut() {}
        virtual void OnFitCanvas() {}
        virtual void OnResetView() {}
        virtual void OnToggleViewOption(ViewOptionId) {}
        virtual void OnSetSnapMode(SnapModeId) {}
        virtual void OnTogglePanel(WorkspacePanelId) {}
        virtual void OnToggleLeftSidebar() {}
        virtual void OnToggleRightSidebar() {}
        virtual void OnInitializeLayout() {}
        virtual void OnShowUnavailable() {}
        virtual void OnCloseWorkspace() {}
    };

    void Dispatch(Handler& handler, AppCommand command, uint64_t userData = 0, const String& payload = L"") const;
};

} // namespace CloverPic::Core
