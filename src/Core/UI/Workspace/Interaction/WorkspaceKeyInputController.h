#pragma once

#include "Core/App/ModalManager.h"
#include "Core/App/WorkspaceEditorFacade.h"
#include "Core/UI/Workspace/Modal/WorkspaceModalState.h"
#include "Core/Input/InputEvents.h"
#include <functional>

namespace CloverPic::Core {

struct WorkspaceKeyInputContext {
    ModalKind modalKind = ModalKind::None;
    WorkspaceCanvasSizeState* canvasSizeState = nullptr;
    String* openMenu = nullptr;
    bool* wantsClose = nullptr;
    WorkspaceEditorFacade* editor = nullptr;
};

struct WorkspaceKeyInputCallbacks {
    std::function<void()> touchWorkspace;
    std::function<void()> closeModal;
    std::function<void()> applyCanvasSize;
    std::function<void(bool)> saveProject;
    std::function<void()> undo;
    std::function<void()> redo;
    std::function<void()> selectAll;
    std::function<void()> clearSelection;
    std::function<void()> invertSelection;
    std::function<void()> fitCanvas;
    std::function<void(uint16_t)> setZoomPreset;
    std::function<void()> showSettings;
    std::function<void()> zoomIn;
    std::function<void()> zoomOut;
    std::function<void(ToolType)> switchTool;
    std::function<void(const String&)> setStatus;
};

class WorkspaceKeyInputController {
public:
    static bool HandleKeyDown(const Input::KeyEvent& event,
                              WorkspaceKeyInputContext& context,
                              const WorkspaceKeyInputCallbacks& callbacks);
};

} // namespace CloverPic::Core
