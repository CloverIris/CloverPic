#import "Platform/Mac/MacCloverDemo.h"

#if CLOVERPIC_HAS_CLOVER_PLATFORM

#import <Cocoa/Cocoa.h>

#include "Platform/Mac/MacCloverIme.h"
#include "clover_platform.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

/// Colours for the host demo live here, not in the markup.
constexpr const char* kHostStyles = R"XAML(
<Styles>
  <Style TargetType="Button" Background="#2F6FD0" Foreground="#FFFFFF"
         CornerRadius="6" BorderThickness="1" BorderBrush="#4C8DE8"/>
  <Style x:Name="canvas" Background="#101215"/>
</Styles>
)XAML";

/// Builds the demo document. The recent-project list is generated so it is long
/// enough to exercise the ScrollViewer.
std::string BuildDemoXaml() {
    std::string xaml = R"XAML(
<StackPanel Background="#1E2023">
  <StackPanel Orientation="Horizontal" Spacing="6" Padding="8" Background="#25282C">
    <Button Content="Brush"  Width="72" x:Name="btnBrush"  Focusable="True" Command="tool:brush"/>
    <Button Content="Eraser" Width="72" x:Name="btnEraser" Focusable="True" Command="tool:eraser"/>
    <Button Content="Layers" Width="72" x:Name="btnLayers" Focusable="True" Command="toggle:canvas"/>
    <TextBox x:Name="filter" Width="200" Height="32" Placeholder="Filter layers..."
             Background="#1B1E22" Foreground="#E8ECF0" CornerRadius="5"
             BorderThickness="1" BorderBrush="#3B4048"/>
  </StackPanel>
  <Panel Width="Fill" Height="Fill" Background="#141618" Padding="14">
    <StackPanel Orientation="Horizontal" Spacing="14" Width="Fill" Height="Fill">
      <Panel Width="190" Background="#232629" Padding="10">
        <StackPanel Spacing="6" Width="Fill" Height="Fill">
          <TextBlock Text="RECENT PROJECTS" FontSize="11" Foreground="#9AA3AD" Height="18"/>
          <ScrollViewer Height="Fill" Width="Fill" x:Name="recent">
)XAML";
    for (int index = 0; index < 14; ++index) {
        char row[256];
        std::snprintf(row, sizeof(row),
                      "            <Panel Height=\"34\" Background=\"#2A2D31\" CornerRadius=\"4\" "
                      "Padding=\"8,0\" VerticalAlignment=\"Center\">"
                      "<TextBlock Text=\"Sketch %02d\" FontSize=\"12\" Foreground=\"#D6DBE1\" "
                      "VerticalAlignment=\"Center\"/></Panel>\n",
                      index + 1);
        xaml += row;
    }
    xaml += R"XAML(
          </ScrollViewer>
        </StackPanel>
      </Panel>
      <Border Background="#1A1C1F" CornerRadius="6" BorderThickness="1" BorderBrush="#3A3F46"
              Width="Fill" Height="Fill">
        <CloverSurface Id="1" Width="Fill" Height="Fill" x:Name="canvas"/>
      </Border>
    </StackPanel>
  </Panel>
  <Panel Height="26" Background="#181A1D" Padding="14,0">
    <TextBlock Text="CloverPlatform 0.3 — scrollable list + direct-memory canvas, hosted via the C ABI"
               FontSize="11" Foreground="#7F8892" VerticalAlignment="Center"/>
  </Panel>
</StackPanel>
)XAML";
    return xaml;
}

/// Premultiplied BGRA content for the canvas region: checkerboard plus two
/// accent bands, so it is visibly live pixels rather than a flat fill.
void PaintCanvas(clover_surface* surface, uint64_t handle) {
    uint32_t* pixels = nullptr;
    size_t stride = 0;
    uint32_t height = 0;
    if (clover_direct_begin_write(surface, handle, &pixels, &stride, &height) != CLOVER_OK) {
        return;
    }
    const uint32_t width = static_cast<uint32_t>(stride);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const bool dark = (((x / 16) + (y / 16)) % 2) == 0;
            const uint32_t v = dark ? 0x3Au : 0x2Eu;
            pixels[y * stride + x] = 0xFF000000u | (v << 16) | (v << 8) | (v + 4u);
        }
    }
    const uint32_t band = height / 7;
    for (uint32_t y = height / 6; y < height / 6 + band && y < height; ++y) {
        for (uint32_t x = width / 8; x < width * 7 / 8 && x < width; ++x) {
            pixels[y * stride + x] = 0xFFD08B3Du;
        }
    }
    for (uint32_t y = height / 2; y < height / 2 + band && y < height; ++y) {
        for (uint32_t x = width / 6; x < width * 5 / 6 && x < width; ++x) {
            pixels[y * stride + x] = 0xFF4A6AE0u;
        }
    }
    clover_direct_commit(surface, handle);
}

} // namespace

