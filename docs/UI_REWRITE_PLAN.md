# CloverPic UI 重写计划（基于 CloverPlatform）

## 目标

把 `src/Core/UI` 的手写 scene + `SoftRenderer` 全部替换为 `.cui` + CloverPlatform。
**等效替换**：布局、面板拖停靠、交互、视觉都不得简化。

完成后：

```text
src/Core/           Model / ViewModel / Command / Document / PlatformServices
CloverPlatform/     布局 · 绘制 · 输入 · 动画 · 拖动
src/Platform/Mac    开窗 · 翻译输入 · 贴图
```

Core 里不再有 `Render()`，不再有矩形计算。

## 不变量（每一步都要验证）

1. 同一份项目数据，新旧两版截图差异在阈值内。
2. 录制的输入序列（鼠标/键盘）在两版上产生同样的模型状态。
3. 面板位置、尺寸、停靠关系、层序、滚动位置与旧版一致。
4. 性能预算：整帧 < 8ms，交互重绘 < 1ms（3000×1700）。

## 架构约定

- **Core 不感知像素**：只提供可绑定的数据（`ViewModel`）和命令。
- **框架不感知业务**：只认识节点、属性、动作、拖拽载荷。
- **适配层不感知 UI**：只开窗、翻译输入、贴图。
- 缺能力时扩框架，不在 Core 里补绘制代码。

## 施工结构（直接在 CloverPic 里，不在框架里）

```text
src/Core/UI/Workspace/Clover/
  WorkspaceViewModel.{h,cpp}   UI 快照：工具、笔刷、图层、缩放、文档、活动层
  WorkspacePresenter.{h,cpp}   快照 → surface（set_*）；动作 → Core::AppCommand
src/Resource/ui/workspace.cui  布局 + 控件（画布是 <CloverSurface x:Name="canvas"/>）
```

两条数据流：

- **下行**：`ViewModel::Snapshot` → `clover_surface_set_value/set_label/set_checked/
  set_selected/set_visible/set_children_xaml`；结构性变化才 `reload_cui`。
- **上行**：`clover_surface_take_action` → `change:<name>:<value>` → 解析 →
  `Core::AppCommand` → `EditorSession`。

Workspace 窗口本身变成 Clover surface（与 `CLOVERPIC_CLOVER_PM` 同构），
不开第二个窗口。开关：`CLOVERPIC_CLOVER_WORKSPACE=1`。

## 步骤

### Step 1 — 补齐框架缺口，搭出工作区外壳

**框架**：覆盖层（逃逸裁剪的弹出层）、图标路径绘制、鼠标指针形状。

**交付**：工作区外壳的 `.cui` —— 菜单栏、工具栏、状态栏、左右停靠面板的静态骨架，
画布区域暂时仍由 core 的 `RuntimeSurface` 叠加。

**验收**：新外壳窗口能起来，缩放到 1504×879 时各区域对齐，与旧版截图逐区域比对。

### Step 2 — 面板系统

拖动、停靠、改宽/高、浮动、恢复默认布局、布局持久化。

**验收**：拖动每个面板到任意停靠位，重启后布局还原；与旧版行为一致。

### Step 3 — 图层列表与画布

缩略图上传、滚动、层序拖拽、可见性/锁定切换、右键菜单（依赖 Step 1 的覆盖层）。

**验收**：20 层项目的滚动与重排在两版一致。

### Step 4 — 色板与取色器

HSV 方框 + 色相条 + 数值输入 + 最近色。依赖渐变、拖拽、覆盖层。

**验收**：取色结果与旧版 RGB 完全一致。

### Step 5 — 画布零拷贝

画布改为框架直接内存区，删除 core 每帧 RGBA10→BGRA 合成与整块 blit。

**验收**：整帧耗时下降，画布内容像素一致。

### Step 6 — 删除旧的 UI 代码

移除 `WorkspaceNodeRenderer`、`ProjectManagerRenderer`、`IconPainter`、
`SoftwareRenderer` 的 UI 部分与 `FrameScheduler`。

**验收**：`src/Core` 里不再出现 `Render(`；`cargo`+`cmake` 全绿；性能预算达成。

## 验证工具

- `CloverPlatform/crates/clover-demo` 渲染固定样例，作为像素基线。
- 输入回放脚本：把录制的事件序列喂给两个实现，比较模型状态。
- `docs/UI_PERFORMANCE_REVIEW.md` 的采样方法作为性能回归手段。

## 风险

| 风险 | 应对 |
|---|---|
| 面板拖停靠语义复杂，容易做丢 | Step 2 单独一轮，先写行为清单再写代码 |
| 图标依赖路径绘制，可能拖进度 | 先用位图上传兜底，路径绘制并行推进 |
| 旧版有隐藏交互（键盘快捷键、双击等） | 每步做输入回放，用差异驱动补齐 |
