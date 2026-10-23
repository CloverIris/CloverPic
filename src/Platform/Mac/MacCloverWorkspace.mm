#import "Platform/Mac/MacCloverWorkspace.h"

#include "Core/App/WorkspaceRuntime.h"
#include "Core/UI/Workspace/Clover/WorkspaceActionRouter.h"
#include "Core/UI/Workspace/Clover/WorkspaceViewModel.h"
#include "Platform/Mac/MacCloverIme.h"
#include "Platform/Mac/MacString.h"
#include "clover_platform.h"

#include <cstdio>
#include <string>

#ifndef CLOVERPIC_UI_DIR
#define CLOVERPIC_UI_DIR "src/Resource/ui"
#endif

using CloverPic::Core::WorkspaceSnapshot;
using CloverPic::Platform::Mac::ToUtf8;

@implementation CloverWorkspaceView {
    CloverPic::Core::WorkspaceRuntime* _runtime;
    std::unique_ptr<CloverPic::Core::WorkspaceActionRouter> _router;
    clover_surface* _surface;
    NSSize _lastSize;
    float _scale;
    NSTimer* _syncTimer;
    WorkspaceSnapshot _last;
    bool _hasLast;
    std::string _lastLayerMarkup;
}

- (instancetype)initWithRuntime:(CloverPic::Core::WorkspaceRuntime*)runtime {
    self = [super initWithFrame:NSMakeRect(0, 0, 1504, 879)];
    if (self) {
        _runtime = runtime;
        _router = std::make_unique<CloverPic::Core::WorkspaceActionRouter>(*runtime);
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

- (float)currentScale {
    NSWindow* window = self.window;
    return window ? static_cast<float>(window.backingScaleFactor) : 1.0f;
}

- (void)viewDidMoveToWindow {
    [super viewDidMoveToWindow];
    if (!self.window) {
        return;
    }
    [self createSurfaceIfNeeded];
    [self.window makeFirstResponder:self];
    [self syncFromRuntime];
    _syncTimer = [NSTimer timerWithTimeInterval:1.0 / 30.0
                                         target:self
                                       selector:@selector(timerFired:)
                                       userInfo:nil
                                        repeats:YES];
    [[NSRunLoop mainRunLoop] addTimer:_syncTimer forMode:NSRunLoopCommonModes];
}

- (void)timerFired:(NSTimer*)timer {
    (void)timer;
    [self syncFromRuntime];
    [self setNeedsDisplay:YES];
}

- (void)createSurfaceIfNeeded {
    const NSRect bounds = self.bounds;
    if (NSWidth(bounds) < 2.0 || NSHeight(bounds) < 2.0) {
        return;
    }
    const float scale = [self currentScale];
    const std::string path = std::string(CLOVERPIC_UI_DIR) + "/workspace.cui";
    _surface = clover_surface_create_from_cui(static_cast<float>(NSWidth(bounds)),
                                              static_cast<float>(NSHeight(bounds)), scale,
                                              path.c_str());
    if (!_surface) {
        std::fprintf(stderr, "[clover-workspace] failed to load %s\n", path.c_str());
        return;
    }
    _scale = scale;
    _lastSize = bounds.size;
    std::fprintf(stderr, "[clover-workspace] shell loaded from %s\n", path.c_str());
}

- (void)pushLabel:(const char*)name value:(const std::string&)value {
    if (_surface) {
        clover_surface_set_label(_surface, name, value.c_str());
    }
}

- (void)syncFromRuntime {
    if (!_runtime) {
        return;
    }
    const WorkspaceSnapshot snapshot = CloverPic::Core::CaptureWorkspaceSnapshot(
        _runtime->Editor(), _runtime->UiState(), _runtime->Session().GetCurrentFilePath());

    if (!_hasLast || snapshot.documentName != _last.documentName) {
        [self pushLabel:"statusDoc" value:ToUtf8(snapshot.documentName)];
    }
    if (!_hasLast || snapshot.activeTool != _last.activeTool) {
        [self pushLabel:"statusTool" value:ToUtf8(snapshot.activeTool)];
    }
    if (!_hasLast || snapshot.zoomPercent != _last.zoomPercent) {
        [self pushLabel:"statusZoom" value:"ZOOM " + std::to_string(snapshot.zoomPercent) + "%"];
    }
    if (!_hasLast || snapshot.brushSize != _last.brushSize) {
        [self pushLabel:"statusSize" value:"SIZE " + std::to_string(static_cast<int>(snapshot.brushSize + 0.5f))];
        clover_surface_set_value(_surface, "size", snapshot.brushSize);
    }
    if (!_hasLast || snapshot.brushOpacity != _last.brushOpacity) {
        clover_surface_set_value(_surface, "opacity", snapshot.brushOpacity * 100.0f);
    }
    if (!_hasLast || snapshot.blendModes != _last.blendModes) {
        std::string csv;
        for (size_t index = 0; index < snapshot.blendModes.size(); ++index) {
            if (index) {
                csv += ",";
            }
            csv += ToUtf8(snapshot.blendModes[index]);
        }
        clover_surface_set_items(_surface, "blend", csv.c_str());
    }
    if (!_hasLast || snapshot.blendIndex != _last.blendIndex) {
        clover_surface_set_selected(_surface, "blend", snapshot.blendIndex);
    }
    if (!_hasLast || snapshot.canvasWidth != _last.canvasWidth ||
        snapshot.canvasHeight != _last.canvasHeight) {
        [self pushLabel:"statusCanvas" value:std::to_string(snapshot.canvasWidth) + " " +
                                            std::to_string(snapshot.canvasHeight)];
    }

    // The layer list is the only structural part: rebuild it when it really
    // changed, not every tick.
    std::string markup;
    for (size_t index = 0; index < snapshot.layers.size(); ++index) {
        const auto& row = snapshot.layers[index];
        markup += "<Panel Height=\"28\" Width=\"Fill\" Padding=\"8,0\" ";
        markup += row.active ? "Background=\"#3B6FD4\" Command=\"layer:select:"
                             : "Background=\"#22262B\" Command=\"layer:select:";
        markup += std::to_string(index);
        markup += "\"><Panel Width=\"18\" Height=\"18\" Background=\"";
        markup += row.visible ? "#FFFFFF" : "#4A4A4A";
        markup += "\" CornerRadius=\"2\" Margin=\"0,0,8,0\"/>";
        markup += "<TextBlock Text=\"";
        markup += ToUtf8(row.name);
        markup += "\" Foreground=\"";
        markup += row.active ? "#FFFFFF" : "#C8C8C8";
        markup += "\"/></Panel>";
    }
    if (markup != _lastLayerMarkup) {
        clover_surface_set_children_xaml(_surface, "layerList",
                                         ("<Panel>" + markup + "</Panel>").c_str());
        _lastLayerMarkup = markup;
    }

    _last = snapshot;
    _hasLast = true;
}

- (void)handleAction:(const std::string&)action {
    if (_router && _router->Handle(action)) {
        return;
    }
    std::fprintf(stderr, "[clover-workspace] unhandled action: %s\n", action.c_str());
}

- (void)pushPointer:(clover_pointer_action)kind button:(uint32_t)button event:(NSEvent*)event {
    if (!_surface) {
        return;
    }
    const NSPoint local = [self convertPoint:event.locationInWindow fromView:nil];
    clover_surface_pointer(_surface, static_cast<float>(local.x), static_cast<float>(local.y), kind,
                           button, 0);
    [self setNeedsDisplay:YES];
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

- (void)mouseMoved:(NSEvent*)event {
    if (!_surface) {
        return;
    }
    const NSPoint local = [self convertPoint:event.locationInWindow fromView:nil];
    switch (clover_surface_cursor(_surface, static_cast<float>(local.x),
                                 static_cast<float>(local.y))) {
        case 1: [[NSCursor pointingHandCursor] set]; break;
        case 2: [[NSCursor IBeamCursor] set]; break;
        case 3: [[NSCursor resizeLeftRightCursor] set]; break;
        case 4: [[NSCursor resizeUpDownCursor] set]; break;
        case 5: [[NSCursor openHandCursor] set]; break;
        case 6: [[NSCursor crosshairCursor] set]; break;
        default: [[NSCursor arrowCursor] set]; break;
    }
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
    [self setNeedsDisplay:YES];
}

- (void)keyDown:(NSEvent*)event {
    if (!_surface) {
        return;
    }
    [self interpretKeyEvents:@[ event ]];
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    if (!_surface) {
        [self createSurfaceIfNeeded];
        if (!_surface) {
            return;
        }
    }
    if (!NSEqualSizes(self.bounds.size, _lastSize)) {
        _lastSize = self.bounds.size;
        clover_surface_resize(_surface, static_cast<float>(NSWidth(self.bounds)),
                              static_cast<float>(NSHeight(self.bounds)), [self currentScale]);
    }
    [self syncFromRuntime];

    clover_frame frame{};
    if (clover_surface_frame(_surface, &frame) != CLOVER_OK || !frame.pixels) {
        return;
    }
    char buffer[512];
    while (clover_surface_take_action(_surface, buffer, sizeof(buffer)) > 0) {
        [self handleAction:std::string(buffer)];
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
    CGContextDrawImage(target, CGRectMake(0.0, 0.0, NSWidth(self.bounds), NSHeight(self.bounds)),
                       image);
    CGContextRestoreGState(target);
    CGImageRelease(image);
}

@end
