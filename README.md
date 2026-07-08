# CloverPic

CloverPic 是 Clover Creation 系列中的 C++20 桌面绘画软件 MVP。它的目标是提供接近 SAI 的轻量、高响应、图层清晰的绘画工作流，同时把平台相关能力压缩到 adapter 层，让核心编辑器逻辑未来可以继续迁移到 macOS、Linux、WASM 或其他支持现代 C++ 的运行环境。

当前项目已经从早期 Win32 子窗口 UI 改造成 **core-driven single-surface** 架构：Core 拥有项目数据、图层、画笔、历史、UI scene、布局、命中测试、命令分发、刷新调度、软渲染和文字栅格；Platform adapter 只负责创建平台窗口、翻译输入、提供文件/图片/字体/色彩/设备服务，并把 core 输出的 BGRA frame 呈现到窗口。

## 当前定位

- 面向插画和轻量图像编辑的本地绘画工具。
- 先完成 Windows adapter，架构上预留其他平台。
- UI 由 core 自绘，不使用 Win32 原生控件树。
- 核心项目格式由 core 维护，平台只提供字节读写和图片编解码。
- 以 MVP 主流程为优先：新建、打开、保存、导出、画布、画笔、橡皮、颜色、图层、Undo/Redo、基础滤镜、基础选区和基础输入。

## 功能现状

- 启动后先创建 Workspace 主窗口，并显示 owned borderless Program Manager。
- Program Manager 支持新建图像、打开项目、最近项目、设置入口和 Metro tile 区域。
- Workspace 使用单 surface 软渲染：顶部菜单、命令栏、左侧工具栏、左右 dock 面板、浮动面板、状态栏和画布都由 core 绘制。
- 面板支持折叠、展开、拖出浮动、拖回 dock、重排、关闭和调整尺寸。
- Layer 面板支持图层选择、添加、删除、复制、合并、可见性、锁定、protect alpha、solo、opacity、blend mode、滚动列表、拖拽排序和缩略图。
- Color 面板支持 HSV 色域、Hue strip、色块、前景/背景色交换、透明色、HEX 显示和 Web-safe 色彩限制开关。
- Navigator 和 Layer 面板缩略图由 core 低频刷新缓存，adapter 不理解这些 UI 语义。
- 支持统一的鼠标、滚轮、键盘、触摸/数位笔输入路径。Windows adapter 会过滤 pen/touch promoted mouse message，避免鼠标和数位笔状态串扰。
- Core 内部 raster tile 使用 RGBA10 有效数据，窗口呈现时降采样为 BGRA8 frame。

## 项目结构

```text
src/
  Core/
    App/                    # runtime、command、workspace action、canvas controller
    Document/               # Project、EditorSession、ProjectIO / .cloverpic
    Editing/                # Filters、History
    Input/                  # 平台无关输入事件
    Layers/                 # Layer、RasterLayer、TextLayer、LayerManager、BlendMode
    Platform/               # Core 可见的平台服务接口
    Presentation/           # SoftRenderer、FrameScheduler、IconPainter
    Render/                 # BrushEngine、BrushPreset、TilePool
    Services/               # CoreServices / PlatformServices 聚合入口
    Text/                   # Core-owned 字体解析与文字软栅格
    UI/
      Scene/                # UiScene / UiNode
      Workspace/            # Workspace layout / components / interaction / modal / render / persistence
      ProjectManager/       # Program Manager UI state / layout / components / interaction / render
  Platform/
    Windows/                # Windows adapter：HWND、消息泵、服务实现、frame 呈现
  Resource/
    Icons/Tabler/           # MIT 图标资源子集与说明
  Utils/                    # 平台无关基础类型
```

`CloverPicCore` 是平台无关静态库。`CloverPic.exe` 是 Windows adapter 可执行文件，负责把 Win32 能力包装成 core service。

## Core / Adapter 边界

Core 负责：

- 产品行为、项目状态、图层状态、画布状态、命令、modal 和编辑器 session。
- `UiScene`、`UiNode`、布局、z-order、hover、focus、capture、hit-test、菜单、dock、面板大小和 UI 状态持久化。
- 软渲染、图标绘制、文字栅格、缩略图缓存、dirty/animated refresh 调度。
- `.cloverpic` 项目格式解析、序列化、manifest/index/resource 组织。
- 图层、画笔、滤镜、历史、选区、合成和内存中的绘画数据。

Adapter 负责：

