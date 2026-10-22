#import "Platform/Mac/MacCloverIme.h"

#include <cstring>

#if CLOVERPIC_HAS_CLOVER_PLATFORM

@implementation CloverImeView {
    clover_surface* _cloverSurface;
    BOOL _hasMarkedText;
}

- (void)setCloverSurface:(clover_surface*)surface {
    _cloverSurface = surface;
}

- (void)cloverActionsChanged {
}

/// Applies the cursor the framework asks for at a logical point.
- (void)cloverUpdateCursorAt:(NSPoint)point {
    if (!_cloverSurface) {
        return;
    }
    switch (clover_surface_cursor(_cloverSurface, (float)point.x, (float)point.y)) {
        case 1: [[NSCursor pointingHandCursor] set]; break;
        case 2: [[NSCursor IBeamCursor] set]; break;
        case 3: [[NSCursor resizeLeftRightCursor] set]; break;
        case 4: [[NSCursor resizeUpDownCursor] set]; break;
        case 5: [[NSCursor openHandCursor] set]; break;
        case 6: [[NSCursor crosshairCursor] set]; break;
        default: [[NSCursor arrowCursor] set]; break;
    }
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (BOOL)becomeFirstResponder {
    return YES;
}

// ---- NSTextInputClient --------------------------------------------------

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange {
    (void)replacementRange;
    NSString* text = [string isKindOfClass:[NSAttributedString class]]
                         ? [(NSAttributedString*)string string]
                         : (NSString*)string;
    if (_cloverSurface && text.length > 0) {
        clover_surface_ime_insert(_cloverSurface, text.UTF8String);
        _hasMarkedText = NO;
        [self setNeedsDisplay:YES];
        [self cloverActionsChanged];
    }
}

- (void)setMarkedText:(id)string
        selectedRange:(NSRange)selectedRange
     replacementRange:(NSRange)replacementRange {
    (void)selectedRange;
    (void)replacementRange;
    NSString* text = [string isKindOfClass:[NSAttributedString class]]
                         ? [(NSAttributedString*)string string]
                         : (NSString*)string;
    if (!_cloverSurface) {
        return;
    }
    if (text.length == 0) {
        clover_surface_ime_cancel(_cloverSurface);
        _hasMarkedText = NO;
    } else {
        clover_surface_ime_mark(_cloverSurface, text.UTF8String);
        _hasMarkedText = YES;
    }
    [self setNeedsDisplay:YES];
    [self cloverActionsChanged];
}

- (void)unmarkText {
    if (_cloverSurface) {
        clover_surface_ime_cancel(_cloverSurface);
    }
    _hasMarkedText = NO;
    [self setNeedsDisplay:YES];
}

- (BOOL)hasMarkedText {
    return _hasMarkedText;
}

- (NSRange)markedRange {
    // The IME owns its own buffer; we only mirror it for display, so no range is
    // reported back. `hasMarkedText` is what the input system actually reacts to.
    return NSMakeRange(NSNotFound, 0);
}

- (NSRange)selectedRange {
    return NSMakeRange(NSNotFound, 0);
}

- (NSArray<NSAttributedStringKey>*)validAttributesForMarkedText {
    return @[];
}

- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range
                                               actualRange:(NSRangePointer)actualRange {
    (void)range;
    if (actualRange) {
        *actualRange = NSMakeRange(NSNotFound, 0);
    }
    return nil;
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point {
    (void)point;
    return NSNotFound;
}

/// Anchors the candidate window to the framework's caret.
- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actualRange {
    (void)range;
    if (actualRange) {
        *actualRange = NSMakeRange(NSNotFound, 0);
    }
    if (!_cloverSurface) {
        return NSZeroRect;
    }
    clover_rect caret{};
    if (clover_surface_caret_rect(_cloverSurface, &caret) != CLOVER_OK) {
        return NSZeroRect;
    }
    // The view is flipped, so the caret rect is already top-left based; AppKit
    // converts it to window space and then to screen space for us.
    NSRect local = NSMakeRect(caret.x, caret.y, caret.w, caret.h);
    NSRect inWindow = [self convertRect:local toView:nil];
    NSWindow* window = self.window;
    if (!window) {
        return NSZeroRect;
    }
    return [window convertRectToScreen:inWindow];
}

/// Editing commands the input system hands back to us, mapped onto the
/// framework's virtual-key path so there is only one editing implementation.
- (void)doCommandBySelector:(SEL)selector {
    static const struct {
        const char* name;
        uint32_t key;
    } mapping[] = {
        {"deleteBackward:", 0x08},
        {"deleteForward:", 0x2E},
        {"moveLeft:", 0x25},
        {"moveRight:", 0x27},
        {"moveUp:", 0x26},
        {"moveDown:", 0x28},
        {"moveToBeginningOfLine:", 0x24},
        {"moveToEndOfLine:", 0x23},
        {"moveToBeginningOfDocument:", 0x24},
        {"moveToEndOfDocument:", 0x23},
        {"insertNewline:", 0x0D},
        {"insertTab:", 0x09},
    };
    const char* target = sel_getName(selector);
    for (const auto& entry : mapping) {
        if (std::strcmp(entry.name, target) == 0) {
            if (_cloverSurface) {
                clover_surface_key(_cloverSurface, entry.key, CLOVER_KEY_DOWN, 0, false);
                [self setNeedsDisplay:YES];
                [self cloverActionsChanged];
            }
            return;
        }
    }
    [super doCommandBySelector:selector];
}

@end

#endif // CLOVERPIC_HAS_CLOVER_PLATFORM
