#pragma once

#include "Core/App/ProjectManagerRuntime.h"
#include "Core/App/WorkspaceRuntime.h"
#include <memory>
#include <windows.h>

namespace CloverPic::Platform::Windows {

enum class SurfaceRole {
    Home,
    Workspace
};

class WindowsHost;

class WindowsSurfaceWindow final {
public:
    WindowsSurfaceWindow(WindowsHost& owner, SurfaceRole role, Core::RuntimeSurface& runtime);
    ~WindowsSurfaceWindow();

    bool Create(HINSTANCE instance, const wchar_t* title, int clientWidth, int clientHeight,
                bool visible, HWND owner = nullptr, bool borderless = false);
    void Destroy();
    void Show(int command);
    void Hide();
    void RequestFrame();

    HWND GetHwnd() const { return m_hwnd; }
    SurfaceRole GetRole() const { return m_role; }
    bool IsClosed() const { return m_closed; }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static void RegisterClass(HINSTANCE instance);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void ResizeFromClient();
    void Paint();
    void PresentToHdc(HDC hdc, const Core::RgbaFrame& frame, const std::vector<Rect>& dirtyRects);
    Input::PointerEvent MakeMouseEvent(Input::PointerAction action, LPARAM lParam, MouseButton button) const;
    Input::KeyEvent MakeKeyEvent(Input::KeyAction action, WPARAM wParam, LPARAM lParam) const;
    uint32_t CurrentModifiers() const;
    bool TranslatePointerPen(UINT msg, WPARAM wParam, Input::PointerEvent& outEvent) const;
    void UpdateDpiFromWindow();
    void CheckRuntimeCloseRequest();

    WindowsHost& m_owner;
    SurfaceRole m_role;
    Core::RuntimeSurface& m_runtime;
    HINSTANCE m_instance = nullptr;
    HWND m_hwnd = nullptr;
    bool m_closed = false;
    bool m_borderless = false;
    float m_dpiScale = 1.0f;
    Size m_viewport;
    BITMAPINFO m_bitmapInfo = {};

    static constexpr wchar_t ClassName[] = L"CloverPic_SurfaceWindow";
    static constexpr UINT FrameTimerId = 0x3110;
};

class WindowsHost final {
public:
    WindowsHost();
    ~WindowsHost();

    bool Initialize(HINSTANCE instance);
    int Run();

    HWND GetActiveDialogOwner() const;
    void SetActiveDialogOwner(HWND hwnd);
    void RequestQuit();
    void OnSurfaceDestroyed(SurfaceRole role);
    void OnProgramManagerCloseRequested();

private:
    void CreateHomeWindow();
    void CreateWorkspaceWindow();
    void OpenWorkspace(Core::WorkspaceLaunchRequest request);
    void CloseWorkspaceAndReturnHome();
    void PumpRuntimeRequests();
    Size ComputeHomeClientSize() const;
    Size ComputeWorkspaceClientSize() const;

    HINSTANCE m_instance = nullptr;
    HWND m_activeDialogOwner = nullptr;
    bool m_running = true;
    bool m_workspaceDestroyed = false;
    std::unique_ptr<Core::ProjectManagerRuntime> m_homeRuntime;
    std::unique_ptr<Core::WorkspaceRuntime> m_workspaceRuntime;
    std::unique_ptr<WindowsSurfaceWindow> m_homeWindow;
    std::unique_ptr<WindowsSurfaceWindow> m_workspaceWindow;
};

} // namespace CloverPic::Platform::Windows
