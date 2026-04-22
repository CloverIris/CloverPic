# AGENTS.md（开发环境、技术约束与项目进度）

> **本文件为 KimiCode 开发 VividPic 的强制约束与参考文档。所有代码生成、架构决策、依赖选择、编译调试必须优先遵循以下条款。**

---

## 1. 开发环境与工具链（绝对约束）

### 1.1 系统与编译器

| 约束项 | 强制要求 | 说明 |
|--------|----------|------|
| **操作系统** | Windows 10/11 (x64) | 目标运行平台，开发调试必须基于此 |
| **C++ 标准** | C++20 (`-std=c++20`) | extensions off，禁止使用 GNU 扩展 |
| **编译器** | MinGW-w64 GCC 13.1.0 | **禁止**使用 MSVC 或 Clang-CL |
| **构建系统** | CMake 3.25+ (CLion bundled) | 生成 Ninja 构建文件 |
| **链接器** | ld (MinGW 自带) | 兼容 Windows 10 SDK，不依赖 MSVC runtime |
| **调试器** | GDB (MinGW 自带) | CLion 集成调试 |

### 1.2 工具链绝对路径

以下路径在本机上固定，如迁移环境需同步更新：

```
G++ 编译器:
  C:\Users\CloverIris\AppData\Local\Programs\CLion\bin\mingw\bin\g++.exe

C 编译器:
  C:\Users\CloverIris\AppData\Local\Programs\CLion\bin\mingw\bin\gcc.exe

C++ 前端 (cc1plus，g++ 内部调用):
  C:\Users\CloverIris\AppData\Local\Programs\CLion\bin\mingw\libexec\gcc\x86_64-w64-mingw32\13.1.0\cc1plus.exe

CMake:
  C:\Users\CloverIris\AppData\Local\Programs\CLion\bin\cmake\win\x64\bin\cmake.exe

Ninja (构建系统):
  C:\Users\CloverIris\AppData\Local\Programs\CLion\bin\ninja\win\x64\ninja.exe
  (备用 cygwin 路径: ...\bin\ninja\cygwin\x64\ninja.exe)

MinGW bin 目录 (必须加入 PATH 才能编译):
  C:\Users\CloverIris\AppData\Local\Programs\CLion\bin\mingw\bin
```

### 1.3 编译标志（CMake 已固化）

```
DEFINES  = -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00
CXXFLAGS = -Wall -Wextra -O2 -g -std=c++20 -fdiagnostics-color=always
INCLUDES = -IC:/Users/CloverIris/CLionProjects/VividPic/src
LDFLAGS  = -static-libgcc -static-libstdc++ -mwindows -mconsole -municode
LIBS     = -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic
           -luser32 -lgdi32 -lgdiplus -lshell32 -lole32 -luuid
           -lcomctl32 -ldwmapi -lshlwapi -lwinmm
           -ld2d1 -ldwrite -lwindowscodecs
           -lkernel32 -luser32 -lgdi32 -lwinspool -lshell32 -lole32 -loleaut32
           -luuid -lcomdlg32 -ladvapi32
```

**关键约束**：
- `-static-libgcc -static-libstdc++`：必须静态链接 C/C++ 运行时，否则目标机器缺少 MinGW DLL
- `-Wl,-Bstatic -lwinpthread -Wl,-Bdynamic`：winpthread 必须静态链接，否则运行时报 DLL 缺失
- `-mwindows -municode`：Win32 GUI 子系统 + Unicode 入口点

---

## 2. 项目目录结构