- 创建平台窗口和平台主循环。
- 把平台输入事件翻译为 `CloverPic::Input`。
- 呈现 core 输出的 `RgbaFrame`。
- 提供文件字节、PNG 编解码、文件对话框、最近文件存储、系统字体发现、ICC/ICM profile 发现、显示/内存信息和 app settings bytes。

Adapter 不允许理解或实现按钮、菜单、layer list、panel docking、缩略图、颜色选择器或编辑器业务逻辑。完整边界见 [docs/CoreAdapterAPI.md](docs/CoreAdapterAPI.md)。

## 项目格式

当前目标格式是 `.cloverpic`。项目格式由 core 拥有，平台 adapter 只提供原始字节读写和图片编解码服务。

MVP 文件容器设计：

- magic: `CLVPIC10`
- version: `1`
- endian: little-endian
- `MANIFEST`: 项目、画布、profile 摘要、图层顺序和图层元数据。
- `INDEX`: resource offset、size、type、crc、layer index。
- `RESOURCE`: 每个图层独立资源，重复保存 layer index、opacity、blend mode、visible、lock、protect alpha、solo、pixel format 等校验元数据。
- Raster layer 使用 16bit PNG 承载 RGBA10 有效数据，并在 metadata 标记 `validBits=10`。
- Vector layer 使用 SVG resource。
- Text layer 是带 CloverPic text marker 的特殊 SVG/payload resource，后续继续扩展文字编辑器。

早期 `.vvp` 兼容不是当前 MVP 目标。

## UI 设计方向

CloverPic 的 UI 是 core-owned scene tree。平台只知道窗口大小和输入坐标；core 决定这个坐标命中的是按钮、菜单、面板、图层列表、颜色区域还是画布。

当前视觉方向是 Metro / Windows 10 + SAI 暗色工作流软件：

- 方形、低圆角、1px 高光/暗边。
- 深色工作区、蓝色 active 状态、清晰 hover/focus/capture 状态。
- 左侧 64px 工具 rail，工具项固定尺寸，图标和标签垂直排布。
- 左右 dock 面板可折叠、浮动、排序和调整大小。
- 控件尽量用图标、色块、toggle、slider、dropdown、scroll list，而不是大量文字按钮。

## 构建

推荐使用 CLion 的 Build / Run。命令行构建需要先把 CLion bundled MinGW 加入 `PATH`：

```powershell
$mingwBin = "C:\Users\YoCoc\AppData\Local\Programs\CLion\bin\mingw\bin"
$env:PATH = "$mingwBin;$env:PATH"
& "C:\Users\YoCoc\AppData\Local\Programs\CLion\bin\cmake\win\x64\bin\cmake.exe" -S . -B cmake-build-debug -G Ninja
& "C:\Users\YoCoc\AppData\Local\Programs\CLion\bin\cmake\win\x64\bin\cmake.exe" --build cmake-build-debug --target CloverPic -j 14
```

## 工具链

- OS: Windows 10/11 x64
- C++: C++20，`CMAKE_CXX_EXTENSIONS OFF`
- Compiler: CLion bundled MinGW-w64 GCC 13.1.0
- Build: CLion bundled CMake + Ninja
- Runtime: 不依赖 MSVC runtime
- Target: `CloverPic`

## 验证建议

构建后建议至少跑下面几类 smoke：

- 启动：Workspace 显示，Program Manager 无边框浮在上层，可新建/打开项目。
- 绘画：画笔、橡皮、颜色切换、透明背景、鼠标/数位笔输入状态不串扰。
- 图层：添加、删除、复制、合并、拖拽排序、opacity、blend mode、visible、lock、protect alpha、solo。
- UI：左右栏折叠/展开，面板拖出/回嵌，面板 resize，Layer list 滚动，Color 面板 HEX/Web-safe/透明色。
- 文件：保存 `.cloverpic`、重新打开、导出 PNG。
- 边界：`src/Core` 不出现 `windows.h`、`HWND`、`HDC`、WIC、D2D、DirectWrite 等平台类型。

## 当前非目标

- 多文档并行编辑。
- 云同步、投稿、教程、素材商店、社区。
- 完整 HDR / WCS advanced color pipeline。
- 完整矢量编辑器、复杂自由变换、透视/扭曲。
- HarfBuzz 级复杂脚本 shaping、彩色 emoji、LCD subpixel 文本渲染。

这些方向可以在核心边界稳定后逐轮推进。
