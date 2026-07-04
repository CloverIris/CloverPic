# CloverPic PRD

## 1. Product Positioning

CloverPic 是 Clover Creation 系列中的桌面绘画软件 MVP，目标是提供接近 SAI 的轻量、高响应、图层清晰的绘画工作流，同时保持 core/adapter 架构足够干净，未来可以继续扩展到 macOS、Linux、WASM 或其他 C++ 运行环境。

核心哲学：core 拥有产品逻辑、UI scene、布局、命中测试、项目格式、绘画数据和软渲染；adapter 只暴露操作系统无法抽象掉的能力，例如窗口、输入事件、文件字节、图片编解码、字体/色彩 profile 发现、内存/显示设备信息和最终帧呈现。

## 2. MVP User Goals

- 用户启动应用后先看到 Workspace 主窗口和一个无边框 Program Manager 浮窗。
- 用户可以从 Program Manager 新建画布、打开最近项目、打开设置页。
- 用户可以使用画笔、橡皮、颜色、图层、Undo/Redo、基础滤镜和 PNG 导出完成绘画主流程。
- 用户可以保存 `.cloverpic` 项目并在之后重新打开继续编辑。
- 用户可以通过鼠标、键盘、滚轮和数位笔路径进行统一输入。

## 3. Current MVP Scope

- Program Manager：Recent/Open/New/Settings 导航、最近项目搜索、Metro tile 区、完整 Create New Image 表单。
- Workspace：菜单栏、命令工具条、左侧 64px 工具栏、颜色面板、画笔面板、右侧 Navigator/Layer/Brush Size 面板、状态栏、中心画布。
- UI：core-owned `UiScene`，所有布局、z-order、hover/focus/capture、modal、命中测试和 command payload 都在 core 内处理。
- 文件：core-owned `.cloverpic` v1 chunked container，adapter 只提供文件字节和 PNG codec。
- 像素：内部 raster tile 使用 RGBA10 有效数据；呈现给窗口时降到 BGRA8 frame。
- 色彩：Windows adapter 枚举 ICC/ICM profile，core 在新建项目和 settings 中使用 profile path/bytes。
- 文字：adapter 只提供字体文件发现，core 自研字体解析/软光栅路径用于 UI/TextLayer。

## 4. Non-MVP Scope

- 云同步、投稿、素材商城、教程和社区功能。
- 完整 HDR tone mapping、复杂 WCS advanced color pipeline。
- 完整矢量编辑器、复杂自由变换、透视/扭曲。
- HarfBuzz 级复杂脚本 shaping、彩色 emoji 字体、LCD subpixel 文本渲染。
- 多文档并行 MainWindow。

## 5. Core Responsibilities

- `Project`、`LayerManager`、`RasterLayer`、`TextLayer`、`SelectionMask`、`BrushEngine`、`History`、`Filters`。
- `ProjectManagerRuntime`、`WorkspaceRuntime`、`EditorSession`、`CanvasController`。
- `UiScene`、`UiNode`、layout、hit-test、focus、hover、capture、modal、command dispatch。
- `SoftRenderer`、`FrameScheduler`、core text rasterization、icon/vector drawing。
- `.cloverpic` schema、manifest/index/resource chunks、10bit raster resources、profile embedding。

## 6. Adapter Responsibilities

- 创建 Workspace 主窗口和 owned borderless Program Manager 浮窗。
- 翻译 Win32 mouse/keyboard/wheel/touch/pen events 为 core input events。
- 呈现 core 输出的 `RgbaFrame`，不理解 UI 控件语义。
- 提供 `IPlatformFileSystem`、`IImageCodec`、`IFileDialogService`、`IRecentFilesStore`。
- 提供 `IPlatformFontCatalogProvider`、`IColorProfileProvider`、`IDisplayInfoProvider`、`IMemoryInfoProvider`、`IAppSettingsStore`。
- 不解析 `.cloverpic`，不拥有图层/画布/工具状态，不创建旧式 Win32 子控件 UI。

## 7. Project Format

`.cloverpic` v1 使用 magic `CLVPIC10`，little-endian chunked container：

- `MANIFEST`：项目名、画布尺寸、dpi、透明背景、profile 摘要/bytes、图层顺序和图层元数据。
- `INDEX`：资源 id、layer index、resource offset、size、kind、crc。
- `RESOURCE`：每个图层单独资源，重复保存 layer index、opacity、blend mode、visible/lock/protectAlpha 等元数据作为校验。

Raster layer 使用 16bit PNG resource 承载 RGBA10 有效数据，metadata 标记 `validBits=10`。Vector layer 使用 SVG resource。Text layer 是带 CloverPic text marker 的特殊 SVG/payload resource，后续可继续扩展。

## 8. Acceptance Criteria

- `CloverPicCore` 不包含 `windows.h/HWND/HDC/WIC/DirectWrite/D2D` 类型。
- Windows-only 代码只在 `src/Platform/Windows`。
- 构建目标 `CloverPic` 使用 CLion bundled CMake + MinGW Ninja 成功链接。
- 启动后 Workspace 先显示，Program Manager 无边框浮在上层，空白区域可拖动。
- New Image 表单创建项目后 Program Manager 隐藏，Workspace 接收项目并进入编辑状态。
- 保存/打开 `.cloverpic` 后图层顺序、透明度、混合模式、可见/锁定/protect alpha、profile bytes 和画面内容保持一致。
- 左侧工具栏图标尺寸保持 64px rail / 54px item / 约 30px icon，标签清晰可读。