```
VividPic/
├── AGENTS.md               # 本文件（开发约束与进度）
├── UI.md                   # UI 设计规范（HomeScreen/Workspace/面板布局与常量）
├── CMakeLists.txt          # 主构建配置
├── cmake-build-debug/      # CLion 默认构建目录
│   ├── build.ninja         # Ninja 构建图
│   ├── CMakeFiles/         # 对象文件目录树
│   └── VividPic.exe        # 输出可执行文件
├── assets/                 # 运行时资源（由 CMake POST_BUILD 复制）
└── src/
    ├── App/                # Application 单例（消息泵、生命周期）
    ├── Core/               # Project, Layer, LayerManager, BlendMode, MemoryAdvisor, ProjectIO
    ├── Render/             # RenderBackend, D2DCanvas, TilePool, BrushEngine, BrushPreset
    ├── Tablet/             # TabletInput (WindowsInkDriver + WinTabDriver + TabletManager)
    ├── UI/
    │   ├── Core/           # Window, Theme, 消息分发基类
    │   ├── Widgets/        # Button, Panel, EditBox, ComboBox, CanvasView
    │   ├── Panels/         # BrushPanel, ColorsPanel, LayersPanel, NavigatorPanel
    │   └── Screens/        # HomeScreen, NewCanvasDialog, Workspace
    └── Utils/              # Types (String, Point, Rect, Size, Color 等)
```

---

## 3. 编译与开发流程

### 3.1 CLion 内编译（推荐）

直接点击 CLion 的 **Build** (Ctrl+F9) 或 **Run** (Shift+F10)。CLion 会自动调用 bundled CMake + Ninja。

### 3.2 命令行手动编译（PowerShell）

当只需要快速编译修改的单个文件时：

```powershell
$mingwBin = "C:\Users\CloverIris\AppData\Local\Programs\CLion\bin\mingw\bin"
$gpp      = "$mingwBin\g++.exe"
$wd       = "C:\Users\CloverIris\CLionProjects\VividPic"
$env:PATH = "$mingwBin;$env:PATH"

# 编译单个文件（示例：TabletInput.cpp）
& $gpp -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00 `
  -IC:/Users/CloverIris/CLionProjects/VividPic/src `
  -Wall -Wextra -O2 -g -std=c++20 `
  -o cmake-build-debug/CMakeFiles/VividPic.dir/src/Tablet/TabletInput.cpp.obj `
  -c src/Tablet/TabletInput.cpp
```

**注意**：`$env:PATH` **必须**包含 `mingw\bin`，否则 g++ 找不到 `cc1plus.exe` 和 `as.exe`，会静默失败（exit 1，无输出）。

### 3.3 命令行链接

