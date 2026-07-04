#include "Platform/Windows/WindowsHost.h"
#include <windows.h>
#include <cstdio>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nShowCmd;

    // Ensure console stdout/stderr are available for logging
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    CloverPic::Platform::Windows::WindowsHost host;
    if (!host.Initialize(hInstance)) {
        MessageBoxW(nullptr, L"Failed to create main window", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    return host.Run();
}
