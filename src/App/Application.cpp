#include "App/Application.h"
#include "UI/Core/Window.h"
#include "UI/Core/Theme.h"
#include <gdiplus.h>
#include <shellscalingapi.h>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shcore.lib")

namespace VividPic {

Application* Application::s_instance = nullptr;

Application& Application::GetInstance() {
    static Application instance;
    return instance;
}

Application::Application() {
    s_instance = this;
}

Application::~Application() {
    Shutdown();
}

bool Application::Initialize() {
    // Set DPI awareness for crisp rendering on high-DPI displays
    // Try Per-Monitor V2 first (Win10 1703+), fallback to System aware
    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE)) {
            SetProcessDPIAware();
        }
    }
    
    // Detect system DPI and set global UI scale
    UINT dpi = 96;
    if (HMODULE hUser32 = GetModuleHandleW(L"user32.dll")) {
        using GetDpiForSystemFn = UINT(WINAPI*)();
        auto pfn = reinterpret_cast<GetDpiForSystemFn>(GetProcAddress(hUser32, "GetDpiForSystem"));
        if (pfn) {
            dpi = pfn();
        }
    }
    if (dpi == 96) {
        HDC hdc = GetDC(nullptr);
        dpi = GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(nullptr, hdc);
    }
    UI::Theme::Scale = static_cast<float>(dpi) / 96.0f;
    
    // Initialize GDI+
    Gdiplus::GdiplusStartupInput input;
    input.GdiplusVersion = 1;
    input.DebugEventCallback = nullptr;
    input.SuppressBackgroundThread = FALSE;
    input.SuppressExternalCodecs = FALSE;
    
    Gdiplus::GdiplusStartup(&m_gdiplusToken, &input, nullptr);
    
    return true;
}

int Application::Run() {
    MSG msg = {};
    while (m_running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                m_running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        // Idle processing could go here
    }
    
    return static_cast<int>(msg.wParam);
}

void Application::Shutdown() {
    if (m_gdiplusToken != 0) {
        Gdiplus::GdiplusShutdown(m_gdiplusToken);
        m_gdiplusToken = 0;
    }
    FreeConsole();
}

void Application::SetMainWindow(UI::Window* window) {
    m_mainWindow = window;
}

} // namespace VividPic
