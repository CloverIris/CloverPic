#import "Platform/Mac/MacSurfaceView.h"

#include "Platform/Mac/MacTime.h"

#include <algorithm>
#include <cmath>

using namespace CloverPic;

namespace {

uint32_t TranslateModifiers(NSEventModifierFlags flags) {
    uint32_t modifiers = Input::ModifierNone;
    if ((flags & NSEventModifierFlagShift) != 0) modifiers |= Input::ModifierShift;
    // Core commands are written against a Control-based shortcut model, so the
    // Command key maps onto ModifierControl to keep Cmd+S / Cmd+Z native.
    if ((flags & (NSEventModifierFlagCommand | NSEventModifierFlagControl)) != 0) {
        modifiers |= Input::ModifierControl;
    }
    if ((flags & NSEventModifierFlagOption) != 0) modifiers |= Input::ModifierAlt;
    return modifiers;
}

// Translates a macOS hardware key code into the Windows virtual-key values that
// core's input controllers compare against.
uint32_t TranslateKeyCode(NSEvent* event) {
    switch (event.keyCode) {
        case 0x00: return 'A';
        case 0x0B: return 'B';
        case 0x08: return 'C';
        case 0x02: return 'D';
        case 0x0E: return 'E';
        case 0x03: return 'F';
        case 0x05: return 'G';
        case 0x04: return 'H';
        case 0x22: return 'I';
        case 0x26: return 'J';
        case 0x28: return 'K';
        case 0x25: return 'L';
        case 0x2E: return 'M';
        case 0x2D: return 'N';
        case 0x1F: return 'O';
        case 0x23: return 'P';
        case 0x0C: return 'Q';
        case 0x0F: return 'R';
        case 0x01: return 'S';
        case 0x11: return 'T';
        case 0x20: return 'U';
        case 0x09: return 'V';
        case 0x0D: return 'W';
        case 0x07: return 'X';
        case 0x10: return 'Y';
        case 0x06: return 'Z';

        case 0x1D: return '0';
        case 0x12: return '1';
        case 0x13: return '2';
        case 0x14: return '3';
        case 0x15: return '4';
        case 0x17: return '5';
        case 0x16: return '6';
        case 0x1A: return '7';
        case 0x1C: return '8';
        case 0x19: return '9';

        case 0x24: return 0x0D; // Return
        case 0x4C: return 0x0D; // Keypad Enter
        case 0x30: return 0x09; // Tab
        case 0x33: return 0x08; // Delete (backspace)
        case 0x35: return 0x1B; // Escape
        case 0x31: return 0x20; // Space

        case 0x7B: return 0x25; // Left
        case 0x7E: return 0x26; // Up
        case 0x7C: return 0x27; // Right
        case 0x7D: return 0x28; // Down
        case 0x73: return 0x24; // Home
        case 0x77: return 0x23; // End
        case 0x74: return 0x21; // Page Up
        case 0x79: return 0x22; // Page Down
        case 0x75: return 0x2E; // Forward delete
        case 0x72: return 0x2D; // Help / Insert

        case 0x21: return 0xDB; // [
        case 0x1E: return 0xDD; // ]
        case 0x1B: return 0xBD; // -
        case 0x18: return 0xBB; // =
        case 0x2A: return 0xDC; // backslash
        case 0x29: return 0xBA; // ;
        case 0x27: return 0xDE; // '
        case 0x2B: return 0xBC; // ,
        case 0x2F: return 0xBE; // .
        case 0x2C: return 0xBF; // /
        case 0x32: return 0xC0; // `

        case 0x7A: return 0x70; // F1
        case 0x78: return 0x71; // F2
        case 0x63: return 0x72; // F3
        case 0x76: return 0x73; // F4
        case 0x60: return 0x74; // F5
        case 0x61: return 0x75; // F6
        case 0x62: return 0x76; // F7
        case 0x64: return 0x77; // F8
        case 0x65: return 0x78; // F9
        case 0x6D: return 0x79; // F10
        case 0x67: return 0x7A; // F11
        case 0x6F: return 0x7B; // F12

        default:
            break;
    }

    // Fall back to the printable character so text entry still works for keys
    // that are not in the table above.
    NSString* characters = [event charactersIgnoringModifiers];
    if (characters.length > 0) {
        const unichar ch = [characters characterAtIndex:0];
        if (ch >= 'a' && ch <= 'z') return static_cast<uint32_t>(ch - 32);
        if (ch >= 'A' && ch <= 'Z') return ch;
        if (ch >= '0' && ch <= '9') return ch;
    }
    return 0;
}

} // namespace

