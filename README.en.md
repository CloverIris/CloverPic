# CloverPic

CloverPic is a C++20 / MinGW-w64 painting-app MVP. The current architecture is **core-driven single-surface**: the core owns the project model, layers, brush engine, UI scene, command dispatch, hit testing, frame scheduling, and RGBA frame generation. A platform adapter only owns windows, input translation, file/image/font/device services, and final presentation.

See `docs/CoreAdapterAPI.md` for the core/adapter contract.

## Layout

```text
src/
  Core/               # portable app model, scene, rendering, services
  Platform/Windows/   # first single-window adapter
  Utils/              # portable basic types
```

`CloverPicCore` builds as a platform-neutral static library. `CloverPic.exe` links that core with the Windows adapter.

## MVP Features

- New preset canvas, open/save `.vvp`, export PNG.
- Soft-rendered single-surface UI with dirty-rect scheduling.
- Canvas zoom/pan, brush, eraser, basic color switching.
- Layer select/add/delete/visibility.
- Undo/Redo, recent files, platform text rasterization API.

## Build

Use CLion bundled CMake and MinGW-w64 GCC 13.1.0:

```powershell
$mingwBin = "C:\Users\YoCoc\AppData\Local\Programs\CLion\bin\mingw\bin"
$env:PATH = "$mingwBin;$env:PATH"
& "C:\Users\YoCoc\AppData\Local\Programs\CLion\bin\cmake\win\x64\bin\cmake.exe" -S . -B cmake-build-debug -G Ninja
& "C:\Users\YoCoc\AppData\Local\Programs\CLion\bin\cmake\win\x64\bin\cmake.exe" --build cmake-build-debug --target CloverPic -j 14
```