@interface CloverPlatformView : CloverImeView
@end

@implementation CloverPlatformView {
    clover_surface* _surface;
    uint64_t _directHandle;
    float _scale;
    NSSize _lastLogicalSize;
    BOOL _canvasSized;
    BOOL _canvasHidden;
}

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        _scale = 1.0f;
        _surface = nullptr;
        _directHandle = 0;
        _lastLogicalSize = NSZeroSize;
        _canvasSized = NO;
        _canvasHidden = NO;
    }
    return self;
}

- (BOOL)isFlipped {
    return YES;
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent*)event {
    (void)event;
    return YES;
}

- (void)dealloc {
    if (_surface) {
        clover_surface_destroy(_surface);
        _surface = nullptr;
    }
}

- (void)viewDidMoveToWindow {
    [super viewDidMoveToWindow];
    if (self.window) {
        [self setCloverSurface:_surface];
        [self syncSurface];
        [self.window makeFirstResponder:self];
    }
}

/// The IME mutates the framework's text field directly, so the action queue has
/// to be drained here as well as on pointer input.
- (void)cloverActionsChanged {
    [self drainActions];
    [self setNeedsDisplay:YES];
}

- (void)setFrameSize:(NSSize)newSize {
    [super setFrameSize:newSize];
    [self syncSurface];
    [self setNeedsDisplay:YES];
}

- (void)viewDidChangeBackingProperties {
    [super viewDidChangeBackingProperties];
    [self syncSurface];
    [self setNeedsDisplay:YES];
}

- (float)currentScale {
    const CGFloat scale = self.window ? self.window.backingScaleFactor : 1.0;
    return scale > 0 ? static_cast<float>(scale) : 1.0f;
}

/// Creates or resizes the CloverPlatform surface to match the view.
- (void)syncSurface {
    const NSRect bounds = self.bounds;
    if (NSWidth(bounds) < 2.0 || NSHeight(bounds) < 2.0) {
        return;
    }
    const float scale = [self currentScale];
    const float logicalWidth = static_cast<float>(NSWidth(bounds));
    const float logicalHeight = static_cast<float>(NSHeight(bounds));

    if (!_surface) {
        const std::string xaml = BuildDemoXaml();
        _surface = clover_surface_create_from_xaml_with_styles(
            logicalWidth, logicalHeight, scale, xaml.c_str(), kHostStyles);
        if (!_surface) {
            std::fprintf(stderr, "[clover] surface creation failed (no usable font?)\n");
            return;
        }
        _scale = scale;
        [self setCloverSurface:_surface];
        _directHandle = clover_surface_add_direct(_surface, 1, 1);
        _canvasSized = NO;
        clover_frame frame{};
        clover_surface_frame(_surface, &frame); // runs the layout pass
        std::fprintf(stderr,
                     "[clover] CloverPlatform surface %.0fx%.0f pt @%.1fx (direct handle %llu)\n",
                     logicalWidth, logicalHeight, scale,
                     static_cast<unsigned long long>(_directHandle));
    } else if (std::fabs(scale - _scale) > 0.01f || !NSEqualSizes(bounds.size, _lastLogicalSize)) {
        _scale = scale;
        _canvasSized = NO;
        clover_surface_resize(_surface, logicalWidth, logicalHeight, scale);
        clover_frame frame{};
        clover_surface_frame(_surface, &frame);
    }
    _lastLogicalSize = bounds.size;

    if (!_canvasSized && _directHandle != 0) {
        clover_rect canvas{};
        if (clover_surface_node_bounds(_surface, "canvas", &canvas) == CLOVER_OK && canvas.w > 1.0f) {
            const auto pixelWidth = static_cast<uint32_t>(canvas.w * _scale + 0.5f);
            const auto pixelHeight = static_cast<uint32_t>(canvas.h * _scale + 0.5f);
            clover_direct_resize(_surface, _directHandle, pixelWidth, pixelHeight);
            PaintCanvas(_surface, _directHandle);
            clover_surface_invalidate(_surface);
            _canvasSized = YES;
            std::fprintf(stderr, "[clover] canvas %.0fx%.0f pt -> %ux%u px\n", canvas.w, canvas.h,
                         pixelWidth, pixelHeight);
        }
    }
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    if (!_surface) {
        return;
    }
    clover_frame frame{};
    if (clover_surface_frame(_surface, &frame) != CLOVER_OK || frame.pixels == nullptr) {
        return;
    }
    CGContextRef context = [[NSGraphicsContext currentContext] CGContext];
    if (!context) {
        return;
    }
    CGDataProviderRef provider = CGDataProviderCreateWithData(
        nullptr, frame.pixels, static_cast<size_t>(frame.stride) * frame.height * 4, nullptr);
    if (!provider) {
        return;
    }
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGImageRef image = CGImageCreate(
        frame.width, frame.height, 8, 32, frame.stride * 4, space,
        static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedFirst) |
            static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little),
        provider, nullptr, false, kCGRenderingIntentDefault);
    if (image) {
        const NSRect bounds = self.bounds;
        CGContextSaveGState(context);
        CGContextSetInterpolationQuality(context, kCGInterpolationNone);
        CGContextTranslateCTM(context, 0, NSHeight(bounds));
        CGContextScaleCTM(context, 1, -1);
        CGContextDrawImage(context, CGRectMake(0, 0, NSWidth(bounds), NSHeight(bounds)), image);
        CGContextRestoreGState(context);
        CGImageRelease(image);
    }
    CGColorSpaceRelease(space);
    CGDataProviderRelease(provider);
}