```powershell
$objs = @(
  "CMakeFiles/VividPic.dir/src/App/Application.cpp.obj",
  "CMakeFiles/VividPic.dir/src/Core/BlendMode.cpp.obj",
  "CMakeFiles/VividPic.dir/src/Core/Layer.cpp.obj",
  "CMakeFiles/VividPic.dir/src/Core/LayerManager.cpp.obj",
  "CMakeFiles/VividPic.dir/src/Core/MemoryAdvisor.cpp.obj",
  "CMakeFiles/VividPic.dir/src/Core/Project.cpp.obj",
  "CMakeFiles/VividPic.dir/src/Core/ProjectIO.cpp.obj",
  "CMakeFiles/VividPic.dir/src/Render/BrushEngine.cpp.obj",
  "CMakeFiles/VividPic.dir/src/Render/BrushPresetManager.cpp.obj",
  "CMakeFiles/VividPic.dir/src/Render/D2DCanvas.cpp.obj",
  "CMakeFiles/VividPic.dir/src/Render/RenderBackend.cpp.obj",
  "CMakeFiles/VividPic.dir/src/Render/TilePool.cpp.obj",
  "CMakeFiles/VividPic.dir/src/Tablet/TabletInput.cpp.obj",
  "CMakeFiles/VividPic.dir/src/UI/Core/Theme.cpp.obj",
  "CMakeFiles/VividPic.dir/src/UI/Core/Window.cpp.obj",
  "CMakeFiles/VividPic.dir/src/UI/Panels/BrushPanel.cpp.obj",
  "CMakeFiles/VividPic.dir/src/UI/Panels/ColorsPanel.cpp.obj",
  "CMakeFiles/VividPic.dir/src/UI/Panels/LayersPanel.cpp.obj",
  "CMakeFiles/VividPic.dir/src/UI/Panels/NavigatorPanel.cpp.obj",
  "CMakeFiles/VividPic.dir/src/UI/Screens/HomeScreen.cpp.obj",
  "CMakeFiles/VividPic.dir/src/UI/Screens/NewCanvasDialog.cpp.obj",
  "CMakeFiles/VividPic.dir/src/UI/Screens/Workspace.cpp.obj",
  "CMakeFiles/VividPic.dir/src/UI/Widgets/Button.cpp.obj",
  "CMakeFiles/VividPic.dir/src/UI/Widgets/CanvasView.cpp.obj",
  "CMakeFiles/VividPic.dir/src/UI/Widgets/ComboBox.cpp.obj",
  "CMakeFiles/VividPic.dir/src/UI/Widgets/EditBox.cpp.obj",
  "CMakeFiles/VividPic.dir/src/UI/Widgets/Panel.cpp.obj",
  "CMakeFiles/VividPic.dir/src/main.cpp.obj"
) -join " "

& $gpp -Wall -Wextra -O2 -g -static-libgcc -static-libstdc++ `
  -mwindows -municode -o VividPic.exe $objs `
  -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic `
  -luser32 -lgdi32 -lgdiplus -lshell32 -lole32 -luuid `
  -lcomctl32 -ldwmapi -lshlwapi -lwinmm -ld2d1 -ldwrite -lwindowscodecs `
  -lkernel32 -luser32 -lgdi32 -lwinspool -lshell32 -lole32 -loleaut32 `
  -luuid -lcomdlg32 -ladvapi32
```

### 3.4 调试流程

1. **CLion 调试**：设置断点 → Shift+F9 (Debug)。GDB 会自动附加。
2. **命令行 GDB**（如果需要）：`C:\...\mingw\bin\gdb.exe VividPic.exe`
3. **日志调试**：在关键路径插入 `OutputDebugStringA()` 或临时 `MessageBoxW()`。
4. **DPI 调试**：Window 基类已集成 `GetDpiForWindow()`，注意 `Theme::Scale = 1.25f` 在 125% DPI 下的表现。

---

## 4. 技术栈与架构决策

### 4.1 允许使用的技术

- **Win32 API**：窗口创建、消息循环、Raw Input
- **Windows Ink (`WM_POINTER*`)**：现代数位板输入（Win10+ 首选）
- **WinTab (`Wintab32.dll`)**： legacy Wacom 驱动兼容层（动态加载，fallback）
- **Direct2D / WIC / DirectWrite**：画布最终 blit、图像编解码
- **GDI / GDI+**：UI chrome（面板、按钮、对话框）自绘
- **STL + 标准库**：`std::filesystem`, `std::vector`, 智能指针

### 4.2 禁止使用的技术

- **Qt / wxWidgets / MFC / Electron**：UI 必须自研轻量级框架
- **C++/CLI 或 .NET**：严禁引入 CLR
- **MSVC 特有扩展**：`#pragma comment`, `__declspec(uuid)`, COM 智能指针需确保 MinGW 兼容
- **Vulkan / OpenGL**：当前无需，D2D 已满足需求
- **第三方 JSON / ZIP 库**：nlohmann/json、miniz、libzip 等禁止引入；如需要序列化使用自定义二进制格式

### 4.3 渲染架构（已落地）

