# AGENTS.md（CloverPic 当前开发约束）

## Toolchain

- OS: Windows 10/11 x64
- C++: C++20, extensions off
- Compiler: MinGW-w64 GCC 13.1.0 from CLion bundled toolchain
- Build: CLion bundled CMake + Ninja
- Runtime: no MSVC runtime dependency

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
  Core/               # platform-independent app model, UI scene, renderer, services
  Platform/Windows/   # first platform adapter, single HWND
  Utils/              # platform-neutral primitive types
```

- `CloverPicCore` is a platform-neutral static library.
- `CloverPic.exe` links `CloverPicCore` with the Windows adapter.
- The old native child-window UI has been removed.
- The old tablet and D2D canvas paths have been removed.

## Core Responsibilities

- App runtime, editor session, commands, modal state.
- UI scene tree, hit testing, hover, focus, capture, modal stack.
- Soft rendering and frame scheduling.
- Project/layer/tile/brush/history/filter logic.
- `.vvp` project serialization.

Core must not include platform headers or own native handles.

## Adapter Responsibilities

- Create the native window/surface.
- Translate platform input into `CloverPic::Input`.
- Present `Core::RgbaFrame` dirty rects.
- Provide `PlatformServices`: file bytes, PNG encoding, file dialogs, recent files, text rasterization, memory/display facts.

See `docs/CoreAdapterAPI.md` for the full boundary contract.

## Important Constraint

Do not reintroduce native UI widgets, child windows, platform message constants, filesystem APIs, or graphics API handles into `src/Core`.
