#pragma once

#include "Utils/Types.h"

namespace CloverPic::Core::WorkspaceLayout {

constexpr int MenuBarH = 24;
constexpr int CommandBarH = 30;
constexpr int TopBarH = MenuBarH + CommandBarH;
constexpr int StatusBarH = 24;
constexpr int ToolBarW = 64;
constexpr int LeftPanelW = 292;
constexpr int RightPanelW = 312;
constexpr int EdgeTabW = 18;

inline int LeftDockWidth(bool expanded) {
    return expanded ? LeftPanelW : EdgeTabW;
}

inline int RightDockWidth(bool expanded) {
    return expanded ? RightPanelW : EdgeTabW;
}

inline Rect CanvasRect(const Size& viewport, bool leftExpanded, bool rightExpanded, bool statusBarVisible) {
    const int bottom = viewport.height - (statusBarVisible ? StatusBarH : 0);
    return Rect(ToolBarW + LeftDockWidth(leftExpanded),
                TopBarH,
                viewport.width - RightDockWidth(rightExpanded),
                bottom);
}

} // namespace CloverPic::Core::WorkspaceLayout
