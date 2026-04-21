#include "UI/Screens/HomeScreen.h"
#include "UI/Screens/NewCanvasDialog.h"
#include "UI/Screens/Workspace.h"
#include "App/Application.h"
#include <windowsx.h>
#include <shellapi.h>
#include <commdlg.h>
#include <shlobj.h>
#include <cmath>

namespace VividPic {
namespace UI {

HomeScreen::HomeScreen() = default;

bool HomeScreen::Initialize() {
    // Safety: if Theme::Scale is unreasonably low, something went wrong in DPI detection.
    // Force it to 1.25f (the previous comfortable default) so the UI remains usable.
    if (Theme::Scale < 0.5f) {
        Theme::Scale = 1.25f;
    }
    
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    
    // Use screen-relative sizing so the window looks good on any resolution
    int width = static_cast<int>(screenW * 0.55f * Theme::Scale);
    int height = static_cast<int>(screenH * 0.65f * Theme::Scale);
    
    // Clamp to reasonable bounds (never smaller than 900x700, never larger than 1600x1100)
    if (width < Theme::GetSize(900)) width = Theme::GetSize(900);
    if (height < Theme::GetSize(700)) height = Theme::GetSize(700);
    if (width > Theme::GetSize(1600)) width = Theme::GetSize(1600);
    if (height > Theme::GetSize(1100)) height = Theme::GetSize(1100);
    
    int x = (screenW - width) / 2;
    int y = (screenH - height) / 2;
    Rect bounds(x, y, width, height);
    if (!Create(L"ViVidPic v1.0.0", bounds, nullptr)) {
        return false;
    }
    
    // After window creation, query the actual monitor DPI and update Scale if needed
    UINT winDpi = 96;
    if (HMODULE hUser32 = GetModuleHandleW(L"user32.dll")) {
        using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
        auto pfn = reinterpret_cast<GetDpiForWindowFn>(GetProcAddress(hUser32, "GetDpiForWindow"));
        if (pfn) {
            winDpi = pfn(m_hwnd);
        }
    }
    if (winDpi >= 96) {
        float newScale = static_cast<float>(winDpi) / 96.0f;
        if (std::abs(newScale - Theme::Scale) > 0.1f) {
            Theme::Scale = newScale;
            // Re-layout with corrected scale
            OnSize(GetClientBounds().GetSize());
            Invalidate();
        }
    }
    
    Application::GetInstance().SetMainWindow(this);
    return true;
}

bool HomeScreen::OnCreate() {
    CreateButtonGroups();
    LayoutButtons();
    return true;
}

void HomeScreen::CreateButtonGroups() {
    // Group 1: 画画看吧 (Draw)
    ButtonGroup drawGroup;
    drawGroup.title = L"画画看吧";
    
    auto btnDrawIllust = MakeRef<Button>();
    btnDrawIllust->SetText(L"绘制插画");
    btnDrawIllust->SetCallback([this]() { OnDrawIllustration(); });
    drawGroup.buttons.push_back(btnDrawIllust);
    
    auto btnDrawComic = MakeRef<Button>();
    btnDrawComic->SetText(L"绘制漫画");
    btnDrawComic->SetCallback([this]() { OnDrawComic(); });
    drawGroup.buttons.push_back(btnDrawComic);
    
    auto btnOpenFolder = MakeRef<Button>();
    btnOpenFolder->SetText(L"打开文件夹");
    btnOpenFolder->SetCallback([this]() { OnOpenFolder(); });
    drawGroup.buttons.push_back(btnOpenFolder);
    
    auto btnRecent = MakeRef<Button>();
    btnRecent->SetText(L"最近使用的文件夹");
    btnRecent->SetCallback([this]() { OnRecentFolders(); });
    drawGroup.buttons.push_back(btnRecent);
    
    auto btnCloud = MakeRef<Button>();
    btnCloud->SetText(L"从云端打开");
    btnCloud->SetCallback([this]() { OnOpenFromCloud(); });
    drawGroup.buttons.push_back(btnCloud);
    
    auto btnLibrary = MakeRef<Button>();
    btnLibrary->SetText(L"图书馆");
    btnLibrary->SetCallback([this]() { OnLibrary(); });
    drawGroup.buttons.push_back(btnLibrary);
    
    auto btnTimelapse = MakeRef<Button>();
    btnTimelapse->SetText(L"延时摄影画廊");
    btnTimelapse->SetCallback([this]() { OnTimelapseGallery(); });
    drawGroup.buttons.push_back(btnTimelapse);
    
    m_groups.push_back(std::move(drawGroup));
    
    // Group 2: 投稿试试吧 (Submit)
    ButtonGroup submitGroup;
    submitGroup.title = L"投稿试试吧";
    
    auto btnSubmitArt = MakeRef<Button>();
    btnSubmitArt->SetText(L"提交艺术品");
    btnSubmitArt->SetCallback([this]() { OnSubmitArtwork(); });
    submitGroup.buttons.push_back(btnSubmitArt);
    
    auto btnSubmitCloud = MakeRef<Button>();
    btnSubmitCloud->SetText(L"从云端提交");
    btnSubmitCloud->SetCallback([this]() { OnSubmitFromCloud(); });
    submitGroup.buttons.push_back(btnSubmitCloud);
    
    auto btnSubmitComic = MakeRef<Button>();
    btnSubmitComic->SetText(L"漫画投稿");
    btnSubmitComic->SetCallback([this]() { OnSubmitComic(); });
    submitGroup.buttons.push_back(btnSubmitComic);
    
    auto btnContest = MakeRef<Button>();
    btnContest->SetText(L"申请比赛");
    btnContest->SetCallback([this]() { OnApplyContest(); });
    submitGroup.buttons.push_back(btnContest);
    
    m_groups.push_back(std::move(submitGroup));
    
    // Group 3: 其他 (Other)
    ButtonGroup otherGroup;
    otherGroup.title = L"其他";
    
    auto btnSettings = MakeRef<Button>();
    btnSettings->SetText(L"环境设定");
    btnSettings->SetCallback([this]() { OnSettings(); });
    otherGroup.buttons.push_back(btnSettings);
    
    auto btnTutorial = MakeRef<Button>();
    btnTutorial->SetText(L"教程");
    btnTutorial->SetCallback([this]() { OnTutorial(); });
    otherGroup.buttons.push_back(btnTutorial);
    
    auto btnVideo = MakeRef<Button>();
    btnVideo->SetText(L"视频作品");
    btnVideo->SetCallback([this]() { OnVideoWorks(); });
    otherGroup.buttons.push_back(btnVideo);
    
    m_groups.push_back(std::move(otherGroup));
}

void HomeScreen::LayoutButtons() {
    Rect client = GetClientBounds();
    int currentY = Theme::GetSize(TitleHeight) + Theme::GetSize(20);
    int currentX = Theme::GetSize(LeftMargin);
    int maxGroupHeight = 0;
    
    for (auto& group : m_groups) {
        int groupHeight = static_cast<int>(group.buttons.size()) * (Theme::GetSize(ButtonHeight) + Theme::GetSize(ButtonSpacing)) + Theme::GetSize(30);
        
        if (currentY + groupHeight > client.Height() - Theme::GetSize(StatusBarHeight) - Theme::GetSize(20) && currentX == Theme::GetSize(LeftMargin)) {
            // Would overflow, try next column (simplified: we use 2 columns max)
            if (currentX == Theme::GetSize(LeftMargin)) {
                currentX = Theme::GetSize(LeftMargin) + Theme::GetSize(ButtonWidth) + Theme::GetSize(40);
                currentY = Theme::GetSize(TitleHeight) + Theme::GetSize(20);
            }
        }
        
        for (size_t i = 0; i < group.buttons.size(); ++i) {
            auto& btn = group.buttons[i];
            Rect btnBounds(currentX, currentY + Theme::GetSize(30) + static_cast<int>(i) * (Theme::GetSize(ButtonHeight) + Theme::GetSize(ButtonSpacing)), 
                           currentX + Theme::GetSize(ButtonWidth), currentY + Theme::GetSize(30) + static_cast<int>(i) * (Theme::GetSize(ButtonHeight) + Theme::GetSize(ButtonSpacing)) + Theme::GetSize(ButtonHeight));
            btn->Create(L"", btnBounds, this);
        }
        
        currentY += groupHeight + Theme::GetSize(GroupSpacing);
        if (currentY > maxGroupHeight) maxGroupHeight = currentY;
    }
    
    m_statusBarRect = Rect(0, client.Height() - Theme::GetSize(StatusBarHeight), client.Width(), client.Height());
}

void HomeScreen::OnSize(const Size& newSize) {
    // Recreate buttons in new layout
    for (auto& group : m_groups) {
        group.buttons.clear();
    }
    m_groups.clear();
    
    CreateButtonGroups();
    LayoutButtons();
    m_statusBarRect = Rect(0, newSize.height - Theme::GetSize(StatusBarHeight), newSize.width, newSize.height);
    Invalidate();
}

void HomeScreen::OnPaint(HDC hdc, const Rect& clip) {
    DrawBackground(hdc);
    DrawTitle(hdc);
    
    // Draw group titles
    int currentY = Theme::GetSize(TitleHeight) + Theme::GetSize(20);
    int currentX = Theme::GetSize(LeftMargin);
    
    for (auto& group : m_groups) {
        int groupHeight = static_cast<int>(group.buttons.size()) * (Theme::GetSize(ButtonHeight) + Theme::GetSize(ButtonSpacing)) + Theme::GetSize(30);
        
        if (currentY + groupHeight > GetClientBounds().Height() - Theme::GetSize(StatusBarHeight) - Theme::GetSize(20) && currentX == Theme::GetSize(LeftMargin)) {
            if (currentX == Theme::GetSize(LeftMargin)) {
                currentX = Theme::GetSize(LeftMargin) + Theme::GetSize(ButtonWidth) + Theme::GetSize(40);
                currentY = Theme::GetSize(TitleHeight) + Theme::GetSize(20);
            }
        }
        
        // Draw group title
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, Theme::TextPrimary);
        
        HFONT titleFont = CreateFontW(Theme::GetFontSize(16), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                       DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
        HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, titleFont));
        
