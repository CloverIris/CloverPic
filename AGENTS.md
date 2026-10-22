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
cmake -S . -B build-mac -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-mac --target CloverPic -j 8
./build-mac/CloverPic
```

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
- It is **not** in the CMake graph. The C++/ObjC++ adapters do not depend on it
  yet, so `cmake` builds are unaffected.
- Build and test it with `cargo`: `cd CloverPlatform && cargo test`.
- It is the target for the UI migration: core-owned UI scenes will eventually be
  expressed as CloverPlatform trees and presented through the C ABI instead of
  `SoftwareRenderer` into `RgbaFrame`.
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
