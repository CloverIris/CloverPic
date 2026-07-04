#pragma once

#include "Core/App/AppRuntime.h"
#include "Core/Platform/PlatformInterfaces.h"
#include <windows.h>

namespace CloverPic::Platform::Windows {

class WindowsHost final : public Core::IPlatformHost, public Core::ISurfacePresenter {
public:
    WindowsHost();
    ~WindowsHost() override;

    bool Initialize(HINSTANCE instance);
    int Run();

    Size GetViewportSize() const override { return m_viewport; }
    float GetDpiScale() const override { return m_dpiScale; }
    void RequestFrame() override;
    void RequestQuit() override { m_running = false; PostQuitMessage(0); }
    void Present(const Core::RgbaFrame& frame, const std::vector<Rect>& dirtyRects) override;

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void RegisterClass();
    void ResizeFromClient();
    void Paint();
    void PresentToHdc(HDC hdc, const Core::RgbaFrame& frame, const std::vector<Rect>& dirtyRects);
    Input::PointerEvent MakeMouseEvent(Input::PointerAction action, LPARAM lParam, MouseButton button) const;
    Input::KeyEvent MakeKeyEvent(Input::KeyAction action, WPARAM wParam, LPARAM lParam) const;
    uint32_t CurrentModifiers() const;
    bool TranslatePointerPen(UINT msg, WPARAM wParam, Input::PointerEvent& outEvent) const;
    void UpdateDpiFromWindow();

    HINSTANCE m_instance = nullptr;
    HWND m_hwnd = nullptr;
    bool m_running = true;
    float m_dpiScale = 1.0f;
    Size m_viewport;
    Core::AppRuntime m_runtime;
    BITMAPINFO m_bitmapInfo = {};

    static constexpr wchar_t ClassName[] = L"CloverPic_CoreSurfaceWindow";
    static constexpr UINT FrameTimerId = 0x3110;
};

} // namespace CloverPic::Platform::Windows
