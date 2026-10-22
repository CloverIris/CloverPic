#import "Platform/Mac/MacCloverProgramManager.h"

#if CLOVERPIC_HAS_CLOVER_PLATFORM

#import <Cocoa/Cocoa.h>

#include "Platform/Mac/MacCloverDemo.h"
#include "Platform/Mac/MacHost.h"
#include "Platform/Mac/MacString.h"

#include "Core/App/AppCommandPayload.h"
#include "Core/Services/CoreServices.h"
#include "clover_platform.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace CloverPic;
using namespace CloverPic::Platform::Mac;

namespace {

/// Colours live here, not in the markup.
constexpr const char* kProgramManagerStyles = R"XAML(
<Styles>
  <Style TargetType="TextBlock" FontSize="13" Foreground="#E4E8ED"/>
  <Style TargetType="Button" FontSize="13" Padding="18,10" CornerRadius="6"
         Background="#2E6FD0" Foreground="#FFFFFF" BorderThickness="1" BorderBrush="#4C8DE8"/>
  <Style x:Name="root" Background="#15171A"/>
  <Style x:Name="titlebar" Padding="20,16" Background="#1E2126"/>
  <Style x:Name="brand" FontSize="15" Foreground="#FFFFFF"/>
  <Style x:Name="subtitle" FontSize="12" Foreground="#8B939D"/>
  <Style x:Name="sidebar" Background="#1A1C20" Padding="14"/>
  <Style x:Name="section" FontSize="11" Foreground="#8B939D"/>
  <Style x:Name="content" Padding="28" Background="#15171A"/>
  <Style x:Name="note" FontSize="12" Foreground="#8B939D"/>
  <Style x:Name="row" Background="#23262B" CornerRadius="5" Padding="10,0"/>
  <Style x:Name="rowLabel" FontSize="12" Foreground="#D6DBE1"/>
</Styles>
)XAML";

std::string EscapeXml(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char ch : text) {
        switch (ch) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += ch;
        }
    }
    return out;
}

/// Compact label: "…/Parent/Name.cloverpic".
std::string ShortName(const std::string& path) {
    const size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

/// Builds the page from the service layer's recent-file list.
std::string BuildPage(const std::vector<std::string>& recents) {
    std::string xaml = R"XAML(
<StackPanel x:Name="root" Width="Fill" Height="Fill">
  <StackPanel x:Name="titlebar" Orientation="Horizontal" Spacing="10">
    <TextBlock Text="CLOVERPIC" x:Name="brand"/>
    <TextBlock Text="Program Manager" x:Name="subtitle"/>
  </StackPanel>
  <StackPanel x:Name="body" Orientation="Horizontal" Width="Fill" Height="Fill">
    <StackPanel x:Name="sidebar" Width="228" Height="Fill" Spacing="8">
      <TextBlock Text="RECENT PROJECTS" x:Name="section"/>
      <ScrollViewer Height="Fill" Width="Fill" x:Name="recent">
)XAML";
    if (recents.empty()) {
        xaml += R"XAML(
        <TextBlock Text="No recent projects yet" x:Name="note"/>
)XAML";
    } else {
        for (const auto& path : recents) {
            xaml += "        <Panel Height=\"32\" x:Name=\"row\" Command=\"recent:";
            xaml += EscapeXml(path);
            xaml += "\"><TextBlock Text=\"";
            xaml += EscapeXml(ShortName(path));
            xaml += "\" x:Name=\"rowLabel\"/></Panel>\n";
        }
    }
    xaml += R"XAML(
      </ScrollViewer>
    </StackPanel>
    <StackPanel x:Name="content" Width="Fill" Height="Fill" Spacing="14">
      <StackPanel Orientation="Horizontal" Spacing="10">
        <Button Content="New 1600 x 1000" Command="new:1600x1000"/>
        <Button Content="New 1920 x 1080" Command="new:1920x1080"/>
      </StackPanel>
      <StackPanel Orientation="Horizontal" Spacing="10">
        <Button Content="Open Project..." Command="open"/>
        <Button Content="Quit" Command="quit"/>
      </StackPanel>
      <TextBlock Text="This page is drawn by CloverPlatform: layout, text, hit testing and scrolling are the framework's. Project state, recent files and canvas creation still come from CloverPic core through a single command entry point." x:Name="note" TextWrapping="Wrap"/>
    </StackPanel>
  </StackPanel>
</StackPanel>
)XAML";
    return xaml;
}

std::vector<std::string> LoadRecentProjects() {
    std::vector<std::string> out;
    if (auto* store = CloverPic::CoreServices::GetRecentFilesStore()) {
        for (const auto& entry : store->LoadRecentFiles()) {
            out.push_back(ToUtf8(entry));
        }
    }
    return out;
}

} // namespace

@interface CloverProgramManagerView : NSView
- (instancetype)initWithHost:(CloverPic::Platform::Mac::MacHost*)host;
@end

@implementation CloverProgramManagerView {
    clover_surface* _surface;
    CloverPic::Platform::Mac::MacHost* _host;
    float _scale;
    NSSize _lastSize;
}

