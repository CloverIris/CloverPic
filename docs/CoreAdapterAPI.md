# CloverPic Core / Adapter API

This document is the contract between the platform-independent CloverPic core and any platform adapter.

The core owns product behavior: project data, painting state, UI scene, hit testing, command dispatch, frame scheduling, soft rendering, text rasterization, and memory-heavy drawing data. A platform adapter owns only the operating-system capabilities that cannot exist in portable C++: windows, event sources, file dialogs, raw filesystem access, image encoding, system font discovery, display/device facts, and final frame presentation.

## Architectural Rule

`CloverPicCore` must compile without platform headers such as `windows.h`, Cocoa, X11, Wayland, Android, browser DOM, or WASI host APIs.

Adapters may include platform headers freely, but they must translate native concepts into the stable core API before crossing the boundary. The adapter must never expose handles such as `HWND`, `HDC`, platform text objects, image-codec objects, file descriptors, Objective-C objects, DOM nodes, or Java handles to core.

## Runtime Lifecycle

1. The platform executable starts first.
2. The adapter initializes platform libraries required by its implementation, such as COM, image encoding, Win32 DPI awareness, native font enumeration, or browser canvas bindings.
3. The adapter creates a `PlatformServices` bundle and installs it through `CoreServices::InstallPlatformServices`.
4. The adapter creates the Workspace/MainWindow first and instantiates `WorkspaceRuntime` in `NoProject` state.
5. The adapter creates an owned, borderless Program Manager popup and instantiates `ProjectManagerRuntime`.
6. Program Manager can emit a `WorkspaceLaunchRequest`; the adapter hides Program Manager and passes the request into the existing `WorkspaceRuntime`.
7. During initialization, core requests platform facts and font file paths, then initializes its own soft text engine.
8. Whenever viewport size or DPI changes, the adapter calls `RuntimeSurface::Resize(width, height, dpiScale)`.
9. The adapter translates native input into `CloverPic::Input` events and calls `HandlePointer`, `HandleKey`, or `HandleWheel`.
10. The core updates editor state, UI scene state, command state, and dirty regions.
11. The adapter asks `RuntimeSurface::NeedsFrame(nowMs)` from its timer/message loop.
12. When a frame is needed, the adapter calls `RuntimeSurface::Render(nowMs, dirtyRects)`.
13. The adapter presents the returned `RgbaFrame` to the platform surface, clipping to `dirtyRects` where supported.
14. Closing MainWindow exits the process; closing Program Manager hides it when a project is open, or exits when no project exists.

## Adapter Responsibilities

Window and loop:
- Create the native top-level window, browser canvas, or app surface.
- Report viewport size and DPI scale.
- Deliver resize, close, keyboard, pointer, wheel, tablet, and focus events to core.
- Schedule repaint requests when core has pending dirty or animated regions.

Presentation:
- Accept `Core::RgbaFrame`.
- Treat the current MVP frame as `SurfacePixelFormat::Bgra8Unorm`.
- Present only dirty rectangles when practical.
- Do not interpret UI widgets, buttons, layers, tools, layout, or command payloads.

Storage, encoding, and dialogs:
- Implement raw byte reads/writes through `IPlatformFileSystem`.
- Implement PNG/image codec operations through `IImageCodec`, including BGRA8 display PNG export and RGBA16 PNG resources for 10bit raster layers.
- Implement platform file pickers through `IFileDialogService`.
- Implement recent-file persistence through `IRecentFilesStore`.
- Choose platform-appropriate app data locations.
- Do not implement `.cloverpic` parsing in the adapter; core owns the project schema.
- Discover ICC/ICM color profiles through `IColorProfileProvider`.
- Store opaque app settings bytes through `IAppSettingsStore`.

Fonts:
- Implement `IPlatformFontCatalogProvider`.
- Return the system UI family name, an optional preferred system UI font path, and candidate `.ttf` / `.ttc` font file paths.
- Do not rasterize text in the adapter.
- Do not expose platform font API objects to core.

Device and display facts:
- Implement `IMemoryInfoProvider`.
- Implement `IDisplayInfoProvider`.
- Return available physical memory, display pixel size, DPI scale, bits per pixel, color profile path if available, and dynamic range when the platform can determine it.
- Use `DisplayDynamicRange::Unknown` rather than guessing when HDR/SDR cannot be determined reliably.

