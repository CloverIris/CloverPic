#pragma once

#include "Utils/Types.h"
#include <memory>

namespace VividPic {
namespace UI {
    class Window;
    class HomeScreen;
}

class Application {
public:
    Application();
    ~Application();
    
    // Singleton access
    static Application& GetInstance();
    
    // Lifecycle
    bool Initialize();
    int Run();
    void Shutdown();
    
    // Window management
    void SetMainWindow(UI::Window* window);
    UI::Window* GetMainWindow() const { return m_mainWindow; }
    
    // Exit
    void Quit() { m_running = false; }
    
private:
    static Application* s_instance;
    
    bool m_running = true;
    UI::Window* m_mainWindow = nullptr;
    
    // GDI+
    ULONG_PTR m_gdiplusToken = 0;
};

} // namespace VividPic
