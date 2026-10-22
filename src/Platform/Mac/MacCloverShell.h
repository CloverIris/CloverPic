#pragma once

namespace CloverPic::Platform::Mac {

/// Hosts the workspace shell from `src/Resource/ui/workspace.cui`.
///
/// Step 1 of the UI migration: the chrome (menu, toolbar, docks, status bar) is
/// layout rather than drawing code. The canvas is still the core surface.
///
/// Enabled with `CLOVERPIC_CLOVER_SHELL=1`.
bool ShowCloverShellWindow();

} // namespace CloverPic::Platform::Mac
