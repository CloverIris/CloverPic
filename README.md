# CloverPic

CloverPic is a C++20 / MinGW-w64 digital painting MVP in the Clover Creation series. It aims for a light, fast, SAI-like drawing workflow while keeping the editor model portable enough for future Windows, macOS, Linux, and WASM adapters.

The project now uses a **core-driven single-surface** architecture. Core owns project data, layers, brushes, history, UI scene, layout, hit testing, command dispatch, refresh scheduling, soft text rasterization, and BGRA frame generation. A platform adapter owns only native windows, input translation, raw filesystem access, image encoding, system font discovery, device facts, and presenting core frames to a platform surface.

## Product Goals

- Provide the main illustration workflow: new canvas, drawing, eraser, colors, layers, undo/redo, save, open, and export.
- Keep UI self-rendered by core so buttons, panels, project manager, workspace, hit testing, focus, and dirty refresh are platform-neutral.
- Keep `.vvp` project serialization in core so project files behave consistently across platforms.
- Make new platforms implement adapters instead of rewriting editor logic.
- Keep Windows as the first adapter, not as the architecture.

## Current Architecture

```text
src/
  Core/
    App/              # AppRuntime, scene builder, command dispatcher, canvas controller
    Input/            # Platform-neutral input events
    Platform/         # Core-visible host / presenter boundaries
    Presentation/     # Soft renderer, frame scheduler, UI frame graph
    Render/           # Brush engine, brush presets, tile utilities
    Services/         # PlatformServices and adapter service interfaces
    Text/             # Core-owned TrueType probing and soft glyph rasterization
    UI/               # UiScene / UiNode tree, hit testing, focus, modal state
    *.cpp             # project, layers, filters, history, serializer
  Platform/Windows/   # First adapter: single HWND, input translation, presentation, OS services
  Utils/              # Platform-neutral basic types
```

`CloverPicCore` is a platform-neutral static library. It must not include `windows.h`, `HWND`, `HDC`, native rendering objects, platform text objects, or platform image-codec objects. `CloverPic.exe` is the Windows adapter executable and wraps Windows capabilities into core service interfaces.

## Core / Adapter Boundary

- Core does not create native child windows, read Win32 messages, call file dialogs, call image encoders, or call platform text APIs.
- Adapter does not understand buttons, panels, menus, layer UI, layout semantics, or app commands. It only translates input into core-coordinate events and presents `RgbaFrame`.
- Adapter provides font discovery through `IPlatformFontCatalogProvider`; core owns font parsing, glyph lookup, kerning, glyph cache, and grayscale soft rasterization.
- `.vvp` serialization/deserialization is owned by core. Adapter only provides byte reads/writes.
- PNG/JPEG encoding, recent-file storage, file dialogs, display facts, and memory facts are platform services.
- See [docs/CoreAdapterAPI.md](docs/CoreAdapterAPI.md) for the full boundary contract.

## MVP Features

- Metro-inspired Program Manager with recent projects, search, quick actions, and tile placeholders for future announcements or thumbnails.
- Single-window, single-surface self-rendered UI with dirty-rect refresh scheduling.
- Preset canvas creation, `.vvp` open/save, and PNG export.
- Canvas zoom/pan, brush, eraser, color switching, layer selection, add/delete, and visibility toggles.
- Raster layer and Text layer data models.
- Core-owned soft text rendering based on supported TrueType/OpenType `glyf` fonts discovered by the adapter.
- Undo/redo, recent files, basic filters, mouse/keyboard/wheel/pen input through unified core input events.

## UI Direction

CloverPic's UI is driven by a core-owned `UiScene` tree. Each node carries bounds, z-order, children, focus/hover/capture state, refresh class, accessibility label, and command payload. The platform only knows the viewport size and input coordinates; core decides whether a click hits a button, a tile, a panel, or the canvas.

The visual direction for the MVP is a sharp Metro/Windows 10 inspired surface: square tiles, strong flat color blocks, dark chrome, clear typography, and simple rectangles that are fast for the soft renderer to draw.

## Build

The recommended path is CLion Build / Run. For command-line builds, add CLion bundled MinGW to `PATH` first:

```powershell
$mingwBin = "C:\Users\YoCoc\AppData\Local\Programs\CLion\bin\mingw\bin"
$env:PATH = "$mingwBin;$env:PATH"
& "C:\Users\YoCoc\AppData\Local\Programs\CLion\bin\cmake\win\x64\bin\cmake.exe" -S . -B cmake-build-debug -G Ninja
& "C:\Users\YoCoc\AppData\Local\Programs\CLion\bin\cmake\win\x64\bin\cmake.exe" --build cmake-build-debug --target CloverPic -j 14
```

## Toolchain

- Windows 10/11 x64
- C++20 with `CMAKE_CXX_EXTENSIONS OFF`
- MinGW-w64 GCC 13.1.0
- CLion bundled CMake + Ninja
- Static MinGW runtime linking for easier app distribution
