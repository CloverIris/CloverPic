# Agents.md（开发环境与技术约束）

> **本文件为 KimiCode 开发 ViVidPic 的强制约束文档。所有代码生成、架构决策、依赖选择必须优先遵循以下条款。**

## 1. 开发环境（绝对约束）

| 约束项 | 强制要求 | 说明 |
|--------|----------|------|
| **操作系统** | Windows 10 (x64) | 目标运行平台，开发调试必须基于此 |
| **编译器** | MinGW-w64 GCC 13+ | **禁止**使用 MSVC 或 Clang-CL |
| **C++ 标准** | C++20 | 必须完整使用 `-std=c++20`，充分利用 Concepts、Ranges、Coroutines（如适用） |
| **构建系统** | CMake 3.25+ | 生成 MinGW Makefiles 或 Ninja |
| **链接器** | ld/lld (MinGW 自带) | 兼容 Windows 10 SDK 但**不依赖** MSVC runtime |
| **调试器** | GDB (MinGW 自带) 或 LLDB | |

## 2. 技术栈选择原则

### 2.1 允许使用的技术
- **Win32 API**：窗口创建、消息循环、Raw Input、Windows Ink (`WM_POINTER*`)、WinTab (`Wintab32.dll`)
- **Direct2D / Direct3D 11**：渲染后端（MinGW 可通过 `-ld2d1 -ld3d11` 链接系统 DLL）
- **DirectWrite**：字体渲染
- **WIC (Windows Imaging Component)**：图像编解码（PNG/JPEG/TIFF/BMP）
- **STL + 标准库**：`std::filesystem`, `std::thread`, `std::future`, 智能指针
- **第三方库（仅限头文件或 MinGW 可编译）**：
  - **Dear ImGui**（可选，用于调试工具或内部编辑器）
  - **stb_image / stb_image_write**（图像 I/O 备选）
  - **nlohmann/json**（配置与项目元数据）
  - **zlib / minizip**（VVP 文件压缩）

### 2.2 禁止使用的技术
- **Qt / wxWidgets / MFC**：保持"纯 C++"与最小依赖原则，UI 必须自研或基于轻量级方案
- **C++/CLI 或 .NET**：严禁引入 CLR
- **MSVC 特有扩展**：如 `#pragma comment`, `__declspec(uuid)`, COM 智能指针（使用 `WRL` 需谨慎确保 MinGW 兼容）
- **Vulkan / OpenGL**：除非必要，优先使用 Direct2D/D3D（Windows 原生、MinGW 兼容性好）

## 3. 数位板输入层强制规范

所有数位板相关代码必须封装在 `TabletInput` 命名空间下，且**同时**实现以下两个后端：

```cpp
namespace TabletInput {
    // 后端 1：Windows Ink (Windows 8+ Pointer API)
    class WindowsInkDriver {
        bool Initialize(HWND hwnd);
        bool ProcessPointerMessage(UINT msg, WPARAM wParam, LPARAM lParam);
        TabletState GetState() const;
    };
    
    // 后端 2：WinTab (Wacom 兼容层)
    class WinTabDriver {
        bool Initialize(HWND hwnd);
        bool LoadWintab32();
        bool ProcessPacket(WTPKT packet);
        TabletState GetState() const;
    };
    
    // 统一状态结构
    struct TabletState {
        float x, y;
        float pressure;
        float tiltX, tiltY;
        float rotation;
        bool isEraser;
        bool isTouching;
    };
}
```

**运行时策略**：优先尝试 WinTab，若检测到设备则使用；否则回退到 Windows Ink；若两者都失败，则启用鼠标模拟模式（无压感）。

## 4. 内存管理强制规范

- **禁止使用裸 `new/delete`**（除底层内存池实现外），全部使用智能指针与 RAII
- **大内存分配**：画布像素数据必须使用自定义对齐分配（`alignas(32)` 或 `VirtualAlloc`），便于 SIMD 优化
- **瓦片内存池**：实现 `TilePool` 类，使用对象池复用 256x256 RGBA 瓦片内存
- **内存上限检查**：任何可能导致内存超限的操作（新建图层、扩大画布）必须先调用 `MemoryAdvisor::CheckAvailability()`，失败则抛出 `MemoryException` 并由 UI 捕获提示用户

## 5. 构建与打包

```cmake
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

target_link_libraries(VividPic PRIVATE 
    d2d1 dwrite windowscodecs wintab32 
    d3d11 dxgi user32 gdi32 ole32 uuid
)
```

## 6. 代码风格

- 命名：`PascalCase` 类名 / 结构体，`camelCase` 函数与变量，`SCREAMING_SNAKE_CASE` 宏与常量
- 文件编码：UTF-8 with BOM（确保中文资源在 MinGW 下正确编译）
- 头文件：使用 `#pragma once`，接口类使用纯虚函数定义

## 7. M1 已落地架构决策

- **UI 框架**：自研轻量级 retained-mode 框架（`UI::Window` 基类 + `Button`/`Panel` 控件），基于 Win32 API + GDI 自绘，支持暗色主题
- **渲染策略**：GDI 负责 UI chrome（HomeScreen、对话框、面板），Direct2D 仅用于画布渲染区域（M2 实施）
- **项目目录结构**：
  ```
  src/
    App/          # Application 单例
    Core/         # Project/Canvas/Layer/MemoryAdvisor
    Tablet/       # TabletInput 双栈驱动
    UI/
      Core/       # Window/Theme
      Widgets/    # Button/Panel/EditBox/ComboBox/CanvasView
      Screens/    # HomeScreen/NewCanvasDialog/Workspace
    Render/       # RenderBackend/D2DCanvas/TilePool/BrushEngine
    Utils/        # Types
  ```
- **已编译验证模块**：HomeScreen（暗色主题 + 分组按钮）、MemoryAdvisor（系统内存检测 + 预算计算）、TabletInput（Windows Ink + WinTab 动态加载 stub）
