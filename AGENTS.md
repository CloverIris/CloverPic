# AGENTS.md（CloverPic 当前开发约束）

## Toolchain

- OS: Windows 10/11 x64
- C++: C++20, extensions off
- Compiler: MinGW-w64 GCC 13.1.0 from CLion bundled toolchain
- Build: CLion bundled CMake + Ninja
- Runtime: no MSVC runtime dependency
- Target: `CloverPic`

## Build Command

```powershell
$mingwBin = "C:\Users\YoCoc\AppData\Local\Programs\CLion\bin\mingw\bin"
$env:PATH = "$mingwBin;$env:PATH"
& "C:\Users\YoCoc\AppData\Local\Programs\CLion\bin\cmake\win\x64\bin\cmake.exe" -S . -B cmake-build-debug -G Ninja
& "C:\Users\YoCoc\AppData\Local\Programs\CLion\bin\cmake\win\x64\bin\cmake.exe" --build cmake-build-debug --target CloverPic -j 14
```

## Current Architecture

```text
src/
  Core/               # platform-independent app model, UI scene, renderer, document model, services
  Platform/Windows/   # first platform adapter, Win32 host/presenter/services
  Resource/           # vendored source resources such as icon subsets
  Utils/              # platform-neutral primitive types
```

- `CloverPicCore` is a platform-neutral static library.
- `CloverPic.exe` links `CloverPicCore` with the Windows adapter.
- The old native child-window UI has been removed.
- The old tablet and D2D canvas paths have been removed.
- Workspace and Program Manager UI are core-owned single-surface scenes.

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
