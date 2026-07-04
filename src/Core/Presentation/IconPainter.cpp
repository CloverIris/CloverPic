#include "Core/Presentation/IconPainter.h"
#include "Core/Presentation/SoftRenderer.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace CloverPic::Presentation {

namespace {

struct V2 {
    float x = 0.0f;
    float y = 0.0f;
};

class IconCanvas {
public:
    IconCanvas(SoftRenderer& renderer, Rect bounds, Color color, int thickness)
        : m_renderer(renderer), m_bounds(bounds), m_color(color), m_thickness(std::max(1, thickness)) {}

    void Line(float x1, float y1, float x2, float y2) {
        m_renderer.DrawLine(X(x1), Y(y1), X(x2), Y(y2), m_color, m_thickness);
    }

    void Box(float left, float top, float right, float bottom) {
        Line(left, top, right, top);
        Line(right, top, right, bottom);
        Line(right, bottom, left, bottom);
        Line(left, bottom, left, top);
    }

    void Poly(std::initializer_list<V2> points) {
        if (points.size() < 2) return;
        auto it = points.begin();
        V2 last = *it++;
        for (; it != points.end(); ++it) {
            Line(last.x, last.y, it->x, it->y);
            last = *it;
        }
    }

    void Circle(float x, float y, float radius) {
        m_renderer.StrokeCircle(X(x), Y(y), R(radius), m_color, m_thickness);
    }

    void Dot(float x, float y, float radius) {
        m_renderer.FillCircle(X(x), Y(y), R(radius), m_color);
    }

private:
    int X(float value) const {
        return m_bounds.left + static_cast<int>((value / 24.0f) * std::max(1, m_bounds.Width()));
    }

    int Y(float value) const {
        return m_bounds.top + static_cast<int>((value / 24.0f) * std::max(1, m_bounds.Height()));
    }

    int R(float value) const {
        return std::max(1, static_cast<int>((value / 24.0f) * std::min(m_bounds.Width(), m_bounds.Height())));
    }

    SoftRenderer& m_renderer;
    Rect m_bounds;
    Color m_color;
    int m_thickness = 1;
};

void DrawFile(IconCanvas& c, bool plus, bool disk, bool exportIcon) {
    c.Poly({ {7, 3}, {14, 3}, {19, 8}, {19, 21}, {7, 21}, {7, 3} });
    c.Poly({ {14, 3}, {14, 8}, {19, 8} });
    if (plus) {
        c.Line(13, 12, 13, 18);
        c.Line(10, 15, 16, 15);
    } else if (disk) {
        c.Box(9, 5, 17, 10);
        c.Box(9, 15, 17, 20);
    } else if (exportIcon) {
        c.Line(11, 16, 17, 10);
        c.Line(17, 10, 17, 15);
        c.Line(17, 10, 12, 10);
    }
}

void DrawBrush(IconCanvas& c) {
    c.Poly({ {7, 19}, {5, 21}, {3, 21}, {3, 19}, {5, 17} });
    c.Poly({ {6, 17}, {15, 8}, {18, 11}, {9, 20} });
    c.Poly({ {15, 8}, {17, 4}, {21, 3}, {20, 7}, {18, 11} });
}

void DrawEraser(IconCanvas& c) {
    c.Poly({ {4, 15}, {12, 7}, {20, 15}, {12, 23}, {4, 15} });
    c.Line(8, 11, 16, 19);
    c.Line(11, 22, 21, 22);
    c.Line(4, 22, 8, 22);
}

void DrawBucket(IconCanvas& c) {
    c.Poly({ {4, 10}, {10, 4}, {20, 14}, {14, 20}, {4, 10} });
    c.Line(7, 7, 17, 17);
    c.Poly({ {14, 20}, {18, 19}, {21, 16} });
    c.Dot(19, 21, 1.5f);
}

void DrawLayers(IconCanvas& c) {
    c.Poly({ {12, 3}, {21, 8}, {12, 13}, {3, 8}, {12, 3} });
    c.Poly({ {3, 12}, {12, 17}, {21, 12} });
    c.Poly({ {3, 16}, {12, 21}, {21, 16} });
}

} // namespace