Tablet and pointer input:
- Translate platform pen/tablet APIs into `Input::PointerEvent`.
- Normalize pressure to `0.0..1.0`.
- Convert positions into core window coordinates before dispatch.
- Preserve pen eraser, tilt, rotation, and contact state when available.

## Core Responsibilities

Application:
- Own `ProjectManagerRuntime`, `WorkspaceRuntime`, `EditorSession`, modal state, and command dispatch.
- Own the `UiScene` tree, node z-order, hit testing, hover, capture, modal blocker, and focus policy.
- Own commands such as new canvas, save, export, undo, redo, layer operations, tool selection, and modal actions.

Rendering:
- Own `SoftRenderer`, visual theme tokens, panels, buttons, swatches, canvas rendering, and frame composition.
- Produce `RgbaFrame` and dirty rectangles.
- Own `FrameScheduler` and decide which areas are static, dirty, or animated.

Text:
- Own `CoreTextEngine`, font face probing, glyph lookup, kerning, glyph caching, and grayscale antialiased glyph rasterization.
- Parse supported TrueType/OpenType-with-`glyf` font data from adapter-provided file paths.
- Use the platform system UI font as the default UI face when it can be found.
- Fall back to another catalog face for missing glyphs.
- Fall back to the built-in debug bitmap font only when core font initialization or glyph loading fails.

Painting model:
- Own projects, layers, tiles, selection masks, brush engine, filters, history, and canvas controller.
- Own large painting allocations and tile lifecycle.
- Use adapter memory information only as advisory input.

Forbidden in core:
- Native windows or child controls.
- Platform message constants as public API.
- Platform handles such as `HWND`, `HDC`, file descriptors, Objective-C objects, DOM nodes, or Java handles.
- Platform-specific filesystem paths except opaque strings returned by adapter services.
- Direct calls to OS file dialogs, color management APIs, DPI APIs, tablet APIs, image encoding APIs, or platform text APIs.

## Current Windows Adapter

The current Windows adapter is `Platform/Windows/WindowsHost`.

It provides:
- A normal Win32 Workspace `HWND` plus an owned borderless Program Manager popup.
- Win32 message pump and event translation.
- Mouse, wheel, keyboard, touch, and Windows pointer pen translation.
- BGRA frame presentation through `StretchDIBits`.
- Dirty-rect clipping during presentation.
- Win32 file dialogs.
- `%AppData%/CloverPic/recent.txt` recent-file storage.
- Raw file byte access for core-owned `.cloverpic` project storage.
- WIC-backed BGRA8 PNG export and RGBA16 PNG resource encoding/decoding.
- Windows ICC/ICM color profile discovery and current display profile lookup.
- System UI font discovery and installed font file enumeration.
- Physical memory and primary display facts.

It must not:
- Create child windows for core UI controls.
- Run old GDI widget painting paths.
- Rasterize application text.
- Understand app commands beyond forwarding input and presenting frames.
- Own editor state, layer state, or project state.

## Adding A New Platform Adapter

Create a new platform directory, for example `src/Platform/Linux`, `src/Platform/macOS`, or `src/Platform/Wasm`.

The adapter must provide:
- A host equivalent to `IPlatformHost`.
- A presenter equivalent to `ISurfacePresenter`.
- Implementations for `IMemoryInfoProvider`, `IDisplayInfoProvider`, `IRecentFilesStore`, `IPlatformFileSystem`, `IImageCodec`, `IFileDialogService`, `IPlatformFontCatalogProvider`, `IColorProfileProvider`, and `IAppSettingsStore`.
- A `PlatformServices` bundle installed before any core runtime is initialized.
- Native-event translation into `CloverPic::Input`.
- A small executable entry point that registers services, creates the host, initializes Workspace plus Program Manager, and runs the platform loop.

If a platform cannot provide a capability, return a safe empty value or `Unknown`. Do not add platform conditionals to core for missing adapter features.

## Stability Promise

The core/adapter boundary should be treated as a stable MVP API. Future platform work should prefer extending interfaces with portable concepts over leaking platform handles into core.
