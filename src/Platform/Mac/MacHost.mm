#import "Platform/Mac/MacHost.h"

#include "Platform/Mac/MacCloverDemo.h"
#include "Platform/Mac/MacCloverProgramManager.h"
#include "Platform/Mac/MacCloverShell.h"
#include "Platform/Mac/MacCoreServices.h"
#include "Platform/Mac/MacString.h"
#include "Platform/Mac/MacTime.h"

#include <algorithm>
#include <cstdio>

using namespace CloverPic;

@implementation MacAppDelegate
- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    (void)notification;
    [NSApp activateIgnoringOtherApps:YES];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    (void)sender;
    // The host decides when the app is done: closing the Program Manager while a
    // workspace is open only returns to the workspace.
    return NO;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender {
    (void)sender;
    return NSTerminateNow;
}

- (void)frameTimerFired:(NSTimer*)timer {
    (void)timer;
    if (self.host) self.host->FrameTick();
}
@end

@implementation MacWindowDelegate
- (BOOL)windowShouldClose:(NSWindow*)sender {
    (void)sender;
    if (self.host) {
        if (self.role == MacSurfaceRoleHome) {
            self.host->OnProgramManagerCloseRequested();
        } else {
            self.host->OnWorkspaceCloseRequested();
        }
    }
    // The host performs its own teardown; never let AppKit also close the window.
    return NO;
}
@end

@implementation MacBorderlessWindow
- (BOOL)canBecomeKeyWindow {
    return YES;
}
- (BOOL)canBecomeMainWindow {
    return YES;
}
@end

