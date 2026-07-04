# CloverPic PRD

## 1. 产品定位

CloverPic 是 Clover Creation 系列中的桌面绘画软件，面向插画、草图和轻量图片编辑。产品体验参考轻量级专业绘图工具：启动快、画布响应直接、图层和画笔工作流清晰，不依赖大型第三方 UI 框架。

架构目标是把绘画模型、UI 逻辑和平台能力彻底分离。Core 负责所有平台无关的产品逻辑；adapter 只负责操作系统无法抽象掉的部分。未来添加 macOS、Linux、WASM 或其他平台时，应当只新增 adapter，而不是修改 core。

## 2. 用户目标

- 用户可以快速新建画布并开始绘制。
- 用户可以使用画笔、橡皮、颜色和图层完成基础插画流程。
- 用户可以保存 `.vvp` 项目并在之后继续编辑。
- 用户可以导出 PNG 等通用图片格式。
- 用户可以通过撤销/重做安全探索绘制过程。
- 用户可以使用鼠标、键盘、滚轮和数位笔完成自然输入。

## 3. MVP 范围

MVP 保留绘画主流程，不追求云端、社区、素材库、教程等外围功能。

当前 MVP 包含：

- Program Manager：首页项目管理器，包含新建、打开、最近项目、搜索和未来内容 tile 占位。
- Workspace：顶栏、工具栏、颜色区、画笔区、导航区、图层区、状态栏和画布区。
- 画布：缩放、平移、软合成、dirty-rect 局部刷新。
- 工具：画笔、橡皮、基础移动/选择/颜色切换扩展点。
- 图层：Raster layer、Text layer 数据模型，选择、增加、删除、显示/隐藏。
- 历史记录：Undo / Redo。
- 文件：`.vvp` 保存与打开，PNG 导出。
- 输入：鼠标、键盘、滚轮、数位笔压力路径统一进入 core input event。
- 渲染：core 生成固定 `Bgra8Unorm` frame，adapter 只负责显示。
- 文字：adapter 发现系统字体文件，core 自研 TrueType 解析、glyph lookup、kerning、灰度抗锯齿软光栅。

## 4. 非 MVP 范围

以下能力不作为当前闭环的完成条件，但应保留扩展空间：

- 云同步、投稿、图库、教程、社区。
- 完整素材管理和在线 brush marketplace。
- 完整自由变换、透视变形、高级矢量形状。
- 完整色彩管理管线和 HDR tone mapping。
- 多平台 adapter 的实际实现。
- 高级性能优化，如 SIMD tile 合成、GPU 图层合成、真 tile pool 池化。
- 复杂脚本文字 shaping、OpenType feature shaping、emoji color fonts、LCD subpixel rendering。

## 5. 架构原则

### 5.1 Core owns product behavior

Core 拥有：

- Project、Layer、LayerManager、RasterLayer、TextLayer、SelectionMask。
- BrushEngine、tile 数据、滤镜、历史记录。
- AppRuntime、EditorSession、CanvasController。
- UiScene、UiNode、布局、z-order、focus、hover、capture、modal。
- CommandDispatcher、ModalManager、FrameScheduler。
- `.vvp` 项目序列化/反序列化。
- RGBA/BGRA frame 生成和 dirty rect 决策。
- CoreTextEngine：字体 face 探测、TrueType glyph 解析、glyph cache、fallback、文本软光栅。

### 5.2 Adapter owns platform capabilities

Adapter 拥有：

- 平台窗口和主循环。
- DPI、viewport、resize、close、focus 等窗口状态。
- 鼠标、键盘、滚轮、touch、pen/tablet 原生事件翻译。
- 文件字节读写、文件对话框、最近文件存储。
- PNG/JPEG 等平台图片编码能力。
- 系统 UI 字体族名、字体目录、字体文件路径枚举。
- 显示器信息、动态范围、色彩配置路径、系统内存信息。
- 把 core 输出的 `RgbaFrame` 呈现到平台 surface。

Adapter 不应理解 UI 语义，不应拥有图层或画布状态，不应解析 `.vvp` 项目格式，也不应负责应用文字栅格。

## 6. Core / Adapter API

平台能力通过 `Core::PlatformServices` 聚合安装，必须在 `AppRuntime::Initialize()` 前完成。

核心接口组：

- `IPlatformHost`：viewport、DPI、request frame、request quit。
- `ISurfacePresenter`：接收 `RgbaFrame` 与 dirty rects。
- `IPlatformFileSystem`：跨平台文件字节读写。
- `IImageEncoder`：PNG 等图片编码能力。
- `IFileDialogService`：打开、保存、导出路径选择。
- `IRecentFilesStore`：最近文件持久化。
- `IPlatformFontCatalogProvider`：系统 UI 字体族名、字体路径 hint、安装字体文件路径。
- `IDisplayInfoProvider`：显示器尺寸、DPI、色彩配置、HDR/SDR 信息。
- `IMemoryInfoProvider`：系统可用内存等建议性信息。

