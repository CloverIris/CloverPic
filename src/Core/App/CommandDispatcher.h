#pragma once

#include "Core/App/AppCommand.h"
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
        virtual void OnCreateCanvasPreset(uint32_t width, uint32_t height) = 0;
        virtual void OnCloseModal() = 0;
        virtual void OnOpenProject() = 0;
        virtual void OnOpenRecentProject(const String& path) = 0;
        virtual void OnFocusHomeSearch() = 0;
        virtual void OnSaveProject(bool saveAs) = 0;
        virtual void OnExportPng() = 0;
        virtual void OnUndo() = 0;
        virtual void OnRedo() = 0;
        virtual void OnQuit() = 0;
        virtual void OnSelectTool(ToolType tool) = 0;
        virtual void OnSetColor(const Color& color) = 0;
        virtual void OnAddLayer() = 0;
        virtual void OnDeleteLayer() = 0;
        virtual void OnToggleActiveLayerVisibility() = 0;
        virtual void OnSelectLayer(size_t index) = 0;
    };

    void Dispatch(Handler& handler, AppCommand command, uint64_t userData = 0, const String& payload = L"") const;
};

} // namespace CloverPic::Core
