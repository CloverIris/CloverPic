#pragma once

#import <Cocoa/Cocoa.h>

#include "Core/App/ProjectManagerRuntime.h"
#include "Core/App/WorkspaceRuntime.h"
#include "Platform/Mac/MacSurfaceView.h"

#include <memory>

namespace CloverPic::Platform::Mac {

class MacHost;

} // namespace CloverPic::Platform::Mac

@interface MacAppDelegate : NSObject <NSApplicationDelegate>
@property(nonatomic, assign) CloverPic::Platform::Mac::MacHost* host;
- (void)frameTimerFired:(NSTimer*)timer;
@end

@interface MacWindowDelegate : NSObject <NSWindowDelegate>
@property(nonatomic, assign) CloverPic::Platform::Mac::MacHost* host;
@property(nonatomic, assign) MacSurfaceRole role;
@end

// A borderless NSWindow refuses key status by default; the Program Manager is a
// fully core-drawn borderless surface, so it must be able to become key.
@interface MacBorderlessWindow : NSWindow
@end

namespace CloverPic::Platform::Mac {

// macOS counterpart of WindowsHost: owns the NSApplication shell, the two core
// runtimes (Program Manager + Workspace), their windows, and the frame pump.
class MacHost final {
public:
    MacHost();
    ~MacHost();

    bool Initialize();
    int Run();

    void RequestQuit();
    void OnProgramManagerCloseRequested();
    void OnWorkspaceCloseRequested();
    void FrameTick();

    // Opaque NSWindow* used to parent modal dialogs.
    void* ActiveWindow() const;

    /// Runs a Program Manager command. Used by UI front-ends that are not the
    /// core-drawn scene (CloverPlatform), so both drive the same state machine.
    void HandleProgramManagerCommand(Core::AppCommand command, uint64_t userData = 0,
                                     const String& payload = L"");

private:
    bool ConfirmWorkspaceDestructiveAction(const String& actionLabel);
    void CreateHomeWindow();
    void CreateWorkspaceWindow();
    void OpenWorkspace(Core::WorkspaceLaunchRequest request);
    void PumpRuntimeRequests();
    Size ComputeHomeClientSize() const;
    Size ComputeWorkspaceClientSize() const;
    void InstallMainMenu();

    std::unique_ptr<Core::ProjectManagerRuntime> m_homeRuntime;
    std::unique_ptr<Core::WorkspaceRuntime> m_workspaceRuntime;

    NSWindow* m_homeWindow = nil;
    bool m_useCloverProgramManager = false;
    NSWindow* m_workspaceWindow = nil;
    MacSurfaceView* m_homeView = nil;
    MacSurfaceView* m_workspaceView = nil;

    MacAppDelegate* m_appDelegate = nil;
    MacWindowDelegate* m_homeWindowDelegate = nil;
    MacWindowDelegate* m_workspaceWindowDelegate = nil;
    NSTimer* m_frameTimer = nil;
};

} // namespace CloverPic::Platform::Mac
