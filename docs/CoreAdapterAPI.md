# CloverPic Core / Adapter API 契约

本文档定义 CloverPic 平台无关 core 与平台 adapter 之间的边界。原则很简单：**core 拥有产品，adapter 暴露平台能力**。

Core 负责项目数据、绘画状态、UI scene、命中测试、命令分发、刷新调度、软渲染、文字栅格、缩略图缓存和大块绘画内存。Platform adapter 只负责无法用可移植 C++ 表达的操作系统能力：窗口、事件源、文件对话框、原始文件字节、图片编解码、系统字体发现、色彩 profile 发现、设备信息和最终 frame 呈现。

## 架构硬规则

`CloverPicCore` 必须能在没有平台头文件的情况下编译。Core 不能 include 或暴露：

- `windows.h`
- `HWND` / `HDC`
- WIC / DirectWrite / D2D 类型
- Cocoa / Objective-C 对象
- X11 / Wayland native handles
- Android / Java handles
- 浏览器 DOM / Canvas 对象
- 文件描述符或平台图形 API 句柄

Adapter 可以自由 include 平台头文件，但进入 core 前必须把平台概念转换为稳定的 core API。Adapter 不得把 native handle、平台文本对象、图片 codec 对象、文件句柄或窗口控件对象传给 core。

## 运行生命周期

1. 平台 executable 启动。
2. Adapter 初始化平台库，例如 COM、DPI awareness、WIC、Win32 input、字体枚举或浏览器 canvas binding。
3. Adapter 创建 `PlatformServices`，通过 `CoreServices::InstallPlatformServices` 安装。
4. Adapter 先创建 Workspace/MainWindow，并实例化 `WorkspaceRuntime`。
5. Adapter 创建 owned borderless Program Manager popup，并实例化 `ProjectManagerRuntime`。
6. Program Manager 只产生 `WorkspaceLaunchRequest`，adapter 把请求传给现有 `WorkspaceRuntime`。
7. Core 读取平台 facts、字体路径、色彩 profile、settings bytes，并初始化 core-owned runtime 状态。
8. 窗口 resize / DPI 变化时，adapter 调用 `RuntimeSurface::Resize(width, height, dpiScale)`。
9. Adapter 把 native input 转成 `CloverPic::Input`，调用 `HandlePointer`、`HandleKey` 或 `HandleWheel`。
10. Core 更新 editor state、UI state、command state、dirty regions 和 animated regions。
11. Adapter 在消息循环或 timer 中查询 `RuntimeSurface::NeedsFrame(nowMs)`。
12. 需要绘制时，adapter 调用 `RuntimeSurface::Render(nowMs, dirtyRects)`。
13. Adapter 呈现返回的 `RgbaFrame`，可按 `dirtyRects` 裁剪呈现。
14. 关闭 MainWindow 退出进程；关闭 Program Manager 在已有项目时隐藏，没有项目时退出。

## Adapter 必须提供的能力

窗口与主循环：

- 创建平台顶层窗口、浏览器 canvas 或 app surface。
- 报告 viewport size 和 DPI scale。
- 传递 resize、close、keyboard、pointer、wheel、tablet、focus 事件。
- 在 core 有 pending dirty/animated region 时安排 repaint。

呈现：

- 接收 `Core::RgbaFrame`。
- 当前 MVP frame 格式固定为 `SurfacePixelFormat::Bgra8Unorm`。
- 能局部呈现时按 dirty rect 裁剪。
- 不解释 UI widget、按钮、图层、工具、布局或 command payload。

存储、编解码和对话框：

- `IPlatformFileSystem`: 原始字节读写。
- `IImageCodec`: PNG/image codec。包括 BGRA8 display PNG export 和 RGBA16 PNG resource，用于承载 10bit raster layer。
- `IFileDialogService`: 平台文件选择器。
- `IRecentFilesStore`: 最近文件列表。
- `IAppSettingsStore`: app settings opaque bytes。
- Adapter 只选择平台合适的数据目录，不解析 `.cloverpic`。

字体：

- `IPlatformFontCatalogProvider`: 返回系统 UI 字体族名、可选 UI 字体路径、候选 `.ttf` / `.ttc` / `.otf` 字体文件路径。
- Adapter 只做字体发现，不做文字栅格。
- Adapter 不暴露平台字体 API 对象。

设备、显示和色彩：

- `IMemoryInfoProvider`: 物理内存、可用内存等。
- `IDisplayInfoProvider`: 显示尺寸、DPI、bits per pixel、dynamic range。
- `IColorProfileProvider`: 当前显示 profile、系统/用户 ICC/ICM 列表、默认 RGB/CMYK profile、profile bytes。
- 无法可靠判断 HDR/SDR 时返回 `DisplayDynamicRange::Unknown`。