namespace CloverPic::Platform::Mac {

namespace {

NSRect VisibleScreenFrame() {
    NSScreen* screen = [NSScreen mainScreen];
    if (!screen) return NSMakeRect(0, 0, 1440, 900);
    return screen.visibleFrame;
}

} // namespace

MacHost::MacHost() = default;

MacHost::~MacHost() {
    if (m_frameTimer) {
        [m_frameTimer invalidate];
        m_frameTimer = nil;
    }
    if (m_homeWindow) {
        m_homeWindow.delegate = nil;
        [m_homeWindow orderOut:nil];
        m_homeWindow = nil;
    }
    if (m_workspaceWindow) {
        m_workspaceWindow.delegate = nil;
        [m_workspaceWindow orderOut:nil];
        m_workspaceWindow = nil;
    }
}

bool MacHost::Initialize() {
    NSApplication* app = [NSApplication sharedApplication];
    [app setActivationPolicy:NSApplicationActivationPolicyRegular];

    m_appDelegate = [[MacAppDelegate alloc] init];
    m_appDelegate.host = this;
    app.delegate = m_appDelegate;

    InstallMainMenu();

    RegisterMacCoreServices([this]() -> void* { return ActiveWindow(); });

    CreateWorkspaceWindow();
    CreateHomeWindow();

    m_frameTimer = [NSTimer timerWithTimeInterval:1.0 / 60.0
                                           target:m_appDelegate
                                         selector:@selector(frameTimerFired:)
                                         userInfo:nil
                                          repeats:YES];
    if (m_frameTimer) {
        [[NSRunLoop mainRunLoop] addTimer:m_frameTimer forMode:NSRunLoopCommonModes];
    }

    [app activateIgnoringOtherApps:YES];
    if (m_homeWindow) {
        [m_homeWindow makeKeyAndOrderFront:nil];
    } else if (m_workspaceWindow) {
        [m_workspaceWindow makeKeyAndOrderFront:nil];
    }

    // Optional: an additive CloverPlatform window, enabled with
    // CLOVERPIC_CLOVER_DEMO=1. Created last so it ends up front and key, and it
    // never touches the two core surfaces.
    ShowCloverPlatformDemoWindow();

    // Optional: the workspace shell rendered from src/Resource/ui/workspace.cui.
    ShowCloverShellWindow();

    // Optional: render the Program Manager with CloverPlatform instead of the
    // core-drawn scene. Same state machine, different presentation.
    m_useCloverProgramManager = ShowCloverProgramManager(*this);
    if (m_useCloverProgramManager && m_homeWindow) {
        [m_homeWindow orderOut:nil];
    }

    return m_workspaceWindow != nil && m_homeWindow != nil;
}

int MacHost::Run() {
    [NSApp run];
    return 0;
}

void MacHost::HandleProgramManagerCommand(Core::AppCommand command, uint64_t userData,
                                          const String& payload) {
    if (m_homeRuntime) {
        m_homeRuntime->HandleCommand(command, userData, payload);
    }
}

void* MacHost::ActiveWindow() const {
    NSWindow* key = [NSApp keyWindow];
    if (key) return (__bridge void*)key;
    if (m_homeWindow && m_homeWindow.isVisible) return (__bridge void*)m_homeWindow;
    return m_workspaceWindow ? (__bridge void*)m_workspaceWindow : nullptr;
}

void MacHost::RequestQuit() {
    if (m_frameTimer) {
        [m_frameTimer invalidate];
        m_frameTimer = nil;
    }
    [NSApp terminate:nil];
}

void MacHost::OnProgramManagerCloseRequested() {
    if (m_workspaceRuntime && m_workspaceWindow) {
        if (m_homeRuntime) m_homeRuntime->ClearQuitRequest();
        if (m_useCloverProgramManager) {
            HideCloverProgramManagerWindow();
        } else if (m_homeWindow) {
            [m_homeWindow orderOut:nil];
        }
        [m_workspaceWindow makeKeyAndOrderFront:nil];
        if (m_workspaceView) [m_workspaceView requestFrame];
        return;
    }
    RequestQuit();
}

void MacHost::OnWorkspaceCloseRequested() {
    if (!ConfirmWorkspaceDestructiveAction(L"closing CloverPic")) {
        if (m_workspaceRuntime) m_workspaceRuntime->ClearQuitRequest();
        return;
    }
    RequestQuit();
}

void MacHost::FrameTick() {
    PumpRuntimeRequests();

    if (m_workspaceView) [m_workspaceView tickFrame];
    if (m_homeView && m_homeWindow.isVisible) [m_homeView tickFrame];

    // Core sets a quit request for a surface when its own UI asks to close
    // (Program Manager close tile, or an in-canvas quit command).
    if (m_homeRuntime && m_homeRuntime->WantsQuit() && m_homeWindow.isVisible) {
        m_homeRuntime->ClearQuitRequest();
        OnProgramManagerCloseRequested();
    }
    if (m_workspaceRuntime && m_workspaceRuntime->WantsQuit()) {
        OnWorkspaceCloseRequested();
    }
}

bool MacHost::ConfirmWorkspaceDestructiveAction(const String& actionLabel) {
    if (!m_workspaceRuntime || !m_workspaceRuntime->HasUnsavedChanges()) {
        return true;
    }
    auto* dialogs = CoreServices::GetFileDialogService();
    if (!dialogs) return false;
    switch (dialogs->PromptUnsavedChanges(actionLabel)) {
        case UnsavedChangesChoice::Save:
            return m_workspaceRuntime->SaveCurrentProjectInteractive(false);
        case UnsavedChangesChoice::Discard:
            return true;
        case UnsavedChangesChoice::Cancel:
        default:
            break;
    }
    return false;
}

void MacHost::CreateWorkspaceWindow() {
    const Size client = ComputeWorkspaceClientSize();

    m_workspaceRuntime = std::make_unique<Core::WorkspaceRuntime>(Core::WorkspaceLaunchRequest{});
    m_workspaceView = [[MacSurfaceView alloc] initWithRuntime:m_workspaceRuntime.get()
                                                        role:MacSurfaceRoleWorkspace];

    m_workspaceWindow = [[NSWindow alloc]
        initWithContentRect:NSMakeRect(0, 0, client.width, client.height)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                             NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
                    backing:NSBackingStoreBuffered
                      defer:NO];
    m_workspaceWindow.title = @"CloverPic";
    m_workspaceWindow.minSize = NSMakeSize(900, 600);
    m_workspaceWindow.contentView = m_workspaceView;
    m_workspaceWindow.acceptsMouseMovedEvents = YES;
    [m_workspaceWindow center];

    m_workspaceWindowDelegate = [[MacWindowDelegate alloc] init];
    m_workspaceWindowDelegate.host = this;
    m_workspaceWindowDelegate.role = MacSurfaceRoleWorkspace;
    m_workspaceWindow.delegate = m_workspaceWindowDelegate;

    [m_workspaceWindow makeKeyAndOrderFront:nil];
    std::fprintf(stderr, "[mac] workspace window %dx%d pt\n", client.width, client.height);
}

void MacHost::CreateHomeWindow() {
    const Size client = ComputeHomeClientSize();

    m_homeRuntime = std::make_unique<Core::ProjectManagerRuntime>();
    m_homeView = [[MacSurfaceView alloc] initWithRuntime:m_homeRuntime.get()
                                                   role:MacSurfaceRoleHome];

    m_homeWindow = [[MacBorderlessWindow alloc]
        initWithContentRect:NSMakeRect(0, 0, client.width, client.height)
                  styleMask:NSWindowStyleMaskBorderless
                    backing:NSBackingStoreBuffered
                      defer:NO];
    m_homeWindow.opaque = YES;
    m_homeWindow.backgroundColor = [NSColor blackColor];
    m_homeWindow.hasShadow = YES;
    m_homeWindow.movableByWindowBackground = NO;
    m_homeWindow.contentView = m_homeView;
    m_homeWindow.acceptsMouseMovedEvents = YES;
    [m_homeWindow center];

    m_homeWindowDelegate = [[MacWindowDelegate alloc] init];
    m_homeWindowDelegate.host = this;
    m_homeWindowDelegate.role = MacSurfaceRoleHome;
    m_homeWindow.delegate = m_homeWindowDelegate;

    // Mirrors the Windows owned-window relationship: the borderless Program
    // Manager belongs to the workspace window.
    if (m_workspaceWindow) {
        [m_workspaceWindow addChildWindow:m_homeWindow ordered:NSWindowAbove];
    }
    [m_homeWindow makeKeyAndOrderFront:nil];
    std::fprintf(stderr, "[mac] program manager window %dx%d pt\n", client.width, client.height);
}

void MacHost::OpenWorkspace(Core::WorkspaceLaunchRequest request) {
    if (!m_workspaceRuntime || !m_workspaceWindow) {
        CreateWorkspaceWindow();
    }
    if (request.kind != Core::WorkspaceLaunchKind::None &&
        !ConfirmWorkspaceDestructiveAction(request.kind == Core::WorkspaceLaunchKind::OpenProject
                                               ? L"opening another project"
                                               : L"creating a new image")) {
        return;
    }
    m_workspaceRuntime->AcceptWorkspaceLaunchRequest(std::move(request));
    [m_workspaceWindow makeKeyAndOrderFront:nil];
    if (m_useCloverProgramManager) {
        HideCloverProgramManagerWindow();
    } else if (m_homeWindow) {
        [m_homeWindow orderOut:nil];
    }
    if (m_workspaceView) [m_workspaceView requestFrame];
}

void MacHost::PumpRuntimeRequests() {
    if (m_workspaceRuntime && m_workspaceRuntime->ConsumeProgramManagerRequest()) {
        const bool showSettings = m_workspaceRuntime->ConsumeProgramManagerSettingsRequest();
        if (m_homeRuntime) {
            m_homeRuntime->RefreshRecentFiles();
            if (showSettings) m_homeRuntime->ShowSettingsPage();
        }
        if (m_useCloverProgramManager) {
            ShowCloverProgramManagerWindow();
        } else if (m_homeWindow) {
            [m_homeWindow makeKeyAndOrderFront:nil];
            if (m_homeView) [m_homeView requestFrame];
        }
    }
    if (m_homeRuntime && m_homeRuntime->HasLaunchRequest()) {
        OpenWorkspace(m_homeRuntime->ConsumeLaunchRequest());
    }
}

Size MacHost::ComputeHomeClientSize() const {
    const NSRect visible = VisibleScreenFrame();
    const int workW = static_cast<int>(visible.size.width);
    const int workH = static_cast<int>(visible.size.height);

    int clientH = std::clamp(static_cast<int>(workH * 0.62f), 570, 840);
    int clientW = clientH * 4 / 3;
    const int maxW = std::min(1120, static_cast<int>(workW * 0.78f));
    if (clientW > maxW) {
        clientW = std::max(760, maxW);
        clientH = clientW * 3 / 4;
    }
    clientW = std::clamp(clientW, 760, 1120);
    clientH = std::clamp(clientH, 570, 840);
    return Size(clientW, clientH);
}

Size MacHost::ComputeWorkspaceClientSize() const {
    const NSRect visible = VisibleScreenFrame();
    const int workW = static_cast<int>(visible.size.width);
    const int workH = static_cast<int>(visible.size.height);
    return Size(std::clamp(static_cast<int>(workW * 0.88f), 1180, 1920),
                std::clamp(static_cast<int>(workH * 0.86f), 780, 1280));
}

void MacHost::InstallMainMenu() {
    NSMenu* mainMenu = [[NSMenu alloc] init];

    NSMenuItem* appMenuItem = [[NSMenuItem alloc] init];
    [mainMenu addItem:appMenuItem];
    NSMenu* appMenu = [[NSMenu alloc] init];
    [appMenu addItemWithTitle:@"About CloverPic"
                       action:@selector(orderFrontStandardAboutPanel:)
                keyEquivalent:@""];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItemWithTitle:@"Hide CloverPic" action:@selector(hide:) keyEquivalent:@"h"];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItemWithTitle:@"Quit CloverPic" action:@selector(terminate:) keyEquivalent:@"q"];
    appMenuItem.submenu = appMenu;

    NSMenuItem* windowMenuItem = [[NSMenuItem alloc] init];
    [mainMenu addItem:windowMenuItem];
    NSMenu* windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];
    [windowMenu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
    [windowMenu addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];
    windowMenuItem.submenu = windowMenu;
    [NSApp setWindowsMenu:windowMenu];

    [NSApp setMainMenu:mainMenu];
}

} // namespace CloverPic::Platform::Mac
