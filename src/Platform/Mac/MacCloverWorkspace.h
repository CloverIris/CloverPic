#pragma once

#import <Cocoa/Cocoa.h>

#import "Platform/Mac/MacCloverIme.h"

namespace CloverPic::Core {
class WorkspaceRuntime;
}

/// A workspace view rendered by CloverPlatform instead of the core scene.
///
/// This is the real migration target: the same `Core::WorkspaceRuntime`, the same
/// window, a different presentation. It pulls a `WorkspaceSnapshot` from core and
/// pushes only what changed, and routes widget actions back through
/// `Core::WorkspaceActionRouter`.
@interface CloverWorkspaceView : CloverImeView

- (instancetype)initWithRuntime:(CloverPic::Core::WorkspaceRuntime*)runtime;

/// Pulls a snapshot and pushes it into the surface.
- (void)syncFromRuntime;

@end
