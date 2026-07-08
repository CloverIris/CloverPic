# CloverPic PRD

## 1. 产品定位

CloverPic 是 Clover Creation 系列中的桌面绘画软件 MVP。它面向插画、草稿和轻量图像编辑场景，目标是提供接近 SAI 的高响应绘画体验，同时保持 core/adapter 架构足够干净，未来可以扩展到 Windows 之外的平台。

核心哲学：

- Core 拥有产品逻辑、UI scene、布局、命中测试、项目格式、绘画数据、软渲染和文字栅格。
- Adapter 只暴露操作系统无法抽象掉的能力，例如窗口、输入事件、文件字节、图片编解码、字体发现、色彩 profile 发现、内存/显示设备信息和最终 frame 呈现。
- UI 不是平台控件树，而是 core 自绘的单 surface。

## 2. MVP 用户目标

- 用户启动应用后看到 Workspace 主窗口和 Program Manager 浮窗。
- 用户可以从 Program Manager 新建画布、打开项目、查看最近项目和进入设置。
- 用户可以在 Workspace 中使用画笔、橡皮、颜色、图层、Undo/Redo、基础滤镜和 PNG 导出完成绘画主流程。
- 用户可以保存 `.cloverpic` 项目，并在之后重新打开继续编辑。
- 用户可以通过鼠标、键盘、滚轮和数位笔进入统一输入路径。
- 用户可以折叠、浮动、排序和调整左右工具面板。

## 3. 当前 MVP 范围

Program Manager：

- Recent / Open / New / Settings 导航。
- 最近项目搜索。
- Metro tile 区域。
- Create New Image 表单。
- Borderless owned popup，视觉上作为项目管理器而不是传统独立窗口。

Workspace：

- 顶部菜单和命令工具条。
- 左侧 64px 工具 rail。
- 左侧 Color / Brush Control / Brush Preview / Brush Presets 面板。
- 右侧 Navigator / Layer / Brush Size 面板。
- 中心画布、状态栏、modal、dropdown、context-like menu。
- 面板折叠、展开、浮动、回嵌、重排、关闭、调整大小和持久化。

UI：

- Core-owned `UiScene`。
- Core-owned hit-test、z-order、hover、focus、capture、modal blocker。
- Core-owned command payload。
- Core-owned scroll list、layer drag reorder、panel resize、thumbnail cache。

文件：

- Core-owned `.cloverpic` v1 项目格式。
- Adapter 只提供文件字节读写和 PNG codec。

像素：

- 内部 raster tile 走 RGBA10 有效数据。
- 呈现给窗口时输出 BGRA8 frame。

色彩：

- Windows adapter 枚举 ICC/ICM profile。
- Core 在新建项目、设置和项目格式中使用 profile path/bytes。
- Color 面板支持前景/背景色、透明色、HEX 和 Web-safe 限制。

文字：

- Adapter 只提供字体文件发现。
- Core 自研字体解析和软光栅路径，服务 UI 和未来 TextLayer。

## 4. 非 MVP 范围

- 云同步、投稿、素材商店、教程和社区。
- 多文档并行 MainWindow。
- 完整 HDR tone mapping 和复杂 WCS advanced color pipeline。
- 完整矢量编辑器、自由变换、透视/扭曲。
- HarfBuzz 级复杂脚本 shaping、彩色 emoji、LCD subpixel 文本渲染。
- 完整动画/Time-lapse 工作流。

这些功能可以作为后续阶段扩展，但不能破坏 core/adapter 边界。

## 5. Core 职责

- `Project`、`EditorSession`、`.cloverpic` serializer。
- `LayerManager`、`RasterLayer`、`TextLayer`、`SelectionMask`。
- `BrushEngine`、`History`、`Filters`、blend/composite。
- `ProjectManagerRuntime`、`WorkspaceRuntime`、`CanvasController`、command dispatch。
- `UiScene`、`UiNode`、layout、hit-test、focus、hover、capture、modal。
- `SoftRenderer`、`FrameScheduler`、IconPainter、CoreTextEngine。
- Workspace UI settings schema。
- Navigator 和 Layer 缩略图缓存。

## 6. Adapter 职责

- 创建 Workspace 主窗口和 owned Program Manager popup。
- 翻译 Win32 mouse / keyboard / wheel / touch / pen events 为 core input events。
- 呈现 core 输出的 `RgbaFrame`，不理解 UI 控件语义。
- 提供 `IPlatformFileSystem`、`IImageCodec`、`IFileDialogService`、`IRecentFilesStore`。
- 提供 `IPlatformFontCatalogProvider`、`IColorProfileProvider`、`IDisplayInfoProvider`、`IMemoryInfoProvider`、`IAppSettingsStore`。
- 不解析 `.cloverpic`。
- 不拥有图层、画布、工具状态或 UI 状态。
- 不创建旧式 Win32 子控件 UI。

## 7. 项目格式

`.cloverpic` v1 使用 magic `CLVPIC10`，little-endian chunked container。

- `MANIFEST`: 项目名、画布尺寸、dpi、透明背景、profile 摘要/bytes、图层顺序和图层元数据。
- `INDEX`: resource id、layer index、resource offset、size、kind、crc。
- `RESOURCE`: 每个图层单独资源，重复保存 layer index、opacity、blend mode、visible、lock、protect alpha、solo 等元数据作为校验。

Raster layer 使用 16bit PNG resource 承载 RGBA10 有效数据，metadata 标记 `validBits=10`。Vector layer 使用 SVG resource。Text layer 是带 CloverPic text marker 的特殊 SVG/payload resource。

## 8. 验收标准

- `CloverPicCore` 不包含 `windows.h/HWND/HDC/WIC/DirectWrite/D2D` 类型。
- Windows-only 代码只在 `src/Platform/Windows`。
- `CloverPic` 使用 CLion bundled CMake + MinGW Ninja 能成功链接。
- 启动后 Workspace 先显示，Program Manager 无边框浮在上层。
- New Image 创建项目后 Program Manager 隐藏，Workspace 进入编辑状态。
- 保存/打开 `.cloverpic` 后图层顺序、透明度、混合模式、可见性、锁定、protect alpha、profile bytes 和画面内容保持一致。
- 鼠标和数位笔状态不串扰。
- Layer 面板支持滚动、拖拽排序、缩略图和完整图层操作。
- 左右面板支持折叠、浮动、回嵌、排序、关闭和调整尺寸。
