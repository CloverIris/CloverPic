#pragma once

#include "Core/App/AppCommand.h"
#include "Core/UI/ProjectManager/Layout/ProjectManagerLayout.h"
#include "Core/UI/Scene/UiScene.h"

namespace CloverPic::Core {

class ProjectManagerComponentBuilder {
public:
    static void Build(const Size& viewport,
                      const ProjectManagerUiState& uiState,
                      CoreUI::UiScene& scene);
};

} // namespace CloverPic::Core
