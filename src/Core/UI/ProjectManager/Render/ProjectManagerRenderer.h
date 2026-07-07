#pragma once

#include "Core/Presentation/SoftRenderer.h"
#include "Core/UI/ProjectManager/State/ProjectManagerUiState.h"
#include "Core/UI/Scene/UiScene.h"

namespace CloverPic::Core {

class ProjectManagerRenderer {
public:
    static void Render(Presentation::SoftRenderer& renderer,
                       const Size& viewport,
                       const ProjectManagerUiState& uiState,
                       const CoreUI::UiScene& scene);
};

} // namespace CloverPic::Core