/// Drains the framework's action queue and reacts. This is the whole host/UI
/// contract: the framework says *what happened*, the host decides what it means.
- (void)drainActions {
    if (!_surface) {
        return;
    }
    char buffer[128];
    for (;;) {
        const int32_t length = clover_surface_take_action(_surface, buffer, sizeof(buffer));
        if (length <= 0) {
            break;
        }
        const std::string action(buffer, static_cast<size_t>(length));
        std::fprintf(stderr, "[clover] action: %s\n", action.c_str());
        if (action == "toggle:canvas") {
            _canvasHidden = !_canvasHidden;
            clover_surface_set_visible(_surface, "canvas", !_canvasHidden);
        }
        [self setNeedsDisplay:YES];
    }
}

- (uint32_t)currentModifiers:(NSEvent*)event {
    uint32_t modifiers = 0;
    if (event.modifierFlags & NSEventModifierFlagShift) modifiers |= CLOVER_MODIFIER_SHIFT;
    if (event.modifierFlags & (NSEventModifierFlagCommand | NSEventModifierFlagControl)) {
        modifiers |= CLOVER_MODIFIER_CONTROL;
    }
    if (event.modifierFlags & NSEventModifierFlagOption) modifiers |= CLOVER_MODIFIER_ALT;
    return modifiers;
}

- (void)pushPointer:(clover_pointer_action)action button:(uint32_t)button event:(NSEvent*)event {
    if (!_surface) {
        return;
    }
    const NSPoint local = [self convertPoint:event.locationInWindow fromView:nil];
    clover_surface_pointer(_surface, static_cast<float>(local.x), static_cast<float>(local.y), action,
                           button, [self currentModifiers:event]);
    [self drainActions];
    [self setNeedsDisplay:YES];
}

- (void)mouseDown:(NSEvent*)event {
    [self pushPointer:CLOVER_POINTER_DOWN button:1 event:event];
}

- (void)mouseUp:(NSEvent*)event {
    [self pushPointer:CLOVER_POINTER_UP button:1 event:event];
}

- (void)mouseDragged:(NSEvent*)event {
    [self pushPointer:CLOVER_POINTER_MOVE button:1 event:event];
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
    clover_surface_wheel(_surface, static_cast<float>(local.x), static_cast<float>(local.y),
                         static_cast<float>(delta), [self currentModifiers:event]);
    [self drainActions];
    [self setNeedsDisplay:YES];
}

- (void)keyDown:(NSEvent*)event {
    if (!_surface) {
        return;
    }
    // Tab is ours; everything else goes through the input system, which routes
    // printable text to insertText: and the rest to doCommandBySelector:.
    if (event.keyCode == 0x30) {
        const uint32_t modifiers =
            (event.modifierFlags & NSEventModifierFlagShift) ? CLOVER_MODIFIER_SHIFT : 0;
        clover_surface_key(_surface, 0x09, CLOVER_KEY_DOWN, modifiers, event.isARepeat);
        [self drainActions];
        [self setNeedsDisplay:YES];
        return;
    }
    [self interpretKeyEvents:@[ event ]];
}

@end

namespace CloverPic::Platform::Mac {

bool ShowCloverPlatformDemoWindow() {
    const char* enabled = std::getenv("CLOVERPIC_CLOVER_DEMO");
    if (!enabled || std::string(enabled) == "0") {
        return false;
    }

    NSWindow* window = [[NSWindow alloc]
        initWithContentRect:NSMakeRect(0, 0, 560, 400)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                             NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
                    backing:NSBackingStoreBuffered
                      defer:NO];
    window.title = @"CloverPic — CloverPlatform surface (C ABI)";
    CloverPlatformView* view =
        [[CloverPlatformView alloc] initWithFrame:NSMakeRect(0, 0, 560, 400)];
    window.contentView = view;
    window.acceptsMouseMovedEvents = YES;
    [window center];
    [window makeKeyAndOrderFront:nil];
    return true;
}

} // namespace CloverPic::Platform::Mac

#endif // CLOVERPIC_HAS_CLOVER_PLATFORM
