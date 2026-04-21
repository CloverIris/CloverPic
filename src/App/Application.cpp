#include "App/Application.h"
#include "UI/Core/Window.h"
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")

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
}

void Application::SetMainWindow(UI::Window* window) {
    m_mainWindow = window;
}

} // namespace VividPic
