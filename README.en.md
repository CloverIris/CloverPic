<p align="center">
  <img src="logo.svg" width="320" height="320" alt="VividPic Logo">
</p>

<h1 align="center">VividPic</h1>

<p align="center">
  <a href="README.md">中文</a> | <a href="README.en.md">English</a> | <a href="README.ja.md">日本語</a>
</p>

> ⚠️ **Disclaimer: This is a personal learning project for technical exploration and practice only. It does not represent a final commercial product.**

## Overview

VividPic is a pure C++ native Windows painting application for illustrators and comic artists. Built on a custom lightweight UI framework and rendering pipeline, it aims to deliver low-latency, high-fidelity drawing experiences on Windows 10/11.

The project is actively under development. Core canvas engine, layer system, brush engine, and dual-stack tablet support are already in place.

## Key Features

- **Pure Native Implementation**: Built on Win32 API and Direct2D. No Qt, Electron, or other external UI frameworks.
- **Custom Rendering Pipeline**: CPU soft-compositing + D2D display blit. 256×256 TileGrid chunked memory management with Copy-on-Write.
- **Professional Layer System**: 8 blend modes (Normal, Multiply, Screen, Overlay, Difference, Add, Subtract, Darken, Lighten) with opacity and visibility controls.
- **Brush Engine**: 5 tip types (RoundHard, RoundSoft, Flat, Bristle, Texture) supporting flow, wetMix, spacing, and pressure mapping.
- **Dual-Stack Tablet Support**: Windows Ink API (primary) → WinTab API (fallback) → Mouse. Full pressure, tilt, and rotation support.
- **Memory Awareness**: Automatic safe-memory-budget calculation before canvas creation, with green/yellow/red dashboard indicators.
- **Dark Theme UI**: DPI-aware scaling (`Theme::Scale`), supporting 100%–200% DPI settings.
- **Dockable Panel Layout**: Workspace supports floating and docking of left/right panels.

## Tech Stack

| Category | Technology |
|----------|------------|
| Language | C++20 |
| Platform | Windows 10/11 (x64) |
| Compiler | MinGW-w64 GCC 13.1.0 |
| Build System | CMake 3.25+ / Ninja |
| Rendering | Direct2D / WIC / GDI+ |
| Input | Windows Ink API / WinTab |
| Standard Library | STL / `std::filesystem` |

## Quick Build

### Prerequisites

- Windows 10/11 x64
- CLion (recommended, bundled CMake + MinGW + Ninja)
- Or manually install MinGW-w64 GCC 13+ and CMake 3.25+

### Build in CLion (Recommended)

1. Open the project root in CLion
2. Ensure the Toolchain points to bundled MinGW
3. Click **Build** (Ctrl+F9) or **Run** (Shift+F10)

### Command Line Build

```powershell
# Ensure MinGW\bin is in PATH
$env:PATH = "C:\Users\CloverIris\AppData\Local\Programs\CLion\bin\mingw\bin;$env:PATH"

cmake -B cmake-build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug
```

## Project Structure

```
VividPic/
├── src/
│   ├── App/          # Application singleton, message pump, lifecycle
│   ├── Core/         # Project, Layer, LayerManager, BlendMode, MemoryAdvisor
│   ├── Render/       # RenderBackend, D2DCanvas, TilePool, BrushEngine, BrushPreset
│   ├── Tablet/       # TabletInput (WindowsInkDriver + WinTabDriver + TabletManager)
│   ├── UI/
│   │   ├── Core/     # Window, Theme, message routing base
│   │   ├── Widgets/  # Button, Panel, EditBox, ComboBox, CanvasView
│   │   ├── Panels/   # BrushPanel, ColorsPanel, LayersPanel, NavigatorPanel
│   │   └── Screens/  # HomeScreen, NewCanvasDialog, Workspace
│   └── Utils/        # Types (String, Point, Rect, Size, Color, etc.)
├── assets/           # Runtime assets
├── PRD.md            # Product Requirements Document
├── UI.md             # UI Design Specification
└── AGENTS.md         # Development Constraints & Progress
```

## Roadmap

| Milestone | Status | Content |
|-----------|--------|---------|
| M1 | ✅ | Foundation, HomeScreen, Window/Theme system |
| M2 | ✅ | Canvas engine, Direct2D rendering, Workspace, basic brush |
| M3 | ✅ | TileGrid layer system, 8 blend modes, panels, shortcuts |
| M4 | ✅ | Full brush engine, 5 tips, BrushPanel, WinTab support |
| M5 | 🚧 | Selection tools, transform, fill, text tool, navigator |
| M6 | ⏳ | File I/O (.vvp / PNG / PSD), history/undo, filters |
| M7 | ⏳ | Cloud sync, timelapse, export |
| M8 | ⏳ | Internationalization, settings panel, performance tuning |

## License

This project is a personal learning work and currently has no open-source license. Code is provided for reference and learning purposes only.

---

<p align="center">Made with ❤️ for learning.</p>
