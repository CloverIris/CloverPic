#pragma once

#include "Utils/Types.h"
#include <cstdint>

namespace CloverPic::Input {

enum class PointerDeviceType {
    Mouse,
    Pen,
    Touch
};

enum class PointerAction {
    Move,
    Down,
    Up,
    DoubleClick
};

enum class KeyAction {
    Down,
    Up
};

enum ModifierKeys : uint32_t {
    ModifierNone = 0,
    ModifierShift = 1u << 0,
    ModifierControl = 1u << 1,
    ModifierAlt = 1u << 2
};

struct PointerEvent {
    PointerDeviceType device = PointerDeviceType::Mouse;
    PointerAction action = PointerAction::Move;
    Point position;
    MouseButton button = MouseButton::None;
    float pressure = 1.0f;
    float tiltX = 0.0f;
    float tiltY = 0.0f;
    float rotation = 0.0f;
    bool inContact = false;
    bool eraser = false;
    uint32_t modifiers = ModifierNone;
};

struct KeyEvent {
    KeyAction action = KeyAction::Down;
    uint32_t keyCode = 0;
    uint32_t modifiers = ModifierNone;
    bool isRepeat = false;
};

} // namespace CloverPic::Input