输入：

- 把平台 mouse / pen / touch / tablet API 翻译为 `Input::PointerEvent`。
- 坐标必须是 core window coordinates。
- pressure 归一化为 `0.0..1.0`。
- 能提供时保留 pen eraser、tilt、rotation、contact state。
- Windows adapter 必须过滤 pen/touch promoted mouse message，避免数位笔和鼠标状态串扰。

## Core 拥有的能力

应用与命令：

- `ProjectManagerRuntime`
- `WorkspaceRuntime`
- `EditorSession`
- modal state
- command dispatch
- workspace actions
- project open/save/export/new canvas

UI：

- `UiScene` / `UiNode` tree
- z-order
- hit-test
- hover / focus / capture
- modal blocker
- menu / dropdown / slider / switch / swatch / scroll list
- panel docking / floating / reorder / resize
- layer list scroll and layer drag reorder
- Program Manager / Settings / Workspace scene 构建

渲染：

- `SoftRenderer`
- visual theme tokens
- panels / buttons / swatches / canvas
- icon/vector drawing
- `RgbaFrame`
- dirty rects
- `FrameScheduler`
- animated regions
- Navigator composite thumbnail cache
- Layer thumbnail cache

文字：

- `CoreTextEngine`
- font face probing
- glyph lookup
- kerning
- glyph cache
- grayscale antialias rasterization
- UI text 和未来 TextLayer raster path

绘画模型：

- `Project`
- `LayerManager`
- `Layer` / `RasterLayer` / `TextLayer`
- `SelectionMask`
- `BrushEngine`
- `History`
- `Filters`
- tile memory lifecycle
- blend mode / opacity / visibility / lock / protect alpha / solo
- RGBA10 内部 raster 数据路径

颜色：

- foreground / secondary color state
- HSV square / hue strip
- swatch
- transparent color
- HEX display
- Web-safe color quantization
- 色彩 profile 在项目中的引用和嵌入策略

文件格式：

- `.cloverpic` schema
- manifest / index / resource chunks
- layer metadata
- profile metadata / bytes
- core-owned serializer/deserializer

## 禁止进入 Core 的东西

- native child window
- platform widget
- platform message constants
- native file dialog calls
- OS filesystem APIs
- platform color management APIs
- DPI APIs
- tablet APIs
- image encoding APIs
- platform text APIs
- adapter-owned editor state

Core 可以保存 adapter 返回的 opaque path/string/bytes，但不能根据平台路径语义做平台分支。

## 当前 Windows Adapter

当前 Windows adapter 位于 `src/Platform/Windows`。

它提供：

- Win32 Workspace `HWND`
- owned borderless Program Manager popup
- Win32 message pump
- DPI / resize / close / timer
- mouse / keyboard / wheel / touch / pointer pen 翻译
- promoted mouse message 过滤
- BGRA frame 通过 DIB / `StretchDIBits` 呈现
- dirty rect clipping
- Win32 file dialogs
- `%AppData%/CloverPic` 下的 recent 和 settings bytes
- raw file byte access
- WIC-backed PNG encode/decode
- ICC/ICM profile discovery
- system UI font discovery
- memory and display facts

它不能：

- 创建 core UI 子窗口。
- 运行旧 GDI widget 绘制路径。
- 栅格化应用文字。
- 理解按钮、菜单、面板、图层列表、缩略图、颜色选择器或 command 语义。
- 拥有项目、图层、画布、工具或 UI 状态。

## 添加新平台 Adapter 的最小清单

新平台目录示例：

```text
src/Platform/Linux/
src/Platform/macOS/
src/Platform/Wasm/
```

必须实现：

- host / main loop equivalent
- surface presenter equivalent
- `IPlatformFileSystem`
- `IImageCodec`
- `IFileDialogService`
- `IRecentFilesStore`
- `IAppSettingsStore`
- `IPlatformFontCatalogProvider`
- `IColorProfileProvider`
- `IDisplayInfoProvider`
- `IMemoryInfoProvider`
- native input -> `CloverPic::Input` translator
- small executable entry point
- `PlatformServices` 安装流程

如果平台缺失某项能力，应返回空列表、安全默认值或 `Unknown`。不要为了平台缺失能力在 core 里加平台分支。

## 当前稳定性承诺

Core/Adapter 边界在 MVP 阶段应被当作稳定 API 维护。未来扩展应优先增加可移植概念，而不是把平台 handle 泄漏进 core。

允许扩展：

- 新平台事实字段。
- 新 image codec 能力。
- 新 input capability。
- 新文件 dialog 选项。
- 新 profile discovery 元数据。

不允许扩展：

- adapter 直接操作 UI 控件。
- adapter 直接操作 layer/project/editor state。
- core 依赖平台头文件。