```
CanvasView 渲染管线（CPU 软合成 + D2D 硬显示）：
  LayerManager::CompositeToBuffer()
    ├── 遍历 Layer 栈（bottom → top）
    ├── 每图层遍历 dirty tiles
    └── CPU 逐像素 alpha 混合 → m_compositeBuffer (RGBA)
  CanvasView::UpdateCompositeBitmap()
    └── m_compositeBuffer → WIC Bitmap → D2D Bitmap
  CanvasView::CompositeAndRender()
    └── D2D RenderTarget::DrawBitmap() + zoom/pan transform
        + checkerboard + canvas border + brush cursor ring

Brush 绘制（纯 CPU rasterize）：
  CanvasView::ApplyBrush() → BrushEngine::GenerateStamps()
    → Layer::DrawBrushStamp() → 逐像素 tip shape + wetMix + flow
```

**关键认知**：当前已经是"软合成、软渲染"。D2D 仅用于最终 RGBA buffer 的屏幕 blit。TileGrid（256x256 分块）本质是分块 RGBA buffer，COW 机制避免不必要拷贝。

### 4.4 数位板输入层规范

所有数位板代码封装在 `TabletInput` 命名空间下：

```cpp
namespace TabletInput {
    struct TabletState {
        float x, y;         // 客户区坐标（已 ScreenToClient）
        float pressure;     // 0.0 ~ 1.0
        float tiltX, tiltY; // -90 ~ +90
        float rotation;     // degrees
        bool isEraser;
        bool isTouching;
    };

    class WindowsInkDriver;  // WM_POINTERDOWN/UPDATE/UP
    class WinTabDriver;      // WT_PACKET 消息，动态加载 Wintab32.dll
    class TabletManager;     // 统一入口：Windows Ink first → WinTab fallback → Mouse
}
```

**运行时策略（重要）**：
- **Windows Ink 优先**（Win10+ 原生支持，可靠性高）
- **WinTab fallback**（ legacy Wacom 驱动，无 Windows Ink 时启用）
- **Mouse 兜底**（无压感）
- **坐标转换**：两个 driver 内部必须使用 `ScreenToClient(m_hwnd, &pt)`，因为 `ptPixelLocation` / WinTab 输出都是**屏幕坐标**，而 CanvasView 期望**客户区坐标**

---

## 5. 内存管理规范

- **TileGrid**：256x256 RGBA 分块存储，空 tile 延迟分配
- **Copy-on-Write**：`Layer::DetachForWrite()` 在 `m_tileRefCount > 1` 时克隆 tile
- **MemoryAdvisor**：新建画布/图层前检查可用内存，超限弹窗提示
- **大内存**：画布像素数据可用 `alignas(32)` 或 `VirtualAlloc` 分配（为 SIMD 预留）

---

## 6. 代码风格

- **命名**：`PascalCase` 类名/结构体，`camelCase` 函数与变量，`SCREAMING_SNAKE_CASE` 宏与常量
- **编码**：UTF-8（**不带 BOM**，除非编译报错再切换）
- **头文件**：`#pragma once`
- **消息处理**：`Window::HandleMessage()` 已设为 `virtual`，子类覆盖时必须调用 `Window::HandleMessage()` fallback

---

## 7. 项目进度（Milestone 追踪）

### M1 ✅ 基础框架（已落地）
- HomeScreen（分组按钮布局）
- `Window` 基类（HWND 封装、消息路由、DPI 感知）
- `Theme` 暗色主题 + DPI 缩放（`Scale = 1.25f`）
- `Button` / `Panel` 基础控件
- `MemoryAdvisor` 系统内存检测
- `Project` 模型（画布尺寸、DPI、图层数）
- `TabletInput` stub（双驱动架构）

### M2 ✅ 画布与视口（已落地）
- Direct2D `RenderBackend` / `D2DCanvas` / `TilePool` 框架
- `NewCanvasDialog`（内存用量仪表盘）
- `Workspace`（中心视口 + 左右面板布局）
- `CanvasView`（zoom/pan、D2D render target）
- `BrushEngine` 基础（size/opacity/color/spacing）
- `EditBox` / `ComboBox` 输入控件

