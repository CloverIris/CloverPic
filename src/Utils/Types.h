#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <Windows.h>

namespace VividPic {

using String = std::wstring;
using StringView = std::wstring_view;

template<typename T>
using Ref = std::shared_ptr<T>;

template<typename T>
using Scope = std::unique_ptr<T>;

template<typename T, typename... Args>
constexpr Ref<T> MakeRef(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

template<typename T, typename... Args>
constexpr Scope<T> MakeScope(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

struct Point {
    int32_t x = 0;
    int32_t y = 0;
    
    Point() = default;
    Point(int32_t x_, int32_t y_) : x(x_), y(y_) {}
};

struct Size {
    int32_t width = 0;
    int32_t height = 0;
    
    Size() = default;
    Size(int32_t w, int32_t h) : width(w), height(h) {}
    
    bool IsEmpty() const { return width <= 0 || height <= 0; }
};

struct Rect {
    int32_t left = 0;
    int32_t top = 0;
    int32_t right = 0;
    int32_t bottom = 0;
    
    Rect() = default;
    Rect(int32_t l, int32_t t, int32_t r, int32_t b) 
        : left(l), top(t), right(r), bottom(b) {}
    Rect(const Point& pos, const Size& size)
        : left(pos.x), top(pos.y), right(pos.x + size.width), bottom(pos.y + size.height) {}
    
    int32_t Width() const { return right - left; }
    int32_t Height() const { return bottom - top; }
    Point Position() const { return Point(left, top); }
    Size GetSize() const { return Size(Width(), Height()); }
    
    bool Contains(const Point& p) const {
        return p.x >= left && p.x < right && p.y >= top && p.y < bottom;
    }
    
    RECT ToWin32Rect() const {
        return RECT{ left, top, right, bottom };
    }
};

struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
    
    Color() = default;
    Color(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255)
        : r(r_), g(g_), b(b_), a(a_) {}
    
    static Color FromHex(uint32_t hex, uint8_t alpha = 255) {
        return Color(
            static_cast<uint8_t>((hex >> 16) & 0xFF),
            static_cast<uint8_t>((hex >> 8) & 0xFF),
            static_cast<uint8_t>(hex & 0xFF),
            alpha
        );
    }
    
    COLORREF ToCOLORREF() const {
        return RGB(r, g, b);
    }
    
    static Color Interpolate(const Color& a, const Color& b, float t) {
        return Color(
            static_cast<uint8_t>(a.r + (b.r - a.r) * t),
            static_cast<uint8_t>(a.g + (b.g - a.g) * t),
            static_cast<uint8_t>(a.b + (b.b - a.b) * t),
            static_cast<uint8_t>(a.a + (b.a - a.a) * t)
        );
    }
};

enum class MouseButton {
    None,
    Left,
    Right,
    Middle
};

struct MouseEvent {
    Point position;
    MouseButton button;
    bool pressed;
    bool doubleClick;
};

using Callback = std::function<void()>;
using CallbackBool = std::function<void(bool)>;
using CallbackInt = std::function<void(int)>;

} // namespace VividPic
