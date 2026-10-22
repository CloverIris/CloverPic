#pragma once

#import <Cocoa/Cocoa.h>

#if CLOVERPIC_HAS_CLOVER_PLATFORM
#include "clover_platform.h"
#endif

/// Base view for CloverPlatform surfaces that accepts text input.
///
/// It implements `NSTextInputClient` so the system input method (including the
/// Chinese IME) can compose text: marked text becomes the framework's inline
/// composition, committed text is inserted at the caret, and the candidate
/// window is anchored to the framework's caret rectangle.
@interface CloverImeView : NSView <NSTextInputClient>

#if CLOVERPIC_HAS_CLOVER_PLATFORM
- (void)setCloverSurface:(clover_surface*)surface;
#endif

/// Called after any text mutation so subclasses can drain the action queue and
/// repaint. Default implementation does nothing.
- (void)cloverActionsChanged;

@end