- (instancetype)initWithHost:(CloverPic::Platform::Mac::MacHost*)host {
    self = [super initWithFrame:NSRectFromCGRect(CGRectMake(0, 0, 760, 520))];
    if (self) {
        _host = host;
        _scale = 1.0f;
        _lastSize = NSZeroSize;
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

- (float)currentScale {
    const CGFloat scale = self.window ? self.window.backingScaleFactor : 1.0;
    return scale > 0 ? static_cast<float>(scale) : 1.0f;
}

- (void)rebuild {
    const std::string xaml = BuildPage(LoadRecentProjects());
    if (!_surface) {
        return;
    }
    if (clover_surface_set_root_xaml(_surface, xaml.c_str(), kProgramManagerStyles) != CLOVER_OK) {
        std::fprintf(stderr, "[clover-pm] page rebuild failed\n");
    }
    [self setNeedsDisplay:YES];
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
        const std::string xaml = BuildPage(LoadRecentProjects());
        _surface = clover_surface_create_from_xaml_with_styles(width, height, scale, xaml.c_str(),
                                                              kProgramManagerStyles);
        if (!_surface) {
            std::fprintf(stderr, "[clover-pm] surface creation failed\n");
            return;
        }
        _scale = scale;
        std::fprintf(stderr, "[clover-pm] Program Manager rendered by CloverPlatform (%.0fx%.0f pt)\n",
                     width, height);
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
        [self syncSurface];
        [self.window makeFirstResponder:self];
    }
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
        CGContextSetInterpolationQuality(context, kCGInterpolationLow);
        CGContextTranslateCTM(context, 0, NSHeight(bounds));
        CGContextScaleCTM(context, 1, -1);
        CGContextDrawImage(context, CGRectMake(0, 0, NSWidth(bounds), NSHeight(bounds)), image);
        CGContextRestoreGState(context);
        CGImageRelease(image);
    }
    CGColorSpaceRelease(space);
    CGDataProviderRelease(provider);
}

/// Turns a framework action into a core command.
- (void)drainActions {
    if (!_surface) {
        return;
    }
    char buffer[1024];
    bool rebuilt = false;
    for (;;) {
        const int32_t length = clover_surface_take_action(_surface, buffer, sizeof(buffer));
        if (length <= 0) {
            break;
        }
        const std::string action(buffer, static_cast<size_t>(length));
        std::fprintf(stderr, "[clover-pm] action: %s\n", action.c_str());
        if (action == "quit") {
            [NSApp terminate:nil];
            return;
        }
        if (action == "open") {
            _host->HandleProgramManagerCommand(CloverPic::Core::AppCommand::OpenProject);
            rebuilt = true;
            continue;
        }
        if (action.rfind("new:", 0) == 0) {
            int w = 1600;
            int h = 1000;
            std::sscanf(action.c_str(), "new:%dx%d", &w, &h);
            _host->HandleProgramManagerCommand(
                CloverPic::Core::AppCommand::CreateCanvasPreset,
                CloverPic::Core::PackCanvasPreset(static_cast<uint32_t>(w), static_cast<uint32_t>(h)));
            continue;
        }
        if (action.rfind("recent:", 0) == 0) {
            const std::string path = action.substr(7);
            _host->HandleProgramManagerCommand(CloverPic::Core::AppCommand::OpenRecentProject, 0,
                                               CloverPic::Platform::Mac::FromUtf8(path));
            continue;
        }
    }
    if (rebuilt) {
        [self rebuild];
    }
}

- (void)pushPointer:(clover_pointer_action)action button:(uint32_t)button event:(NSEvent*)event {
    if (!_surface) {
        return;
    }
    const NSPoint local = [self convertPoint:event.locationInWindow fromView:nil];
    clover_surface_pointer(_surface, static_cast<float>(local.x), static_cast<float>(local.y), action,
                           button, 0);
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
                         static_cast<float>(delta), 0);
    [self setNeedsDisplay:YES];
}

- (void)rebuildFromHost {
    [self rebuild];
}

@end

namespace {

NSWindow* g_programManagerWindow = nil;

} // namespace

namespace CloverPic::Platform::Mac {

bool ShowCloverProgramManager(MacHost& host) {
    const char* enabled = std::getenv("CLOVERPIC_CLOVER_PM");
    if (!enabled || std::string(enabled) == "0") {
        return false;
    }

    if (g_programManagerWindow) {
        [g_programManagerWindow makeKeyAndOrderFront:nil];
        return true;
    }

    NSWindow* window = [[NSWindow alloc]
        initWithContentRect:NSMakeRect(0, 0, 760, 520)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                             NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
                    backing:NSBackingStoreBuffered
                      defer:NO];
    window.title = @"CloverPic — Program Manager (CloverPlatform)";
    CloverProgramManagerView* view =
        [[CloverProgramManagerView alloc] initWithHost:&host];
    window.contentView = view;
    window.acceptsMouseMovedEvents = YES;
    [window center];
    [window makeKeyAndOrderFront:nil];
    g_programManagerWindow = window;
    return true;
}

void ShowCloverProgramManagerWindow() {
    if (g_programManagerWindow) {
        [g_programManagerWindow makeKeyAndOrderFront:nil];
    }
}

void HideCloverProgramManagerWindow() {
    if (g_programManagerWindow) {
        [g_programManagerWindow orderOut:nil];
    }
}

} // namespace CloverPic::Platform::Mac

#endif // CLOVERPIC_HAS_CLOVER_PLATFORM
