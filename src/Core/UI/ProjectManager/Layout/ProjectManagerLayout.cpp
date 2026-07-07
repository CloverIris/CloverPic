#include "Core/UI/ProjectManager/Layout/ProjectManagerLayout.h"
#include <algorithm>

namespace CloverPic::Core {

ProjectManagerLayout ProjectManagerLayoutComputer::Compute(const Size& viewport, ProjectManagerPage page) {
    ProjectManagerLayout layout;
    layout.fullRect = Rect(0, 0, viewport.width, viewport.height);
    if (page == ProjectManagerPage::Settings) {
        layout.contentRight = viewport.width;
        layout.contentBottom = viewport.height;
        layout.navWidth = 220;
        return layout;
    }

    layout.margin = std::max(24, viewport.width / 34);
    layout.navWidth = std::clamp(viewport.width / 4, 220, 280);
    layout.left = layout.margin;
    layout.top = layout.margin;
    layout.contentLeft = layout.left + layout.navWidth + 28;
    layout.contentRight = viewport.width - layout.margin;
    layout.contentBottom = viewport.height - layout.margin;
    layout.fieldWidth = std::clamp((layout.contentRight - layout.contentLeft) / 2, 220, 360);
    layout.fieldLeft = layout.contentLeft + layout.labelWidth;
    layout.footerY = viewport.height - layout.margin - 48;
    return layout;
}

} // namespace CloverPic::Core
