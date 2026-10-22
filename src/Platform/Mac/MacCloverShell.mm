#import "Platform/Mac/MacCloverShell.h"

#include "Platform/Mac/MacCloverIme.h"
#include "clover_platform.h"

#import <Cocoa/Cocoa.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#ifndef CLOVERPIC_UI_DIR
#define CLOVERPIC_UI_DIR "src/Resource/ui"
#endif

namespace {

NSWindow* g_shellWindow = nil;

std::string ShellDocumentPath() {
    if (const char* override = std::getenv("CLOVERPIC_UI_DOC")) {
        return override;
    }
    return std::string(CLOVERPIC_UI_DIR) + "/workspace.cui";
}

} // namespace

/// A CloverPlatform surface showing the workspace shell.
@interface CloverShellView : CloverImeView
- (void)syncSurface;
@end

@implementation CloverShellView {
    clover_surface* _surface;
    NSSize _lastSize;
    float _scale;
}

- (BOOL)isFlipped {
    return YES;
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (float)currentScale {
    NSWindow* window = self.window;
    return window ? static_cast<float>(window.backingScaleFactor) : 1.0f;
}

- (void)syncSurface {
    const NSRect bounds = self.bounds;
    if (NSWidth(bounds) < 2.0 || NSHeight(bounds) < 2.0) {
        return;
    }
    const float scale = [self currentScale];
    const auto width = static_cast<float>(NSWidth(bounds));
    const auto height = static_cast<float>(NSHeight(bounds));

    if (!_surface) {
        const std::string path = ShellDocumentPath();
        _surface = clover_surface_create_from_cui(width, height, scale, path.c_str());
        if (!_surface) {
            std::fprintf(stderr, "[clover-shell] failed to load %s\n", path.c_str());
            return;
        }
        _scale = scale;
        [self setCloverSurface:_surface];
        std::fprintf(stderr, "[clover-shell] workspace shell from %s (%.0fx%.0f pt)\n",
                     path.c_str(), width, height);
    } else if (std::fabs(scale - _scale) > 0.01f || !NSEqualSizes(bounds.size, _lastSize)) {
        _scale = scale;
        clover_surface_resize(_surface, width, height, scale);
    }
    _lastSize = bounds.size;
    clover_frame frame{};
    clover_surface_frame(_surface, &frame);
}

- (void)viewDidMoveToWindow {
    [super viewDidMoveToWindow];
    if (self.window) {
        [self setCloverSurface:_surface];
        [self syncSurface];
        [self.window makeFirstResponder:self];
    }
}

- (void)setFrameSize:(NSSize)newSize {
    [super setFrameSize:newSize];
    [self syncSurface];
    [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    if (!_surface) {
        [self syncSurface];
        if (!_surface) {
            return;
        }
    }
    clover_frame frame{};
    if (clover_surface_frame(_surface, &frame) != CLOVER_OK || !frame.pixels) {
        return;
    }
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(
        const_cast<uint32_t*>(frame.pixels), frame.width, frame.height, 8, frame.stride * 4, space,
        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
    CGColorSpaceRelease(space);
    if (!context) {
        return;
    }
    CGImageRef image = CGBitmapContextCreateImage(context);
    CGContextRelease(context);
    if (!image) {
        return;
    }
    CGContextRef target = [[NSGraphicsContext currentContext] CGContext];
    CGContextSaveGState(target);
    CGContextTranslateCTM(target, 0.0, NSHeight(self.bounds));
    CGContextScaleCTM(target, 1.0, -1.0);
    CGContextDrawImage(target, CGRectMake(0.0, 0.0, NSWidth(self.bounds), NSHeight(self.bounds)), image);
    CGContextRestoreGState(target);
    CGImageRelease(image);

    // Drain actions so the framework queue never grows.
    char buffer[512];
    // `take_action` returns the length copied, 0 when the queue is empty and -1
    // when the buffer is too small. It does not return `CLOVER_OK` (also 0), so
    // testing against `CLOVER_OK` would spin forever on an empty queue.
    while (clover_surface_take_action(_surface, buffer, sizeof(buffer)) > 0) {
        [self handleAction:std::string(buffer)];
    }
}

/// Routes a framework action.
///
/// Widgets report `change:<control>:<value>`; the shell reflects the value in its
/// own readouts. Once the canvas is hosted here these will also forward to core.
- (void)handleAction:(const std::string&)action {
    std::fprintf(stderr, "[clover-shell] action: %s\n", action.c_str());
    if (!_surface) {
        return;
    }
    const std::string prefix = "change:";
    if (action.rfind(prefix, 0) != 0) {
        return;
    }
    const std::string rest = action.substr(prefix.size());
    const auto colon = rest.find(':');
    if (colon == std::string::npos) {
        return;
    }
    const std::string control = rest.substr(0, colon);
    const std::string value = rest.substr(colon + 1);

    if (control == "size" || control == "opacity" || control == "flow" || control == "spacing") {
        clover_surface_set_label(_surface, "brushReadout",
                                 (control + " " + value).c_str());
    } else if (control == "blend") {
        clover_surface_set_label(_surface, "blendReadout", value.c_str());
    }
}

- (void)cloverActionsChanged {
    [self setNeedsDisplay:YES];
}

// ---- input -------------------------------------------------------------
// Without these the view accepts the first responder but never sees a click:
// AppKit delivers mouse events through the responder chain, and an NSView that
// does not override them silently swallows nothing.

- (BOOL)acceptsFirstMouse:(NSEvent*)event {
    (void)event;
    return YES;
}

- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    for (NSTrackingArea* area in [self trackingAreas]) {
        [self removeTrackingArea:area];
    }
    NSTrackingArea* area = [[NSTrackingArea alloc]
        initWithRect:self.bounds
             options:(NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited | NSTrackingActiveInKeyWindow |
                      NSTrackingInVisibleRect)
               owner:self
            userInfo:nil];
    [self addTrackingArea:area];
}

- (void)mouseDown:(NSEvent*)event {
    [self.window makeFirstResponder:self];
    [self pushPointer:CLOVER_POINTER_DOWN button:1 event:event];
}

- (void)mouseUp:(NSEvent*)event {
    [self pushPointer:CLOVER_POINTER_UP button:1 event:event];
}

- (void)mouseDragged:(NSEvent*)event {
    [self pushPointer:CLOVER_POINTER_MOVE button:1 event:event];
}

- (void)rightMouseDown:(NSEvent*)event {
    [self pushPointer:CLOVER_POINTER_DOWN button:2 event:event];
}

- (void)rightMouseUp:(NSEvent*)event {
    [self pushPointer:CLOVER_POINTER_UP button:2 event:event];
}

- (void)mouseMoved:(NSEvent*)event {
    const NSPoint local = [self convertPoint:event.locationInWindow fromView:nil];
    [self cloverUpdateCursorAt:local];
    [self pushPointer:CLOVER_POINTER_MOVE button:0 event:event];
}

- (void)scrollWheel:(NSEvent*)event {
    if (!_surface) {
        return;
    }
    const NSPoint local = [self convertPoint:event.locationInWindow fromView:nil];
    CGFloat delta = event.scrollingDeltaY;
    if (event.hasPreciseScrollingDeltas) {
        delta /= 10.0;
    }
    if (std::abs(delta) < 0.01) {
        return;
    }
    clover_surface_wheel(_surface, (float)local.x, (float)local.y, (float)delta, 0);
    [self syncSurface];
    [self setNeedsDisplay:YES];
}

- (void)pushPointer:(uint32_t)kind button:(uint32_t)button event:(NSEvent*)event {
    if (!_surface) {
        return;
    }
    const NSPoint local = [self convertPoint:event.locationInWindow fromView:nil];
    clover_surface_pointer(_surface, static_cast<float>(local.x), static_cast<float>(local.y),
                           static_cast<clover_pointer_action>(kind), button, 0);
    [self setNeedsDisplay:YES];
}

@end

namespace CloverPic::Platform::Mac {

bool ShowCloverShellWindow() {
    const char* enabled = std::getenv("CLOVERPIC_CLOVER_SHELL");
    if (!enabled || std::string(enabled) == "0") {
        return false;
    }
    if (g_shellWindow) {
        [g_shellWindow makeKeyAndOrderFront:nil];
        return true;
    }

    const NSRect frame = NSMakeRect(0, 0, 1504, 879);
    const NSWindowStyleMask style =
        NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable;
    NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
                                                   styleMask:style
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    window.title = @"CloverPic — workspace shell (CloverPlatform)";
    window.releasedWhenClosed = NO;

    CloverShellView* view = [[CloverShellView alloc] initWithFrame:frame];
    window.contentView = view;
    [window center];
    [window makeKeyAndOrderFront:nil];
    g_shellWindow = window;
    [view syncSurface];
    return true;
}

} // namespace CloverPic::Platform::Mac