@implementation MacSurfaceView {
    Core::RuntimeSurface* _runtime;
    MacSurfaceRole _role;
    float _dpiScale;
}

- (instancetype)initWithRuntime:(Core::RuntimeSurface*)runtime role:(MacSurfaceRole)role {
    self = [super initWithFrame:NSZeroRect];
    if (self) {
        _runtime = runtime;
        _role = role;
        _dpiScale = 1.0f;
    }
    return self;
}

- (BOOL)isFlipped {
    // Core's pixel space has a top-left origin, matching the Windows adapter.
    return YES;
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (BOOL)becomeFirstResponder {
    return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent*)event {
    (void)event;
    return YES;
}

- (void)setFrameSize:(NSSize)newSize {
    [super setFrameSize:newSize];
    [self updateRuntimeSize];
}

- (void)viewDidMoveToWindow {
    [super viewDidMoveToWindow];
    if (self.window) {
        [self updateRuntimeSize];
        [self.window makeFirstResponder:self];
    }
}

- (void)viewDidChangeBackingProperties {
    [super viewDidChangeBackingProperties];
    [self updateRuntimeSize];
}

- (void)updateRuntimeSize {
    if (!_runtime) return;
    NSWindow* window = self.window;
    CGFloat scale = window ? window.backingScaleFactor : 1.0;
    if (!(scale > 0)) scale = 1.0;
    _dpiScale = static_cast<float>(scale);

    const NSRect bounds = self.bounds;
    const int32_t width = static_cast<int32_t>(std::max(1.0, std::round(NSWidth(bounds) * scale)));
    const int32_t height = static_cast<int32_t>(std::max(1.0, std::round(NSHeight(bounds) * scale)));
    _runtime->Resize(static_cast<uint32_t>(width), static_cast<uint32_t>(height), _dpiScale);
    [self setNeedsDisplay:YES];
}

- (void)requestFrame {
    [self setNeedsDisplay:YES];
}

- (void)tickFrame {
    if (!_runtime) return;
    if (_runtime->NeedsFrame(Platform::Mac::NowMs())) {
        [self setNeedsDisplay:YES];
    }
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    if (!_runtime) return;

    std::vector<CloverPic::Rect> dirtyRects;
    const Core::RgbaFrame& frame = _runtime->Render(Platform::Mac::NowMs(), dirtyRects);
    if (frame.IsEmpty()) return;

    CGContextRef context = [[NSGraphicsContext currentContext] CGContext];
    if (!context) return;

    CGDataProviderRef provider = CGDataProviderCreateWithData(
        nullptr, frame.pixels.data(), frame.pixels.size(), nullptr);
    if (!provider) return;
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGImageRef image = CGImageCreate(
        frame.width, frame.height, 8, 32, static_cast<size_t>(frame.width) * 4, colorSpace,
        static_cast<CGBitmapInfo>(kCGImageAlphaNoneSkipFirst) | static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little),
        provider, nullptr, false, kCGRenderingIntentDefault);

    if (image) {
        const NSRect bounds = self.bounds;
        CGContextSaveGState(context);
        CGContextSetInterpolationQuality(context, kCGInterpolationLow);
        // The view is flipped so core and AppKit share a top-left origin; undo
        // that flip for the image draw so pixels are not mirrored vertically.
        CGContextTranslateCTM(context, 0, NSHeight(bounds));
        CGContextScaleCTM(context, 1, -1);
        CGContextDrawImage(context, CGRectMake(0, 0, NSWidth(bounds), NSHeight(bounds)), image);
        CGContextRestoreGState(context);
        CGImageRelease(image);
    }

    CGColorSpaceRelease(colorSpace);
    CGDataProviderRelease(provider);
}

