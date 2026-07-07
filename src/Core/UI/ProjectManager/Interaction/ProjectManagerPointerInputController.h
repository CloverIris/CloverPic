#pragma once

#include "Core/App/AppCommand.h"
#include "Core/Input/InputEvents.h"
#include "Core/UI/ProjectManager/State/ProjectManagerUiState.h"
#include "Core/UI/Scene/UiScene.h"
#include <functional>

namespace CloverPic::Core {

struct ProjectManagerPointerInputContext {
    CoreUI::UiScene* scene = nullptr;
    ProjectManagerUiState* uiState = nullptr;
    Rect fullRect;
};

struct ProjectManagerPointerInputCallbacks {
    std::function<void(AppCommand, uint64_t, const String&)> dispatch;
    std::function<void()> rebuildScene;
    std::function<void(const Rect&)> markDirty;
};

class ProjectManagerPointerInputController {
public:
    static void HandlePointer(const Input::PointerEvent& event,
                              ProjectManagerPointerInputContext& context,
                              const ProjectManagerPointerInputCallbacks& callbacks);
};

} // namespace CloverPic::Core
