#pragma once

#import <Cocoa/Cocoa.h>

#include "Core/Input/InputEvents.h"

namespace CloverPic::Platform::Mac {

/// Core's modifier bits, from an AppKit modifier mask.
uint32_t TranslateModifiers(NSEventModifierFlags flags);

/// Windows virtual-key value for a macOS hardware key code.
uint32_t TranslateKeyCode(NSEvent* event);

/// Builds a core key event from an AppKit key event.
Input::KeyEvent TranslateKeyEvent(NSEvent* event, Input::KeyAction action);

} // namespace CloverPic::Platform::Mac