- (CloverPic::Point)pixelPointForEvent:(NSEvent*)event {
    const NSPoint local = [self convertPoint:event.locationInWindow fromView:nil];
    const float scale = _dpiScale > 0 ? _dpiScale : 1.0f;
    return CloverPic::Point(static_cast<int32_t>(std::lround(local.x * scale)),
                       static_cast<int32_t>(std::lround(local.y * scale)));
}

- (void)sendPointer:(Input::PointerAction)action
             button:(MouseButton)button
              event:(NSEvent*)event {
    if (!_runtime) return;
    Input::PointerEvent pointer;
    pointer.device = Input::PointerDeviceType::Mouse;
    pointer.action = action;
    pointer.position = [self pixelPointForEvent:event];
    pointer.button = button;
    pointer.pressure = 1.0f;
    pointer.inContact = action == Input::PointerAction::Down ||
                        (action == Input::PointerAction::Move && button != MouseButton::None);
    pointer.modifiers = TranslateModifiers(event.modifierFlags);
    _runtime->HandlePointer(pointer);
    [self requestFrame];
}

- (void)mouseDown:(NSEvent*)event {
    if (_role == MacSurfaceRoleHome) {
        const CloverPic::Point position = [self pixelPointForEvent:event];
        if (_runtime && _runtime->IsWindowDragRegion(position)) {
            // The Program Manager is a borderless window, so its chrome-less
            // background doubles as the title bar.
            [self.window performWindowDragWithEvent:event];
            return;
        }
    }
    const Input::PointerAction action =
        event.clickCount >= 2 ? Input::PointerAction::DoubleClick : Input::PointerAction::Down;
    [self sendPointer:action button:MouseButton::Left event:event];
}

- (void)mouseUp:(NSEvent*)event {
    [self sendPointer:Input::PointerAction::Up button:MouseButton::Left event:event];
}

- (void)mouseDragged:(NSEvent*)event {
    [self sendPointer:Input::PointerAction::Move button:MouseButton::Left event:event];
}

- (void)rightMouseDown:(NSEvent*)event {
    [self sendPointer:Input::PointerAction::Down button:MouseButton::Right event:event];
}

- (void)rightMouseUp:(NSEvent*)event {
    [self sendPointer:Input::PointerAction::Up button:MouseButton::Right event:event];
}

- (void)rightMouseDragged:(NSEvent*)event {
    [self sendPointer:Input::PointerAction::Move button:MouseButton::Right event:event];
}

- (void)otherMouseDown:(NSEvent*)event {
    [self sendPointer:Input::PointerAction::Down button:MouseButton::Middle event:event];
}

- (void)otherMouseUp:(NSEvent*)event {
    [self sendPointer:Input::PointerAction::Up button:MouseButton::Middle event:event];
}

- (void)otherMouseDragged:(NSEvent*)event {
    [self sendPointer:Input::PointerAction::Move button:MouseButton::Middle event:event];
}

- (void)mouseMoved:(NSEvent*)event {
    [self sendPointer:Input::PointerAction::Move button:MouseButton::None event:event];
}

- (void)scrollWheel:(NSEvent*)event {
    if (!_runtime) return;
    CGFloat delta = event.scrollingDeltaY;
    if (event.hasPreciseScrollingDeltas) {
        delta /= 10.0;
    }
    if (std::abs(delta) < 0.01) return;
    // Core only reads the sign: positive means wheel-up (zoom in / scroll up).
    const int wheelDelta = delta > 0 ? 120 : -120;
    _runtime->HandleWheel(wheelDelta, [self pixelPointForEvent:event]);
    [self requestFrame];
}

- (void)keyDown:(NSEvent*)event {
    if (!_runtime) return;
    Input::KeyEvent key;
    key.action = Input::KeyAction::Down;
    key.keyCode = TranslateKeyCode(event);
    key.modifiers = TranslateModifiers(event.modifierFlags);
    key.isRepeat = event.isARepeat;
    _runtime->HandleKey(key);
    [self requestFrame];
}

- (void)keyUp:(NSEvent*)event {
    if (!_runtime) return;
    Input::KeyEvent key;
    key.action = Input::KeyAction::Up;
    key.keyCode = TranslateKeyCode(event);
    key.modifiers = TranslateModifiers(event.modifierFlags);
    _runtime->HandleKey(key);
    [self requestFrame];
}

@end