更详细契约见 `docs/CoreAdapterAPI.md`。

## 7. UI 系统需求

UI 应由 core scene 驱动，而不是平台子窗口或平台控件驱动。

`UiScene` 需要支持：

- 稳定节点树：parent、children、bounds、z-order。
- 节点类型：button、panel、toolbar item、layer item、swatch、canvas region、modal blocker、search box、tile。
- 状态：hover、focus、capture、modal stack。
- 命中测试：所有坐标为 core 窗口坐标。
- 可访问性标签：`accessibilityLabel`。
- 刷新分类：静态、交互、高频动画区域。
- command payload：节点触发 command，runtime 不依赖平台消息。

平台 adapter 只提供窗口尺寸和输入事件；按钮点击、拖拽、菜单、modal 阻塞、画布交互都由 core 计算。

## 8. 刷新与渲染需求

- Core 通过 soft renderer 绘制 UI、画布预览、图层合成和基础图形。
- Core 输出固定像素格式 `Bgra8Unorm`。
- `FrameScheduler` 管理 dirty rects 和 animated regions。
- 静态 UI 不应持续重绘。
- 笔刷绘制、拖拽、光标预览、选区动画可以注册为高频刷新区域。
- Adapter 呈现 dirty rects 时不得理解 UI 结构，只按矩形区域搬运像素。

## 9. 文件与项目格式需求

- `.vvp` 是 CloverPic 的 core-owned 项目格式。
- Core 负责项目 schema、版本号、图层 payload、tile 数据、文本图层数据。
- Adapter 只负责 `ReadFileBytes` / `WriteFileBytes`。
- 导出 PNG 时，core 合成像素，adapter 负责编码。
- 未来其他平台必须复用同一个 `.vvp` serializer，避免项目文件在不同平台行为不一致。

## 10. Windows Adapter 需求

Windows 是第一个 adapter，当前职责是：

- 创建单个 Win32 `HWND`。
- 初始化 COM、DPI awareness、WIC 等 Windows 服务。
- 翻译 Win32 mouse / keyboard / wheel / pointer pen / touch 事件为 core input。
- 处理 `WM_SIZE`、`WM_DPICHANGED`、close、paint。
- 在 `BeginPaint` 的 HDC 内呈现 core 帧，支持 dirty rect clipping。
- 通过 Win32 文件对话框提供打开/保存/导出路径。
- 通过 WIC 提供 PNG 编码。
- 通过 Windows 字体目录和注册表提供字体发现。
- 提供系统内存、主显示器、动态范围、色彩配置等设备信息。

Windows adapter 不应创建旧式子窗口 UI，不应运行 GDI 控件体系，不应拥有 Workspace、CanvasView 或 layer state。

## 11. 验收标准

构建验收：

- 使用 CLion bundled CMake + MinGW configure/build 成功。
- `CloverPicCore` 只编译 `src/Core` 和平台无关代码。
- Windows 系统库只链接到最终 `CloverPic.exe`。
- `src/Core` 不包含 `windows.h`、`HWND`、`HDC`、平台渲染对象、平台文字对象、平台图片编解码对象或 Win32 dialog 类型。
- 仓库源码不包含第三方字体栅格库依赖或相关构建入口。

功能验收：

- 应用启动并显示 Program Manager。
- 新建 preset 画布后进入 Workspace。
- 画笔/橡皮可以绘制。
- 颜色切换、图层选择/增删/隐藏可用。
- Undo / Redo 可用。
- 滚轮缩放、中键或对应输入路径平移可用。
- `.vvp` 保存后可以重新打开。
- PNG 可以导出。
- UI 文本使用 core 文字引擎渲染；字体缺失时不崩溃并回退。

架构验收：

- 旧 `src/UI`、`src/App`、`src/Tablet`、旧 `src/Render` 不再作为构建入口。
- `main.cpp` 只启动 Windows adapter 和 core runtime。
- 新平台只需实现 adapter 服务集合，不需要修改 core 才能启动相同产品逻辑。

## 12. 风险与策略

- 旧 UI 功能不会一次性完全等价迁移；MVP 优先保证主绘画流程和架构边界。
- 高级工具可以逐步以 core command、core modal、core widget 的方式补回。
- 平台能力缺失时，adapter 应返回空值或 `Unknown`，core 不为单个平台写条件分支。
- 内存信息、显示信息、HDR/SDR 信息只作为 core 决策建议，不把内存所有权交给 adapter。
- 自研字体引擎首轮只覆盖常见 TrueType/OpenType `glyf` 字体、Unicode cmap、kerning、复合 glyph 和灰度 AA；复杂 shaping 与彩色字体作为后续增强。
