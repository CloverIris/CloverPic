#pragma once

#include "Core/Input/InputEvents.h"
#include "Core/UI/ProjectManager/State/ProjectManagerUiState.h"
#include "Core/UI/Scene/UiScene.h"
#include <functional>

namespace CloverPic::Core {

struct ProjectManagerKeyInputContext {
    CoreUI::UiScene* scene = nullptr;
    ProjectManagerUiState* uiState = nullptr;
    bool* wantsQuit = nullptr;
    Rect fullRect;
};

struct ProjectManagerKeyInputCallbacks {
    std::function<void()> rebuildScene;
    std::function<void(const Rect&)> markDirty;
    std::function<void()> closeModal;
    std::function<void()> openProject;
    std::function<void()> quickNewCanvas;
};

class ProjectManagerKeyInputController {
public:
    static void HandleKey(const Input::KeyEvent& event,
                          ProjectManagerKeyInputContext& context,
                          const ProjectManagerKeyInputCallbacks& callbacks);
};

} // namespace CloverPic::Core
