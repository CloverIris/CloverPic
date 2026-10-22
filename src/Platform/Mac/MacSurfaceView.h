#pragma once

#import <Cocoa/Cocoa.h>

#include "Core/App/RuntimeSurface.h"

#include <vector>

typedef NS_ENUM(NSInteger, MacSurfaceRole) {
    MacSurfaceRoleHome = 0,
    MacSurfaceRoleWorkspace = 1
};

// Single-surface presenter: core owns every pixel and every UI decision. This
// view only forwards AppKit input as portable events and blits the BGRA8 frame
// core renders.
@interface MacSurfaceView : NSView

- (instancetype)initWithRuntime:(CloverPic::Core::RuntimeSurface*)runtime
                           role:(MacSurfaceRole)role;

// Marks the surface for redraw on the next display cycle.
- (void)requestFrame;

// Called from the host frame timer: redraws when core has a pending frame.
- (void)tickFrame;

@end