### M3 ✅ 图层系统与持久绘制（已落地）
- `Layer` / `LayerManager` TileGrid（256x256，COW）
- 18 种 blend mode（Normal, Multiply, Screen, Overlay, Difference, Add, Subtract, Darken, Lighten, ColorDodge, ColorBurn, HardLight, SoftLight, Exclusion, Hue, Saturation, Color, Luminosity）
- 4 面板（Layers / Navigator / Colors / Brush）
- 快捷键系统（Workspace::OnKeyDown）
- 持久绘制（非预览模式，直接写入 tile）

### M4 ✅ 完整笔刷引擎与数位板（已落地）
- 5 种笔头（RoundHard, RoundSoft, Flat, Bristle, Texture）
- flow + wetMix 参数
- `BrushPreset` 系统（5 内置预设）
- `BrushPanel`（tip 按钮 + 4 滑条 + preset 按钮）
- WinTab `WT_PACKET` 完整解析（pressure + tilt + rotation）
- `LayersPanel` blend mode 下拉框 UI
- `UI.md` 设计文档（HomeScreen/Workspace/4 面板规范）
- HomeScreen DPI 缩放修复（所有硬编码偏移量统一 `Theme::GetSize()`）

### M5 ✅ 选择工具、Undo/Redo 与面板完善（已落地）
- `SelectionMask`（矩形/椭圆/套索/魔棒选区 + 反选/清除/边界检测）
- `CanvasView` 选区交互（拖拽创建、移动工具、选区约束绘制）
- `Layer` 多态重构：`RasterLayer`（像素/COW/笔刷）+ `TextLayer`（可编辑属性 + DirectWrite 栅格化缓存）
- `FontManager`：DirectWrite 系统字体枚举
- `TextInputDialog`：文字图层创建对话框（文本/字体/字号/颜色）
- `HistoryManager` + `StrokeUndoItem` 双快照 Undo/Redo（50 步上限）
- `SelectionMask`（矩形/椭圆/套索/魔棒选区 + 反选/清除/边界检测）
- `CanvasView` 选区交互 + 移动工具 + 渐变工具 + 形状工具（填充矩形）+ 变换工具（缩放）
- `LayersPanel` 底部工具栏（新建/复制/合并/删除）+ 不透明度滑条 + protectAlpha 复选框 + Solo 模式
- `NavigatorPanel` 实时缩略图（DIBSection 缓存）+ 视图框 + 点击平移
- `ColorsPanel` 垂直 Hue 条 + HSV/Hex/RGB 数值显示
- `ToolBar` 双向同步（点击切工具 ↔ 键盘快捷键同步高亮）
- `RecentFilesManager`：MRU 最近文件持久化（`%AppData%/VividPic/recent.txt`）
- `ProjectIO` VVP v2 TextLayer 完整往返修复（直接插入反序列化 layer）
- `Workspace` 顶部工具栏（撤销/重做/笔刷切换按钮）
- 菜单栏交互（Undo/Redo 已连接，其余为占位符）

### Bugfix 记录

