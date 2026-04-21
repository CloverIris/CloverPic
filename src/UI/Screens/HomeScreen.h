#pragma once

#include "UI/Core/Window.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Panel.h"
#include <vector>
#include <memory>

namespace VividPic {
namespace UI {

struct ButtonGroup {
    String title;
    std::vector<Ref<Button>> buttons;
};

class HomeScreen : public Window {
public:
    HomeScreen();
    ~HomeScreen() override = default;
    
    bool Initialize();
    
protected:
    void OnPaint(HDC hdc, const Rect& clip) override;
    void OnSize(const Size& newSize) override;
    bool OnCreate() override;
    
    DWORD GetDefaultStyle() const override { 
        return WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME; 
    }
    DWORD GetDefaultExStyle() const override { return WS_EX_CLIENTEDGE; }
    
private:
    void CreateButtonGroups();
    void LayoutButtons();
    void DrawBackground(HDC hdc);
    void DrawTitle(HDC hdc);
    void DrawStatusBar(HDC hdc);
    
    // Button groups
    std::vector<ButtonGroup> m_groups;
    
    // Status bar
    Rect m_statusBarRect;
    String m_languageText = L"Language: (auto)";
    
    // Layout constants
    static constexpr int TitleHeight = 60;
    static constexpr int StatusBarHeight = 32;
    static constexpr int GroupSpacing = 24;
    static constexpr int ButtonHeight = 32;
    static constexpr int ButtonSpacing = 4;
    static constexpr int LeftMargin = 40;
    static constexpr int ButtonWidth = 200;
    
    // Callbacks
    void OnDrawIllustration();
    void OnDrawComic();
    void OnOpenFolder();
    void OnRecentFolders();
    void OnOpenFromCloud();
    void OnLibrary();
    void OnTimelapseGallery();
    void OnSubmitArtwork();
    void OnSubmitFromCloud();
    void OnSubmitComic();
    void OnApplyContest();
    void OnSettings();
    void OnTutorial();
    void OnVideoWorks();
};

} // namespace UI
} // namespace VividPic