        RECT titleRect = { currentX, currentY, currentX + Theme::GetSize(ButtonWidth), currentY + Theme::GetSize(24) };
        DrawTextW(hdc, group.title.c_str(), -1, &titleRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        
        SelectObject(hdc, oldFont);
        DeleteObject(titleFont);
        
        currentY += groupHeight + Theme::GetSize(GroupSpacing);
    }
    
    DrawStatusBar(hdc);
}

void HomeScreen::DrawBackground(HDC hdc) {
    Rect client = GetClientBounds();
    HBRUSH bgBrush = Theme::SolidBrush(Theme::BackgroundDark);
    RECT rc = client.ToWin32Rect();
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);
}

void HomeScreen::DrawTitle(HDC hdc) {
    Rect client = GetClientBounds();
    
    // Title background
    HBRUSH titleBrush = Theme::SolidBrush(Theme::BackgroundDark);
    RECT titleRect = { 0, 0, client.Width(), Theme::GetSize(TitleHeight) };
    FillRect(hdc, &titleRect, titleBrush);
    DeleteObject(titleBrush);
    
    // Title text
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::TextPrimary);
    
    HFONT titleFont = CreateFontW(Theme::GetFontSize(28), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, titleFont));
    
    RECT textRect = { Theme::GetSize(LeftMargin), Theme::GetSize(10), client.Width() - Theme::GetSize(LeftMargin), Theme::GetSize(TitleHeight) - Theme::GetSize(10) };
    DrawTextW(hdc, L"ViVidPic", -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    
    // Version
    SetTextColor(hdc, Theme::TextSecondary);
    HFONT verFont = CreateFontW(Theme::GetFontSize(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    SelectObject(hdc, verFont);
    RECT verRect = { Theme::GetSize(LeftMargin) + Theme::GetSize(140), Theme::GetSize(20), Theme::GetSize(LeftMargin) + Theme::GetSize(240), Theme::GetSize(TitleHeight) - Theme::GetSize(10) };
    DrawTextW(hdc, L"v1.0.0", -1, &verRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    
    SelectObject(hdc, oldFont);
    DeleteObject(titleFont);
    DeleteObject(verFont);
    
    // Separator line
    HPEN linePen = CreatePen(PS_SOLID, 1, RGB(0x55, 0x55, 0x55));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, linePen));
    MoveToEx(hdc, Theme::GetSize(LeftMargin), Theme::GetSize(TitleHeight) - 1, nullptr);
    LineTo(hdc, client.Width() - Theme::GetSize(LeftMargin), Theme::GetSize(TitleHeight) - 1);
    SelectObject(hdc, oldPen);
    DeleteObject(linePen);
}

void HomeScreen::DrawStatusBar(HDC hdc) {
    Rect client = GetClientBounds();
    m_statusBarRect = Rect(0, client.Height() - Theme::GetSize(StatusBarHeight), client.Width(), client.Height());
    
    // Status bar background
    HBRUSH bgBrush = Theme::SolidBrush(Theme::PanelBackground);
    RECT sbRc = m_statusBarRect.ToWin32Rect();
    FillRect(hdc, &sbRc, bgBrush);
    DeleteObject(bgBrush);
    
    // Top border
    HPEN linePen = CreatePen(PS_SOLID, 1, RGB(0x55, 0x55, 0x55));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, linePen));
    MoveToEx(hdc, 0, m_statusBarRect.top, nullptr);
    LineTo(hdc, client.Width(), m_statusBarRect.top);
    SelectObject(hdc, oldPen);
    DeleteObject(linePen);
    
    // Language text
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::TextSecondary);
    
    HFONT font = CreateFontW(Theme::GetFontSize(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
    
    RECT langRect = { client.Width() - Theme::GetSize(200), m_statusBarRect.top + Theme::GetSize(4), client.Width() - Theme::GetSize(20), m_statusBarRect.bottom - Theme::GetSize(4) };
    DrawTextW(hdc, m_languageText.c_str(), -1, &langRect, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
    
    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

// Button callbacks
void HomeScreen::OnDrawIllustration() {
    auto dialog = MakeScope<NewCanvasDialog>();
    if (dialog->Initialize()) {
        if (dialog->ShowModal(this)) {
            auto settings = dialog->GetSettings();
            auto project = MakeRef<Project>(L"未命名插画");
            project->GetCanvas().widthPx = settings.width;
            project->GetCanvas().heightPx = settings.height;
            project->GetCanvas().dpi = settings.dpi;
            
            SetVisible(false);
            
            auto workspace = MakeScope<Workspace>();
            if (workspace->Initialize(project)) {
                workspace->SetProject(project);
                workspace.release();
            }
        }
    }
}

void HomeScreen::OnDrawComic() {
    auto dialog = MakeScope<NewCanvasDialog>();
    if (dialog->Initialize()) {
        if (dialog->ShowModal(this)) {
            auto settings = dialog->GetSettings();
            auto project = MakeRef<Project>(L"未命名漫画");
            project->GetCanvas().widthPx = settings.width;
            project->GetCanvas().heightPx = settings.height;
            project->GetCanvas().dpi = settings.dpi;
            
            SetVisible(false);
            
            auto workspace = MakeScope<Workspace>();
            if (workspace->Initialize(project)) {
                workspace->SetProject(project);
                workspace.release();
            }
        }
    }
}

void HomeScreen::OnOpenFolder() {
    BROWSEINFOW bi = {};
    bi.lpszTitle = L"选择项目文件夹";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) {
            ShellExecuteW(nullptr, L"open", path, nullptr, nullptr, SW_SHOWNORMAL);
        }
        CoTaskMemFree(pidl);
    }
}

void HomeScreen::OnRecentFolders() {
    MessageBoxW(m_hwnd, L"最近使用的项目路径列表\nRecent Projects List", 
                L"最近使用的文件夹", MB_OK | MB_ICONINFORMATION);
}

void HomeScreen::OnOpenFromCloud() {
    MessageBoxW(m_hwnd, L"云端项目列表（需登录）\nCloud Projects (Login Required)", 
                L"从云端打开", MB_OK | MB_ICONINFORMATION);
}

void HomeScreen::OnLibrary() {
    MessageBoxW(m_hwnd, L"素材/参考图库管理器\nAsset Library Manager", 
                L"图书馆", MB_OK | MB_ICONINFORMATION);
}

void HomeScreen::OnTimelapseGallery() {
    MessageBoxW(m_hwnd, L"绘画过程回放列表\nTimelapse Gallery", 
                L"延时摄影画廊", MB_OK | MB_ICONINFORMATION);
}

void HomeScreen::OnSubmitArtwork() {
    MessageBoxW(m_hwnd, L"导出并上传到作品投稿平台\nExport and Upload Artwork", 
                L"提交艺术品", MB_OK | MB_ICONINFORMATION);
}

void HomeScreen::OnSubmitFromCloud() {
    MessageBoxW(m_hwnd, L"选择云端项目直接投稿\nSubmit from Cloud", 
                L"从云端提交", MB_OK | MB_ICONINFORMATION);
}

void HomeScreen::OnSubmitComic() {
    MessageBoxW(m_hwnd, L"漫画分页投稿向导\nComic Submission Wizard", 
                L"漫画投稿", MB_OK | MB_ICONINFORMATION);
}

void HomeScreen::OnApplyContest() {
    MessageBoxW(m_hwnd, L"比赛活动列表页面\nContest List", 
                L"申请比赛", MB_OK | MB_ICONINFORMATION);
}

void HomeScreen::OnSettings() {
    MessageBoxW(m_hwnd, L"全局首选项对话框\nPreferences", 
                L"环境设定", MB_OK | MB_ICONINFORMATION);
}

void HomeScreen::OnTutorial() {
    MessageBoxW(m_hwnd, L"内置教程浏览器\nTutorial Browser", 
                L"教程", MB_OK | MB_ICONINFORMATION);
}

void HomeScreen::OnVideoWorks() {
    MessageBoxW(m_hwnd, L"管理导出的延时摄影视频\nVideo Works Manager", 
                L"视频作品", MB_OK | MB_ICONINFORMATION);
}

} // namespace UI
} // namespace VividPic

