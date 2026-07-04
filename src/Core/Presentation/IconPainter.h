#pragma once

#include "Utils/Types.h"

namespace CloverPic::Presentation {

class SoftRenderer;

enum class IconId {
    None,
    FilePlus,
    FolderOpen,
    DeviceFloppy,
    DeviceFloppyPlus,
    FileExport,
    ArrowBackUp,
    ArrowForwardUp,
    ArrowsMaximize,
    ZoomReset,
    ZoomIn,
    ZoomOut,
    Home,
    Brush,
    Eraser,
    Move,
    ColorPicker,
    Bucket,
    Select,
    Typography,
    Shape,
    Eye,
    EyeOff,
    Lock,
    Trash,
    Copy,
    Layers,
    Plus,
    Minus,
    Settings,
    X,
    RotateLeft,
    RotateRight,
    FlipHorizontal,
    FlipVertical,
    TextPlus,
    Merge
};

class IconPainter {
public:
    static bool Draw(SoftRenderer& renderer, IconId icon, const Rect& bounds, const Color& color, int thickness = 1);
};

} // namespace CloverPic::Presentation
