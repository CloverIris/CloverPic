# AGENTS.md（CloverPic 当前开发约束）

## Toolchain

- OS: Windows 10/11 x64 (primary) or macOS 13+ on Apple Silicon / Intel
- C++: C++20, extensions off
- Compiler: MinGW-w64 GCC 13.1.0 from CLion bundled toolchain (Windows); Apple Clang (macOS)
- Build: CLion bundled CMake + Ninja (Windows); CMake 3.25+ + Ninja (macOS)
- Runtime: no MSVC runtime dependency
- Target: `CloverPic`

## Build Command

Windows:

```powershell
$mingwBin = "C:\Users\YoCoc\AppData\Local\Programs\CLion\bin\mingw\bin"
$env:PATH = "$mingwBin;$env:PATH"
& "C:\Users\YoCoc\AppData\Local\Programs\CLion\bin\cmake\win\x64\bin\cmake.exe" -S . -B cmake-build-debug -G Ninja
& "C:\Users\YoCoc\AppData\Local\Programs\CLion\bin\cmake\win\x64\bin\cmake.exe" --build cmake-build-debug --target CloverPic -j 14
```

macOS:

```bash
cmake -S . -B build-mac -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-mac --target CloverPic -j 8
./build-mac/CloverPic
```

Use `Release` for anything you are going to *use*. The UI renderer is a per-pixel
CPU loop, and `-O0` measured **7.5x slower** (318 ms vs 42 ms per full frame).
See `docs/UI_PERFORMANCE_REVIEW.md`.

`CMakeLists.txt` selects the adapter by host: `WIN32` builds
`src/main.cpp` + `src/Platform/Windows`, `APPLE` builds `src/Platform/Mac`
(Objective-C++, AppKit). `CloverPicCore` is identical for both.

## Current Architecture

```text
src/
  Core/               # platform-independent app model, UI scene, renderer, document model, services
  Platform/Windows/   # first platform adapter, Win32 host/presenter/services
  Platform/Mac/       # macOS adapter, AppKit host/presenter/services
  Resource/           # vendored source resources such as icon subsets
  Utils/              # platform-neutral primitive types
```

- `CloverPicCore` is a platform-neutral static library.
- `CloverPic` links `CloverPicCore` with exactly one platform adapter.
- The old native child-window UI has been removed.
- The old tablet and D2D canvas paths have been removed.
- Workspace and Program Manager UI are core-owned single-surface scenes.

## macOS Adapter Notes

- Objective-C++ (`.mm`) because the shell is AppKit; `MacHost` mirrors `WindowsHost`
  (two `RuntimeSurface` instances: Program Manager + Workspace) and pumps frames on a
  60 Hz `NSTimer` added to `NSRunLoopCommonModes`.
- `MacSurfaceView` is flipped so core and AppKit share a top-left pixel origin, and it
  forwards input in **backing pixels** (`point * backingScaleFactor`), matching the
  Windows per-monitor-DPI-aware contract.
- Cmd maps to `ModifierControl`, Cmd and Control both set it, so core's Control-based
  shortcuts stay native.
- macOS key codes are translated to Windows virtual-key values, because core's input
  controllers compare against VK codes.
- `CoreTypes.h` names (`Point`, `Rect`, `Size`) collide with legacy MacTypes.h. Inside
  `src/Platform/Mac` always qualify them (`CloverPic::Point`).
- `String` is `std::wstring`, which is 32 bit on Apple platforms. `MacString` normalizes
  at the boundary: conversions back into `String` always emit UTF-16 code units so
  core's UTF-16 text paths keep working.
- The PNG codec is self-contained on zlib (`MacPngCodec`) rather than ImageIO, because
  `CGBitmapContext` only supports premultiplied alpha and would destroy the precision of
  the RGBA10 payloads that `.cloverpic` stores as 16 bit PNG.
- `CLOVERPIC_DATA_DIR` redirects settings/recent-file storage for local testing.

## Core Responsibilities

- App runtime, editor session, commands, modal state.
- UI scene tree, layout, hit testing, hover, focus, capture, modal stack.
- Workspace panel docking, floating, resizing, layer-list scrolling, layer reorder, and UI settings persistence.
- Soft rendering, icon rendering, text rasterization, frame scheduling, thumbnail caches.
- Project/layer/tile/brush/history/filter logic.
- `.cloverpic` project serialization/deserialization.
- RGBA10 internal raster data path and BGRA8 display frame generation.

Core must not include platform headers or own native handles.

## Adapter Responsibilities

- Create the native window/surface.
- Translate platform input into `CloverPic::Input`.
- Present `Core::RgbaFrame` dirty rects.
- Provide `PlatformServices`: file bytes, image codec, file dialogs, recent files, font discovery, color profile discovery, settings bytes, memory/display facts.
- Filter or normalize platform input quirks before dispatching to core, such as Windows promoted mouse messages generated from pen/touch input.

The adapter must not understand UI controls, layer-list semantics, panel behavior, project schema, or editor state.

## Important Constraints

- Do not reintroduce native UI widgets, child windows, platform message constants, filesystem APIs, graphics API handles, image-codec objects, or platform text APIs into `src/Core`.
- Do not implement `.cloverpic` parsing in platform adapter code.
- Do not move editor state, layer state, panel layout state, or command behavior into `src/Platform`.
- When adding a new platform capability, extend the portable service interfaces instead of leaking native handles.

See `docs/CoreAdapterAPI.md` for the full boundary contract.

