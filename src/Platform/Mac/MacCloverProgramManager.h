#pragma once

namespace CloverPic::Platform::Mac {

class MacHost;

/// Renders the Program Manager with CloverPlatform instead of the core-drawn
/// scene, driving the same `Core::ProjectManagerRuntime` state machine.
///
/// This is the migration spike: project state stays in core, the presentation
/// moves to the framework. Enabled with `CLOVERPIC_CLOVER_PM=1`.
bool ShowCloverProgramManager(MacHost& host);

/// Re-shows the CloverPlatform Program Manager if it was created before.
void ShowCloverProgramManagerWindow();

/// Hides it, e.g. when a project opens and the workspace takes over.
void HideCloverProgramManagerWindow();

} // namespace CloverPic::Platform::Mac
