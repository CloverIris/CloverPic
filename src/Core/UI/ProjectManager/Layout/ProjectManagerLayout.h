#pragma once

#include "Core/UI/ProjectManager/State/ProjectManagerUiState.h"

namespace CloverPic::Core {

struct ProjectManagerLayout {
    Rect fullRect;
    int margin = 0;
    int navWidth = 0;
    int left = 0;
    int top = 0;
    int contentLeft = 0;
    int contentRight = 0;
    int contentBottom = 0;
    int rowHeight = 36;
    int labelWidth = 148;
    int fieldWidth = 0;
    int fieldLeft = 0;
    int footerY = 0;
};

class ProjectManagerLayoutComputer {
public:
    static ProjectManagerLayout Compute(const Size& viewport, ProjectManagerPage page);
};

} // namespace CloverPic::Core