## CloverPlatform (submodule)

`CloverPlatform/` is a git submodule pointing at
[CloverIris/CloverPlartform](https://github.com/CloverIris/CloverPlartform).

- It is a Rust workspace: `clover-core`, `clover-ui`, `clover-render`,
  `clover-abi`, `clover-demo`.
- On macOS, CMake builds `clover-abi` with cargo and links the static library
  into the executable (optional, guarded by `CLOVERPIC_HAS_CLOVER_PLATFORM`). The
  Windows build does not reference it yet.
- Build and test it with `cargo`: `cd CloverPlatform && cargo test`.
- It is the target for the UI migration: core-owned UI scenes will eventually be
  expressed as CloverPlatform trees and presented through the C ABI instead of
  `SoftwareRenderer` into `RgbaFrame`. Today the demonstration window
  (`MacCloverDemo.mm`) and an alternative Program Manager
  (`MacCloverProgramManager.mm`) use it; the Workspace is still core-drawn.
- Do not copy CloverPlatform sources into `src/`. The submodule boundary is the
  intended coupling; when a capability is missing, extend the framework.

When cloning with submodules: `git clone --recursive`, or
`git submodule update --init` in an existing checkout.

### macOS integration spike

`src/Platform/Mac/MacCloverDemo.mm` hosts a CloverPlatform surface through the C
ABI in its own window. It is additive and off by default:

```bash
CLOVERPIC_CLOVER_DEMO=1 ./build-mac/CloverPic
```

- CMake builds `clover-abi` with cargo and links the static library. If cargo or
  the submodule is missing the adapter still builds, just without the window
  (`CLOVERPIC_HAS_CLOVER_PLATFORM` is then undefined).
- The demo never touches the two core surfaces; it is the proving ground before
  any real UI migrates.
- See `CloverPlatform/docs/MIGRATION.md` for the hosting pattern and the traps
  found so far (damage coordinate space, single damage owner, window key order).

### CloverPlatform Program Manager

The Program Manager can be rendered by CloverPlatform instead of the core scene.
Same `Core::ProjectManagerRuntime`, different presentation:

```bash
CLOVERPIC_CLOVER_PM=1 ./build-mac/CloverPic
```

- `MacCloverProgramManager.mm` builds the page from the recent-files service and
  maps `Command` actions onto `MacHost::HandleProgramManagerCommand`, which calls
  the runtime's public `HandleCommand`.
- Core must not grow UI code for this: a new capability either already exists as
  a command or is added as one.
- Settings still lives in the core-drawn scene; the CloverPlatform page covers
  new-canvas presets, open project, recents and quit.

### CloverPlatform workspace (real integration)

The workspace itself can be rendered by CloverPlatform, in the same window and
driven by the same `Core::WorkspaceRuntime`:

```bash
CLOVERPIC_CLOVER_WORKSPACE=1 CLOVERPIC_NEW_CANVAS=1600x1000 ./build-mac/CloverPic
```

- `src/Core/UI/Workspace/Clover/WorkspaceViewModel` produces a `WorkspaceSnapshot`
  from core state; it holds no window, surface or rectangle.
- `WorkspaceActionRouter` maps `change:<control>:<value>` onto runtime commands.
- `src/Platform/Mac/MacCloverWorkspace.mm` owns the surface, diffs snapshots and
  pushes only what changed (`set_label` / `set_value` / `set_children_xaml`).
- `CLOVERPIC_NEW_CANVAS=WxH` is a development hook that opens a canvas at startup
  so the workspace can be exercised without going through the home screen.
- The canvas is zero-copy: `<CloverSurface x:Name="canvas"/>` is bound to a direct
  region, and `WorkspaceRuntime::RenderCanvasInto` draws the editor straight into
  that region's back buffer. `RgbaFrame::FromBorrowed` is what makes this possible
  without touching the canvas renderer.
- Pointer events inside the canvas box go to the editor and never reach the UI, so
  panels cannot react to strokes.

### Composed UI documents

A page may embed other documents, and the workspace now does:

```xml
<Content x:Name="presets" Source="panels/brush-presets.cui" Height="Fill"/>
```

- `src/Resource/ui/workspace.cui` is the page; `src/Resource/ui/panels/*.cui` are
  panels it embeds.
- Each child owns its tree, renderer and texture; the parent composites the
  texture. A child that fails to load is marked and skipped, never propagated.
- `clover_surface_set_content(slot, path)` swaps a slot's document in place.
- `Source` resolves relative to the document that declares it.
- Bindings do **not** cross document levels yet: a slider moved into a child
  document stops receiving `set_value` from the host. Give a slot its own data
  context before moving interactive controls into one.

### CloverPlatform workspace shell

The workspace chrome (menu bar, toolbar, tool rail, docks, status bar) is now a
document rather than drawing code:

```bash
CLOVERPIC_CLOVER_SHELL=1 ./build-mac/CloverPic
```

- The document is `src/Resource/ui/workspace.cui`, loaded through
  `clover_surface_create_from_cui`; `CLOVERPIC_UI_DOC` overrides the path and
  `CLOVERPIC_UI_DIR` (a compile definition) is the default directory.
- It is additive and off by default: the core-drawn workspace is untouched, so
  the two can be compared side by side while the shell is filled in.
- The canvas region is a placeholder panel; the core surface gets composited
  there in a later step.
- See `docs/UI_REWRITE_PLAN.md` for the remaining steps.
