#pragma once

#include <cstdint>

namespace CloverPic::Core {

enum class WorkspaceCanvasSizeField {
    None,
    Width,
    Height
};

enum class WorkspaceCanvasAnchor : uint32_t {
    TopLeft,
    TopCenter,
    TopRight,
    MiddleLeft,
    Center,
    MiddleRight,
    BottomLeft,
    BottomCenter,
    BottomRight
};

struct WorkspaceCanvasSizeState {
    WorkspaceCanvasSizeField focusedField = WorkspaceCanvasSizeField::None;
    uint32_t width = 0;
    uint32_t height = 0;
    WorkspaceCanvasAnchor anchor = WorkspaceCanvasAnchor::Center;
};

inline constexpr const wchar_t* WorkspaceCanvasSizeWidthPayload = L"canvas-size-width";
inline constexpr const wchar_t* WorkspaceCanvasSizeHeightPayload = L"canvas-size-height";
inline constexpr const wchar_t* WorkspaceCanvasAnchorPayload = L"canvas-anchor";

} // namespace CloverPic::Core
