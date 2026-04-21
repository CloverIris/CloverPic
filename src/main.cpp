#include "App/Application.h"
#include "UI/Screens/HomeScreen.h"
#include <windows.h>
#include <memory>
#include <cstdio>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd) {
    // Ensure console stdout/stderr are available for logging
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    // Initialize application
    auto& app = VividPic::Application::GetInstance();
    if (!app.Initialize()) {
        MessageBoxW(nullptr, L"Failed to initialize application", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    // Create and show HomeScreen
    auto homeScreen = std::make_unique<VividPic::UI::HomeScreen>();
    if (!homeScreen->Initialize()) {
        MessageBoxW(nullptr, L"Failed to create main window", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    // Run message loop
    return app.Run();
}