bool IconPainter::Draw(SoftRenderer& renderer, IconId icon, const Rect& bounds, const Color& color, int thickness) {
    if (icon == IconId::None || bounds.Width() <= 0 || bounds.Height() <= 0) return false;
    IconCanvas c(renderer, bounds, color, thickness);

    switch (icon) {
        case IconId::FilePlus: DrawFile(c, true, false, false); return true;
        case IconId::FolderOpen:
            c.Poly({ {3, 7}, {9, 7}, {11, 9}, {21, 9}, {20, 20}, {4, 20}, {3, 7} });
            c.Line(4, 12, 21, 12);
            return true;
        case IconId::DeviceFloppy: DrawFile(c, false, true, false); return true;
        case IconId::DeviceFloppyPlus: DrawFile(c, true, true, false); return true;
        case IconId::FileExport: DrawFile(c, false, false, true); return true;
        case IconId::ArrowBackUp:
            c.Poly({ {9, 14}, {4, 9}, {9, 4} });
            c.Poly({ {5, 9}, {15, 9}, {20, 14}, {17, 19} });
            return true;
        case IconId::ArrowForwardUp:
            c.Poly({ {15, 14}, {20, 9}, {15, 4} });
            c.Poly({ {19, 9}, {9, 9}, {4, 14}, {7, 19} });
            return true;
        case IconId::ArrowsMaximize:
            c.Poly({ {4, 10}, {4, 4}, {10, 4} });
            c.Poly({ {14, 4}, {20, 4}, {20, 10} });
            c.Poly({ {20, 14}, {20, 20}, {14, 20} });
            c.Poly({ {10, 20}, {4, 20}, {4, 14} });
            return true;
        case IconId::ZoomReset:
            c.Circle(10, 10, 5);
            c.Line(14, 14, 20, 20);
            c.Poly({ {9, 7}, {12, 10}, {9, 13} });
            return true;
        case IconId::ZoomIn:
            c.Circle(10, 10, 5);
            c.Line(14, 14, 20, 20);
            c.Line(10, 7, 10, 13);
            c.Line(7, 10, 13, 10);
            return true;
        case IconId::ZoomOut:
            c.Circle(10, 10, 5);
            c.Line(14, 14, 20, 20);
            c.Line(7, 10, 13, 10);
            return true;
        case IconId::Home:
            c.Poly({ {3, 11}, {12, 4}, {21, 11} });
            c.Poly({ {6, 10}, {6, 20}, {18, 20}, {18, 10} });
            return true;
        case IconId::Brush: DrawBrush(c); return true;
        case IconId::Eraser: DrawEraser(c); return true;
        case IconId::Move:
            c.Line(12, 3, 12, 21);
            c.Line(3, 12, 21, 12);
            c.Poly({ {12, 3}, {9, 6}, {15, 6}, {12, 3} });
            c.Poly({ {12, 21}, {9, 18}, {15, 18}, {12, 21} });
            c.Poly({ {3, 12}, {6, 9}, {6, 15}, {3, 12} });
            c.Poly({ {21, 12}, {18, 9}, {18, 15}, {21, 12} });
            return true;
        case IconId::ColorPicker:
            c.Line(14, 5, 19, 10);
            c.Poly({ {13, 6}, {5, 14}, {4, 20}, {10, 19}, {18, 11} });
            c.Line(7, 16, 10, 19);
            return true;
        case IconId::Bucket: DrawBucket(c); return true;
        case IconId::Select:
            c.Line(5, 5, 10, 5);
            c.Line(14, 5, 19, 5);
            c.Line(19, 5, 19, 10);
            c.Line(19, 14, 19, 19);
            c.Line(19, 19, 14, 19);
            c.Line(10, 19, 5, 19);
            c.Line(5, 19, 5, 14);
            c.Line(5, 10, 5, 5);
            return true;
        case IconId::Typography:
            c.Line(5, 5, 19, 5);
            c.Line(12, 5, 12, 20);
            c.Line(8, 20, 16, 20);
            return true;
        case IconId::Shape:
            c.Box(4, 5, 13, 14);
            c.Circle(16, 16, 5);
            c.Line(6, 19, 18, 19);
            return true;
        case IconId::Eye:
            c.Poly({ {3, 12}, {7, 7}, {12, 5}, {17, 7}, {21, 12}, {17, 17}, {12, 19}, {7, 17}, {3, 12} });
            c.Circle(12, 12, 3);
            return true;
        case IconId::EyeOff:
            c.Line(4, 4, 20, 20);
            c.Poly({ {3, 12}, {7, 7}, {12, 5}, {17, 7}, {21, 12} });
            c.Poly({ {7, 17}, {12, 19}, {17, 17} });
            return true;
        case IconId::Lock:
            c.Box(6, 10, 18, 21);
            c.Poly({ {8, 10}, {8, 7}, {12, 3}, {16, 7}, {16, 10} });
            return true;
        case IconId::Trash:
            c.Line(5, 7, 19, 7);
            c.Poly({ {9, 7}, {9, 4}, {15, 4}, {15, 7} });
            c.Poly({ {7, 7}, {8, 21}, {16, 21}, {17, 7} });
            return true;
        case IconId::Copy:
            c.Box(8, 8, 20, 20);
            c.Box(4, 4, 16, 16);
            return true;
        case IconId::Layers: DrawLayers(c); return true;
        case IconId::Plus:
            c.Line(12, 5, 12, 19);
            c.Line(5, 12, 19, 12);
            return true;
        case IconId::Minus:
            c.Line(5, 12, 19, 12);
            return true;
        case IconId::Settings:
            c.Circle(12, 12, 3);
            c.Poly({ {12, 3}, {14, 6}, {18, 6}, {17, 10}, {20, 12}, {17, 14}, {18, 18}, {14, 18}, {12, 21}, {10, 18}, {6, 18}, {7, 14}, {4, 12}, {7, 10}, {6, 6}, {10, 6}, {12, 3} });
            return true;
        case IconId::X:
            c.Line(6, 6, 18, 18);
            c.Line(18, 6, 6, 18);
            return true;
        case IconId::RotateLeft:
            c.Poly({ {9, 7}, {5, 7}, {5, 3} });
            c.Poly({ {5, 7}, {10, 4}, {16, 5}, {20, 10}, {19, 16}, {14, 20}, {8, 18} });
            return true;
        case IconId::RotateRight:
            c.Poly({ {15, 7}, {19, 7}, {19, 3} });
            c.Poly({ {19, 7}, {14, 4}, {8, 5}, {4, 10}, {5, 16}, {10, 20}, {16, 18} });
            return true;
        case IconId::FlipHorizontal:
            c.Line(12, 4, 12, 20);
            c.Poly({ {4, 7}, {10, 12}, {4, 17}, {4, 7} });
            c.Poly({ {20, 7}, {14, 12}, {20, 17}, {20, 7} });
            return true;
        case IconId::FlipVertical:
            c.Line(4, 12, 20, 12);
            c.Poly({ {7, 4}, {12, 10}, {17, 4}, {7, 4} });
            c.Poly({ {7, 20}, {12, 14}, {17, 20}, {7, 20} });
            return true;
        case IconId::TextPlus:
            c.Line(4, 5, 15, 5);
            c.Line(9, 5, 9, 19);
            c.Line(6, 19, 12, 19);
            c.Line(18, 13, 18, 21);
            c.Line(14, 17, 22, 17);
            return true;
        case IconId::Merge:
            c.Poly({ {6, 4}, {6, 10}, {12, 10}, {12, 20} });
            c.Poly({ {18, 4}, {18, 10}, {12, 10} });
            c.Poly({ {9, 17}, {12, 20}, {15, 17} });
            return true;
        case IconId::None:
            break;
    }
    return false;
}

} // namespace CloverPic::Presentation