| Commit | 问题 | 修复方案 |
|--------|------|----------|
| `87d80cd` | M3 里程碑 | — |
| `9abbad4` | M4 里程碑 | — |
| `7e7e1df` | 新建画布对话框嵌套/无法点击 | `WS_OVERLAPPED` → `WS_POPUP`，延迟 `Create` 到 `ShowModal`，添加 `CenterOnParent` |
| `3529299` | DPI 缩放、数位笔只能画一笔 | `SetProcessDpiAwarenessContext`，`GetDpiForWindow`，`Theme::Scale=1.25f`，WinTab → Windows Ink 优先级切换，`WM_POINTERUPDATE` 缺失 |
| `ad890e6` | 笔光标偏移、鼠标/笔连线、系统光标干扰 | WindowsInkDriver/WinTabDriver `ScreenToClient`，`WM_POINTERUPDATE` 更新 cursor，`WM_SETCURSOR` 隐藏系统光标 |
| `7f8bf09` | M5 Undo/Redo + 调试日志 | `HistoryManager`、`StrokeUndoItem` 双快照实现 |
| `fefa3cc` | LayersPanel 复制/合并功能 | `DuplicateLayer`、`MergeDown` |
| `2252884` | 选择工具与 UI 增强 | `SelectionMask`、`ToolType` 枚举、CanvasView 选区交互 |
| `aef0a53` | 应用退出、HomeScreen 居中、ColorsPanel DIB | `AllocConsole`、`HomeScreen` 屏幕相对居中 |
| `32bb5a0` | 笔悬停误设 isTouching=true | `WM_POINTERUPDATE` 中根据 `pointerInfo.pointerFlags` 正确判断 |
| `51681db` | 鼠标输入被错误处理为过期笔状态 | `TabletManager` 中隔离 mouse 与 pen 状态机 |
| `136fb57` | 控制台崩溃 | `AllocConsole` + `freopen` 顺序修复，降低日志频率 |
| `f69273e` | README 多语言与 logo | 中/英/日 README + logo.svg |
| `图层多态` | Layer 抽象基类 + RasterLayer/TextLayer | 提取 Layer 抽象接口，VVP v2 payload chunk 支持 TextLayer 往返 |
| `文字系统` | FontManager + TextInputDialog + TextTool | DirectWrite 系统字体枚举，文字图层创建与栅格化 |
| `变换形状` | TransformTool + ShapeTool | 拖拽缩放图层（bilinear resampling），拖拽绘制填充矩形 |
| `HomeScreen MRU` | RecentFilesManager | `%AppData%/VividPic/recent.txt` 持久化，HomeScreen 动态最近文件按钮 |
| `面板完善` | ColorsPanel + NavigatorPanel + LayersPanel | 垂直 Hue 条 + HSV/Hex、DIBSection 缩略图缓存、protectAlpha 复选框 + Solo 模式 |
| `ToolBar 修复` | ToolBar ↔ CanvasView 双向同步 | 补全 `SetOnToolChanged` 连接，修正 Transform/Text/Shape 的 `implemented=false` 标记 |

### Active Issues / 待办

- [x] **Save/Export**：`.vvp` 自定义二进制格式序列化 + WIC PNG 导出（M6 已完成）
- [x] **打开文件**：从 HomeScreen / Workspace 打开 `.vvp` 并重建 CanvasView
- [ ] **采样率优化**：Windows Ink `WM_POINTERUPDATE` 受屏幕刷新率限制（60Hz），快速笔迹可能不够平滑。终极方案：`GetPointerPenInfoHistory()` 获取历史帧
- [ ] **TilePool 真池化**：当前 `Layer` 使用 `new Tile`，真对象池分配 deferred
- [ ] **D2D 批量 composite**：当前 `CompositeToBuffer` 是纯 CPU，可考虑 D2D 图层合成加速
- [ ] **纹理导入**：`Texture` brush tip 目前使用 64x64 过程噪声，需支持外部图片
- [ ] **抗锯齿**：笔刷 stamp 边缘在高 zoom 下可见像素颗粒
- [x] **滤镜系统**：6 个破坏性滤镜（亮度/对比度、色相/饱和度、高斯模糊、锐化、反相、阈值）+ FilterDialog 参数对话框 + Undo 集成（M6 已完成）
- [ ] **变换工具增强**：自由变换、扭曲、透视（当前仅实现缩放）
- [ ] **形状工具增强**：椭圆、直线、多边形、矢量形状（当前仅实现填充矩形）
- [ ] **设置面板**：首选项对话框、快捷键自定义
- [ ] **图层分组/剪贴蒙版**：LayersPanel 缺少新建组、蒙版按钮（Solo 模式 ✅ 已完成）
- [x] **VVP v2 TextLayer 完整往返**：`LoadProject` 改用 `AddLayer(Ref<Layer>)` 直接插入反序列化对象，避免工厂重建导致 payload 丢失
