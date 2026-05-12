#include "UI/Screens/Workspace.h"
#include "App/Application.h"
#include "Render/BrushEngine.h"
#include "Render/BrushPresetManager.h"
#include "UI/Core/Theme.h"
#include "Core/History.h"
#include "Core/ProjectIO.h"
#include "Core/Filters.h"
#include "UI/Dialogs/FilterDialog.h"
#include <sstream>

namespace VividPic {
namespace UI {

Workspace::Workspace() = default;

bool Workspace::Initialize(Ref<Project> project) {
    m_project = project;
    
    // Responsive initial size: 85% of primary monitor work area
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    HMONITOR hMon = MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    int initW = static_cast<int>(1400 * Theme::Scale);
    int initH = static_cast<int>(900 * Theme::Scale);
    if (GetMonitorInfo(hMon, &mi)) {
        int workW = mi.rcWork.right - mi.rcWork.left;
        int workH = mi.rcWork.bottom - mi.rcWork.top;
        initW = static_cast<int>(workW * 0.85f);
        initH = static_cast<int>(workH * 0.85f);
    }
    int minW = static_cast<int>(1024 * Theme::Scale);
    int minH = static_cast<int>(768 * Theme::Scale);
    if (initW < minW) initW = minW;
    if (initH < minH) initH = minH;
    
    Rect bounds(50, 50, initW, initH);
    if (!Create(L"ViVidPic - Workspace", bounds, nullptr)) {
        return false;
    }
    
    // Fade-in animation
    ShowWindow(m_hwnd, SW_HIDE);
    AnimateWindow(m_hwnd, 150, AW_ACTIVATE | AW_BLEND);
    
    return true;
}

void Workspace::SetProject(Ref<Project> project) {
    m_project = project;
    if (m_canvasView && m_project) {
        const auto& canvas = m_project->GetCanvas();
        Color bgColor = canvas.transparent ? Color::FromHex(0xFFFFFF) : Color::FromHex(0xFFFFFF);
        m_canvasView->InitializeCanvas(canvas.widthPx, canvas.heightPx, bgColor, canvas.transparent, canvas.initialLayerType);
        
        if (m_navigatorPanel) {
            m_navigatorPanel->SetCanvasSize(canvas.widthPx, canvas.heightPx);
        }
        if (m_layersPanel) {
            m_layersPanel->SetLayerManager(m_canvasView->GetLayerManager());
        }
    }
}

bool Workspace::OnCreate() {
    // Initialize brush preset manager
    Render::BrushPresetManager::GetInstance().Initialize();

    BuildMenus();
    DragAcceptFiles(m_hwnd, TRUE);
    
    Rect client = GetClientBounds();
    int contentTop = Theme::GetSize(MenuBarHeight) + Theme::GetSize(ToolbarHeight);
    int contentBottom = client.Height() - Theme::GetSize(StatusBarHeight);
    int leftPanelX = Theme::GetSize(ToolBarWidth);
    int rightPanelX = client.Width() - m_rightPanelWidth;
    int canvasLeft = leftPanelX + m_leftPanelWidth;
    
    // Create left tool bar
    m_toolBar = MakeScope<ToolBar>();
    Rect toolBarRect(0, contentTop, Theme::GetSize(ToolBarWidth), contentBottom);
    m_toolBar->Create(L"", toolBarRect, this);
    m_toolBar->SetOnStatusMessage([this](const wchar_t* msg) {
        m_saveStatusText = msg;
        m_saveStatusTime = GetTickCount64();
        RefreshStatusBar();
    });
    m_toolBar->SetOnToolChanged([this](ToolType tool) {
        if (m_canvasView) {
            m_canvasView->SetCurrentTool(tool);
            m_canvasView->InvalidateCanvas();
        }
    });
    
    // Create CanvasView
    m_canvasView = MakeScope<CanvasView>();
    Rect canvasRect(canvasLeft, contentTop, rightPanelX, contentBottom);
    m_canvasView->Create(L"", canvasRect, this);
    m_canvasView->SetOnToolChanged([this](ToolType tool) {
        if (m_toolBar) m_toolBar->SetCurrentTool(tool);
    });
    
    auto collapseCallback = [this]() { LayoutPanels(); };
    
    // Create left panels
    m_colorsPanel = MakeScope<ColorsPanel>();
    m_colorsPanel->Create(L"", Rect(leftPanelX, contentTop, leftPanelX + m_leftPanelWidth, contentTop + 100), this);
    m_colorsPanel->SetCollapsible(true);
    m_colorsPanel->SetOnCollapsedChanged(collapseCallback);
    m_colorsPanel->SetOnColorChanged([this](const Color& color) {
        if (m_canvasView) m_canvasView->SetBrushColor(color);
    });
    
    m_brushPanel = MakeScope<BrushPanel>();
    m_brushPanel->Create(L"", Rect(leftPanelX, contentTop + 100, leftPanelX + m_leftPanelWidth, contentTop + 200), this);
    m_brushPanel->SetCollapsible(true);
    m_brushPanel->SetOnCollapsedChanged(collapseCallback);
    m_brushPanel->SetOnSizeChanged([this](float size) { Render::BrushEngine::GetInstance().SetSize(size); RefreshStatusBar(); });
    m_brushPanel->SetOnOpacityChanged([this](float opacity) { Render::BrushEngine::GetInstance().SetOpacity(opacity); RefreshStatusBar(); });
    m_brushPanel->SetOnFlowChanged([this](float flow) { Render::BrushEngine::GetInstance().SetFlow(flow); RefreshStatusBar(); });
    m_brushPanel->SetOnSpacingChanged([this](float spacing) { Render::BrushEngine::GetInstance().SetSpacing(spacing); RefreshStatusBar(); });
    m_brushPanel->SetOnWetMixChanged([this](float wetMix) { Render::BrushEngine::GetInstance().SetWetMix(wetMix); RefreshStatusBar(); });
    m_brushPanel->SetOnTipTypeChanged([this](Render::BrushTipType type) { Render::BrushEngine::GetInstance().SetTipType(type); RefreshStatusBar(); });
    m_brushPanel->SetOnPresetSelected([this](int index) {
        Render::BrushPresetManager::GetInstance().ApplyPreset(index);
        auto& engine = Render::BrushEngine::GetInstance();
        m_brushPanel->SetBrushSize(engine.GetSize());
        m_brushPanel->SetBrushOpacity(engine.GetOpacity());
        m_brushPanel->SetBrushFlow(engine.GetFlow());
        m_brushPanel->SetBrushSpacing(engine.GetSpacing());
        m_brushPanel->SetBrushWetMix(engine.GetWetMix());
        m_brushPanel->SetTipType(engine.GetTipType());
        m_brushPanel->SetActivePreset(index);
        RefreshStatusBar();
    });
    
    // Create right panels
    m_layersPanel = MakeScope<LayersPanel>();
    m_layersPanel->Create(L"", Rect(rightPanelX, contentTop, client.Width(), contentTop + 100), this);
    m_layersPanel->SetCollapsible(true);
    m_layersPanel->SetOnCollapsedChanged(collapseCallback);
    
    m_navigatorPanel = MakeScope<NavigatorPanel>();
    m_navigatorPanel->Create(L"", Rect(rightPanelX, contentTop + 100, client.Width(), contentTop + 200), this);
    m_navigatorPanel->SetCollapsible(true);
    m_navigatorPanel->SetOnCollapsedChanged(collapseCallback);
    
    m_brushSizePanel = MakeScope<BrushSizePanel>();
    m_brushSizePanel->Create(L"", Rect(rightPanelX, contentTop + 200, client.Width(), contentBottom), this);
    m_brushSizePanel->SetCollapsible(true);
    m_brushSizePanel->SetOnCollapsedChanged(collapseCallback);
    m_brushSizePanel->SetOnSizeChanged([this](float size) {
        Render::BrushEngine::GetInstance().SetSize(size);
        if (m_brushPanel) m_brushPanel->SetBrushSize(size);
        RefreshStatusBar();
    });
    m_navigatorPanel->SetOnPanChanged([this](float x, float y) {
        if (m_canvasView) {
            m_canvasView->SetPan(x, y);
            m_canvasView->InvalidateCanvas();
        }
    });
    m_navigatorPanel->SetOnZoomChanged([this](float zoom) {
        if (!m_canvasView) return;
        if (zoom < 0) {
            m_canvasView->FitToWindow();
        } else {
            m_canvasView->SetZoom(zoom);
        }
        m_canvasView->InvalidateCanvas();
    });
    
    if (m_project) {
        const auto& canvas = m_project->GetCanvas();
        Color bgColor = Color::FromHex(0xFFFFFF);
        m_canvasView->InitializeCanvas(canvas.widthPx, canvas.heightPx, bgColor, canvas.transparent, canvas.initialLayerType);
        
        if (m_navigatorPanel) {
            m_navigatorPanel->SetCanvasSize(canvas.widthPx, canvas.heightPx);
        }
        if (m_layersPanel) {
            m_layersPanel->SetLayerManager(m_canvasView->GetLayerManager());
        }
    }
    
    LayoutPanels();
    return true;
}

void Workspace::OnSize(const Size& newSize) {
    LayoutPanels();
}

void Workspace::LayoutPanels() {
    Rect client = GetClientBounds();
    int rightPanelLeft = client.Width() - m_rightPanelWidth;
    int contentTop = Theme::GetSize(MenuBarHeight) + Theme::GetSize(ToolbarHeight);
    int contentBottom = client.Height() - Theme::GetSize(StatusBarHeight);
    int contentHeight = contentBottom - contentTop;
    int leftPanelX = Theme::GetSize(ToolBarWidth);
    int canvasLeft = leftPanelX + m_leftPanelWidth;
    
    if (m_toolBar) {
        Rect tbRect(0, contentTop, Theme::GetSize(ToolBarWidth), contentBottom);
        m_toolBar->SetBounds(tbRect);
    }
    
    if (m_canvasView) {
        Rect canvasRect(canvasLeft, contentTop, rightPanelLeft, contentBottom);
        m_canvasView->SetBounds(canvasRect);
    }
    
    int headerHeight = Theme::GetSize(28);
    int minPanelHeight = Theme::GetSize(80);
    
    // Left side panels
    int colorsBase = Theme::GetSize(260);
    int brushBase = Theme::GetSize(240);
    bool colorsCollapsed = m_colorsPanel && m_colorsPanel->IsCollapsed();
    bool brushCollapsed = m_brushPanel && m_brushPanel->IsCollapsed();
    
    int leftActiveBase = (colorsCollapsed ? 0 : colorsBase) + (brushCollapsed ? 0 : brushBase);
    int leftFixed = (colorsCollapsed ? headerHeight : 0) + (brushCollapsed ? headerHeight : 0);
    int leftFlex = contentHeight - leftFixed;
    if (leftFlex < 0) leftFlex = 0;
    
    int colorsH = colorsCollapsed ? headerHeight : colorsBase;
    int brushH = brushCollapsed ? headerHeight : brushBase;
    if (leftActiveBase > 0 && leftFlex > 0) {
        colorsH = colorsCollapsed ? headerHeight : std::max(minPanelHeight, colorsBase * leftFlex / leftActiveBase);
        brushH = brushCollapsed ? headerHeight : std::max(minPanelHeight, brushBase * leftFlex / leftActiveBase);
    }
    // Adjust to fill exact height
    int leftTotal = colorsH + brushH;
    if (leftTotal < contentHeight && !colorsCollapsed) {
        colorsH += contentHeight - leftTotal;
    } else if (leftTotal < contentHeight && !brushCollapsed) {
        brushH += contentHeight - leftTotal;
    }
    if (leftTotal > contentHeight) {
        if (!brushCollapsed) brushH -= leftTotal - contentHeight;
        if (brushH < minPanelHeight) brushH = minPanelHeight;
    }
    
    if (m_colorsPanel) {
        Rect r(leftPanelX, contentTop, leftPanelX + m_leftPanelWidth, contentTop + colorsH);
        m_colorsPanel->SetBounds(r);
    }
    if (m_brushPanel) {
        Rect r(leftPanelX, contentTop + colorsH, leftPanelX + m_leftPanelWidth, contentBottom);
        m_brushPanel->SetBounds(r);
    }
    
    // Right side panels
    int layersBase = Theme::GetSize(300);
    int navBase = Theme::GetSize(180);
    int bsBase = Theme::GetSize(200);
    bool layersCollapsed = m_layersPanel && m_layersPanel->IsCollapsed();
    bool navCollapsed = m_navigatorPanel && m_navigatorPanel->IsCollapsed();
    bool bsCollapsed = m_brushSizePanel && m_brushSizePanel->IsCollapsed();
    
    int rightActiveBase = (layersCollapsed ? 0 : layersBase) + (navCollapsed ? 0 : navBase) + (bsCollapsed ? 0 : bsBase);
    int rightFixed = (layersCollapsed ? headerHeight : 0) + (navCollapsed ? headerHeight : 0) + (bsCollapsed ? headerHeight : 0);
    int rightFlex = contentHeight - rightFixed;
    if (rightFlex < 0) rightFlex = 0;
    
    int layersH = layersCollapsed ? headerHeight : layersBase;
    int navH = navCollapsed ? headerHeight : navBase;
    int bsH = bsCollapsed ? headerHeight : bsBase;
    if (rightActiveBase > 0 && rightFlex > 0) {
        layersH = layersCollapsed ? headerHeight : std::max(minPanelHeight, layersBase * rightFlex / rightActiveBase);
        navH = navCollapsed ? headerHeight : std::max(minPanelHeight, navBase * rightFlex / rightActiveBase);
        bsH = bsCollapsed ? headerHeight : std::max(minPanelHeight, bsBase * rightFlex / rightActiveBase);
    }
    // Adjust to fill exact height
    int rightTotal = layersH + navH + bsH;
    if (rightTotal < contentHeight && !bsCollapsed) {
        bsH += contentHeight - rightTotal;
    } else if (rightTotal < contentHeight && !navCollapsed) {
        navH += contentHeight - rightTotal;
    } else if (rightTotal < contentHeight && !layersCollapsed) {
        layersH += contentHeight - rightTotal;
    }
    if (rightTotal > contentHeight) {
        if (!bsCollapsed) bsH -= rightTotal - contentHeight;
        if (bsH < minPanelHeight) bsH = minPanelHeight;
    }
    
    if (m_layersPanel) {
        Rect r(rightPanelLeft, contentTop, client.Width(), contentTop + layersH);
        m_layersPanel->SetBounds(r);
    }
    if (m_navigatorPanel) {
        Rect r(rightPanelLeft, contentTop + layersH, client.Width(), contentTop + layersH + navH);
        m_navigatorPanel->SetBounds(r);
    }
    if (m_brushSizePanel) {
        Rect r(rightPanelLeft, contentTop + layersH + navH, client.Width(), contentBottom);
        m_brushSizePanel->SetBounds(r);
    }
}

void Workspace::OnPaint(HDC hdc, const Rect& clip) {
    Rect client = GetClientBounds();
    
    HBRUSH bgBrush = Theme::CachedBrush(Theme::CanvasOuter);
    RECT rc = client.ToWin32Rect();
    FillRect(hdc, &rc, bgBrush);
    
    DrawMenuBar(hdc);
    DrawToolbar(hdc);
    DrawStatusBar(hdc);
    DrawSplitters(hdc);
    
    // Dropdown is rendered in a popup window to avoid child-window occlusion
    if (m_openMenuIndex >= 0) {
        if (!m_dropdownWindow || !m_dropdownWindow->IsVisible()) {
            // Calculate position
            int x = Theme::GetSize(8);
            HDC memdc = GetDC(m_hwnd);
            HFONT font = CreateFontW(Theme::GetFontSize(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
            HFONT old = static_cast<HFONT>(SelectObject(memdc, font));
            for (int i = 0; i < m_openMenuIndex; ++i) {
                SIZE ts;
                GetTextExtentPoint32W(memdc, m_menus[i].name.c_str(), static_cast<int>(m_menus[i].name.length()), &ts);
                x += ts.cx + Theme::GetSize(16);
            }
            SIZE ts;
            GetTextExtentPoint32W(memdc, m_menus[m_openMenuIndex].name.c_str(), static_cast<int>(m_menus[m_openMenuIndex].name.length()), &ts);
            SelectObject(memdc, old);
            // font cached
            ReleaseDC(m_hwnd, memdc);
            
            POINT pt = { m_menuItemRects[m_openMenuIndex].left, Theme::GetSize(MenuBarHeight) };
            ClientToScreen(m_hwnd, &pt);
            ShowMenuDropdown(m_openMenuIndex, pt.x, pt.y, m_menuItemRects[m_openMenuIndex].Width());
        }
    } else if (m_openMenuIndex < 0 && m_dropdownWindow && m_dropdownWindow->IsVisible()) {
        HideMenuDropdown();
    }
}

void Workspace::DrawMenuBar(HDC hdc) {
    Rect client = GetClientBounds();
    
    HBRUSH brush = Theme::CachedBrush(Theme::BackgroundDark);
    RECT menuRc = { 0, 0, client.Width(), Theme::GetSize(MenuBarHeight) };
    FillRect(hdc, &menuRc, brush);
    
    SetBkMode(hdc, TRANSPARENT);
    HFONT font = CreateFontW(Theme::GetFontSize(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
    
    int x = Theme::GetSize(8);
    m_menuItemRects.clear();
    m_menuItemRects.reserve(m_menus.size());
    for (const auto& menu : m_menus) {
        SetTextColor(hdc, Theme::TextPrimary);
        SIZE textSize;
        GetTextExtentPoint32W(hdc, menu.name.c_str(), static_cast<int>(menu.name.length()), &textSize);
        int itemW = textSize.cx + Theme::GetSize(16);
        
        RECT textRect = { x, 0, x + itemW, Theme::GetSize(MenuBarHeight) };
        DrawTextW(hdc, menu.name.c_str(), -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
        m_menuItemRects.push_back(Rect(x, 0, x + itemW, Theme::GetSize(MenuBarHeight)));
        x += itemW;
    }
    
    SelectObject(hdc, oldFont);
    // font cached
    
    HPEN linePen = CreatePen(PS_SOLID, 1, Theme::BorderDark);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, linePen));
    MoveToEx(hdc, 0, Theme::GetSize(MenuBarHeight), nullptr);
    LineTo(hdc, client.Width(), Theme::GetSize(MenuBarHeight));
    SelectObject(hdc, oldPen);
    DeleteObject(linePen);
}

void Workspace::DrawToolbar(HDC hdc) {
    Rect client = GetClientBounds();
    
    HBRUSH brush = Theme::CachedBrush(Theme::PanelBackground);
    RECT toolbarRc = { 0, Theme::GetSize(MenuBarHeight), client.Width(), Theme::GetSize(MenuBarHeight) + Theme::GetSize(ToolbarHeight) };
    FillRect(hdc, &toolbarRc, brush);
    
    DrawToolbarButtons(hdc);
    
    HPEN linePen = CreatePen(PS_SOLID, 1, Theme::BorderDark);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, linePen));
    MoveToEx(hdc, 0, Theme::GetSize(MenuBarHeight) + Theme::GetSize(ToolbarHeight), nullptr);
    LineTo(hdc, client.Width(), Theme::GetSize(MenuBarHeight) + Theme::GetSize(ToolbarHeight));
    SelectObject(hdc, oldPen);
    DeleteObject(linePen);
}

void Workspace::BuildMenus() {
    m_menus.push_back({ L"文件(F)", { L"新建窗口", L"打开", L"保存", L"导出", L"退出" } });
    m_menus.push_back({ L"编辑(E)", { L"撤销", L"重做", L"首选项" } });
    m_menus.push_back({ L"图层(L)", { L"新建图层", L"删除图层", L"合并" } });
    m_menus.push_back({ L"滤镜(R)", { L"亮度/对比度", L"色相/饱和度", L"高斯模糊", L"锐化", L"反相", L"阈值" } });
    m_menus.push_back({ L"工具(T)", { L"笔刷", L"橡皮", L"吸管" } });
    m_menus.push_back({ L"窗口(W)", { L"初始化布局" } });
    m_menus.push_back({ L"Help", { L"关于" } });
}

void Workspace::OnMenuItemClicked(int menuIndex, int itemIndex) {
    if (menuIndex == 0 && itemIndex == 0) { // File > New Window
        SetVisible(false);
        auto* appWindow = Application::GetInstance().GetMainWindow();
        if (appWindow) {
            appWindow->SetVisible(true);
        }
    } else if (menuIndex == 0 && itemIndex == 1) { // File > Open
        wchar_t filePath[MAX_PATH] = {0};
        OPENFILENAMEW ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = m_hwnd;
        ofn.lpstrFilter = L"VividPic Project (*.vvp)\0*.vvp\0";
        ofn.lpstrFile = filePath;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
        ofn.lpstrDefExt = L"vvp";
        if (GetOpenFileNameW(&ofn)) {
            auto& lm = LayerManager::GetInstance();
            auto loadedProject = ProjectSerializer::LoadProject(filePath, &lm);
            if (loadedProject) {
                m_project = loadedProject;
                m_currentFilePath = filePath;
                const auto& canvas = m_project->GetCanvas();
                Color bgColor = Color::FromHex(0xFFFFFF);
                m_canvasView->InitializeCanvas(canvas.widthPx, canvas.heightPx, bgColor, canvas.transparent, canvas.initialLayerType, false);
                if (m_navigatorPanel) {
                    m_navigatorPanel->SetCanvasSize(canvas.widthPx, canvas.heightPx);
                }
                if (m_layersPanel) {
                    m_layersPanel->SetLayerManager(m_canvasView->GetLayerManager());
                }
                m_canvasView->InvalidateCanvas();
            } else {
                MessageBoxW(m_hwnd, L"无法打开项目文件", L"打开", MB_OK | MB_ICONERROR);
            }
        }
    } else if (menuIndex == 0 && itemIndex == 2) { // File > Save
        if (m_currentFilePath.empty()) {
            wchar_t filePath[MAX_PATH] = {0};
            OPENFILENAMEW ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = m_hwnd;
            ofn.lpstrFilter = L"VividPic Project (*.vvp)\0*.vvp\0";
            ofn.lpstrFile = filePath;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_OVERWRITEPROMPT;
            ofn.lpstrDefExt = L"vvp";
            if (GetSaveFileNameW(&ofn)) {
                m_currentFilePath = filePath;
            }
        }
        if (!m_currentFilePath.empty()) {
            if (ProjectSerializer::SaveProject(m_currentFilePath, m_project.get(), m_canvasView->GetLayerManager())) {
                ShowToast(L"保存成功");
            } else {
                MessageBoxW(m_hwnd, L"保存失败", L"保存", MB_OK | MB_ICONERROR);
                ShowToast(L"保存失败");
            }
        }
    } else if (menuIndex == 0 && itemIndex == 3) { // File > Export
        wchar_t filePath[MAX_PATH] = {0};
        OPENFILENAMEW ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = m_hwnd;
        ofn.lpstrFilter = L"PNG Image (*.png)\0*.png\0";
        ofn.lpstrFile = filePath;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_OVERWRITEPROMPT;
        ofn.lpstrDefExt = L"png";
        if (GetSaveFileNameW(&ofn)) {
            if (ProjectSerializer::ExportPNG(filePath, m_canvasView->GetLayerManager())) {
                ShowToast(L"导出成功");
            } else {
                MessageBoxW(m_hwnd, L"导出失败", L"导出", MB_OK | MB_ICONERROR);
                ShowToast(L"导出失败");
            }
        }
    } else if (menuIndex == 1 && itemIndex == 0) { // Edit > Undo
        HistoryManager::GetInstance().Undo();
        if (m_canvasView) m_canvasView->InvalidateCanvas();
        ShowToast(L"已撤销");
    } else if (menuIndex == 1 && itemIndex == 1) { // Edit > Redo
        HistoryManager::GetInstance().Redo();
        if (m_canvasView) m_canvasView->InvalidateCanvas();
        ShowToast(L"已重做");
    } else if (menuIndex == 3) { // Filter menu
        auto* layerManager = m_canvasView ? m_canvasView->GetLayerManager() : nullptr;
        if (!layerManager) return;
        auto layer = layerManager->GetActiveLayer();
        if (!layer) {
            MessageBoxW(m_hwnd, L"请先选择一个图层", L"滤镜", MB_OK | MB_ICONWARNING);
            return;
        }

        if (itemIndex == 0) { // Brightness/Contrast
            FilterDialog dialog;
            std::vector<FilterParamDef> params = {
                { L"亮度", -100, 100, 0, 0 },
                { L"对比度", -100, 100, 0, 0 }
            };
            if (dialog.Initialize(L"亮度/对比度", params) && dialog.ShowModal(this)) {
                auto values = dialog.GetValues();
                CaptureLayerSnapshot(layer.get());
                Filters::ApplyBrightnessContrast(layer.get(), values[0], values[1]);
                m_canvasView->InvalidateCanvas();
            }
        } else if (itemIndex == 1) { // Hue/Saturation
            FilterDialog dialog;
            std::vector<FilterParamDef> params = {
                { L"色相", -180, 180, 0, 0 },
                { L"饱和度", 0, 200, 100, 100 }
            };
            if (dialog.Initialize(L"色相/饱和度", params) && dialog.ShowModal(this)) {
                auto values = dialog.GetValues();
                CaptureLayerSnapshot(layer.get());
                Filters::ApplyHueSaturation(layer.get(), values[0], values[1]);
                m_canvasView->InvalidateCanvas();
            }
        } else if (itemIndex == 2) { // Gaussian Blur
            FilterDialog dialog;
            std::vector<FilterParamDef> params = {
                { L"半径", 1, 20, 3, 3 }
            };
            if (dialog.Initialize(L"高斯模糊", params) && dialog.ShowModal(this)) {
                auto values = dialog.GetValues();
                CaptureLayerSnapshot(layer.get());
                Filters::ApplyGaussianBlur(layer.get(), values[0]);
                m_canvasView->InvalidateCanvas();
            }
        } else if (itemIndex == 3) { // Sharpen
            FilterDialog dialog;
            std::vector<FilterParamDef> params = {
                { L"强度", 0, 100, 50, 50 }
            };
            if (dialog.Initialize(L"锐化", params) && dialog.ShowModal(this)) {
                auto values = dialog.GetValues();
                CaptureLayerSnapshot(layer.get());
                Filters::ApplySharpen(layer.get(), values[0]);
                m_canvasView->InvalidateCanvas();
            }
        } else if (itemIndex == 4) { // Invert
            CaptureLayerSnapshot(layer.get());
            Filters::ApplyInvert(layer.get());
            m_canvasView->InvalidateCanvas();
        } else if (itemIndex == 5) { // Threshold
            FilterDialog dialog;
            std::vector<FilterParamDef> params = {
                { L"阈值", 0, 255, 128, 128 }
            };
            if (dialog.Initialize(L"阈值", params) && dialog.ShowModal(this)) {
                auto values = dialog.GetValues();
                CaptureLayerSnapshot(layer.get());
                Filters::ApplyThreshold(layer.get(), static_cast<uint8_t>(values[0]));
                m_canvasView->InvalidateCanvas();
            }
        }
    } else {
        std::wstring msg = L"菜单: " + m_menus[menuIndex].name + L" > " + m_menus[menuIndex].items[itemIndex];
        MessageBoxW(m_hwnd, msg.c_str(), L"功能占位符", MB_OK);
        ShowToast(msg.c_str());
    }
}

void Workspace::CaptureLayerSnapshot(Layer* layer) {
    if (!layer) return;
    auto undoItem = std::make_unique<StrokeUndoItem>(layer);
    uint32_t gw = layer->GetGridWidth();
    uint32_t gh = layer->GetGridHeight();
    for (uint32_t gy = 0; gy < gh; ++gy) {
        for (uint32_t gx = 0; gx < gw; ++gx) {
            Render::Tile* tile = layer->GetTile(gx, gy);
            if (!tile) continue;
            undoItem->CaptureTile(gx, gy, tile->data, Render::TILE_BYTES);
        }
    }
    undoItem->CaptureRedoTiles();
    HistoryManager::GetInstance().Push(std::move(undoItem));
}

void Workspace::DoSave(bool saveAs) {
    if (saveAs || m_currentFilePath.empty()) {
        wchar_t filePath[MAX_PATH] = {0};
        OPENFILENAMEW ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = m_hwnd;
        ofn.lpstrFilter = L"VividPic Project (*.vvp)\0*.vvp\0";
        ofn.lpstrFile = filePath;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_OVERWRITEPROMPT;
        ofn.lpstrDefExt = L"vvp";
        if (GetSaveFileNameW(&ofn)) {
            m_currentFilePath = filePath;
        } else {
            return; // User cancelled
        }
    }
    if (!m_currentFilePath.empty() && m_canvasView) {
        if (ProjectSerializer::SaveProject(m_currentFilePath, m_project.get(), m_canvasView->GetLayerManager())) {
            m_saveStatusText = L"已保存";
            m_saveStatusTime = GetTickCount64();
        } else {
            m_saveStatusText = L"保存失败";
            m_saveStatusTime = GetTickCount64();
        }
        RefreshStatusBar();
    }
}

void Workspace::RefreshStatusBar() {
    Rect client = GetClientBounds();
    int sbTop = client.Height() - Theme::GetSize(StatusBarHeight);
    RECT rc = { 0, sbTop, client.Width(), client.Height() };
    ::InvalidateRect(m_hwnd, &rc, FALSE);
}

void Workspace::ShowToast(const wchar_t* text) {
    if (!m_toastWindow) {
        m_toastWindow = MakeScope<ToastWindow>();
    }
    m_toastWindow->Show(text);
}

void Workspace::OnMouseDown(const Point& pos, MouseButton button) {
    if (button != MouseButton::Left) return;
    
    // Check splitters first
    SplitterSide side = HitTestSplitter(pos);
    if (side != SplitterSide::None) {
        m_splitterDragging = true;
        m_splitterSide = side;
        m_splitterDragStartX = pos.x;
        m_splitterDragStartWidth = (side == SplitterSide::Left) ? m_leftPanelWidth : m_rightPanelWidth;
        SetCapture(m_hwnd);
        Invalidate();
        return;
    }
    
    // Check menu bar
    if (pos.y < Theme::GetSize(MenuBarHeight)) {
        int menuIdx = HitTestMenuItem(pos);
        if (menuIdx >= 0) {
            if (m_openMenuIndex == menuIdx) {
                m_openMenuIndex = -1;
            } else {
                m_openMenuIndex = menuIdx;
            }
            Invalidate();
            return;
        } else {
            m_openMenuIndex = -1;
            Invalidate();
            return;
        }
    }
    
    // Click outside open menu closes it
    if (m_openMenuIndex >= 0) {
        m_openMenuIndex = -1;
        Invalidate();
        return;
    }
    
    // Check toolbar
    if (pos.y >= Theme::GetSize(MenuBarHeight) && pos.y < Theme::GetSize(MenuBarHeight) + Theme::GetSize(ToolbarHeight)) {
        int btnIdx = HitTestToolbarButton(pos);
        switch (btnIdx) {
            case 0: { // Undo
                HistoryManager::GetInstance().Undo();
                if (m_canvasView) m_canvasView->InvalidateCanvas();
                break;
            }
            case 1: { // Redo
                HistoryManager::GetInstance().Redo();
                if (m_canvasView) m_canvasView->InvalidateCanvas();
                break;
            }
            case 2: { // Toggle B/E
                if (m_canvasView) {
                    auto current = m_canvasView->GetCurrentTool();
                    if (current == ToolType::Brush) {
                        m_canvasView->SetCurrentTool(ToolType::Eraser);
                    } else {
                        m_canvasView->SetCurrentTool(ToolType::Brush);
                    }
                    m_canvasView->InvalidateCanvas();
                }
                break;
            }
        }
        Invalidate();
        return;
    }
}

int Workspace::HitTestMenuItem(const Point& pos) const {
    if (pos.y >= Theme::GetSize(MenuBarHeight)) return -1;
    for (size_t i = 0; i < m_menuItemRects.size(); ++i) {
        if (m_menuItemRects[i].Contains(pos)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int Workspace::HitTestToolbarButton(const Point& pos) const {
    if (pos.y < Theme::GetSize(MenuBarHeight) || pos.y >= Theme::GetSize(MenuBarHeight) + Theme::GetSize(ToolbarHeight)) return -1;
    int btnSize = Theme::GetSize(28);
    int spacing = Theme::GetSize(4);
    int startX = Theme::GetSize(8);
    int index = (pos.x - startX) / (btnSize + spacing);
    int btnX = startX + index * (btnSize + spacing);
    if (pos.x >= btnX && pos.x < btnX + btnSize && index >= 0 && index < 3) return index;
    return -1;
}

void Workspace::DrawToolbarButtons(HDC hdc) {
    int btnSize = Theme::GetSize(28);
    int spacing = Theme::GetSize(4);
    int startX = Theme::GetSize(8);
    int y = Theme::GetSize(MenuBarHeight) + (Theme::GetSize(ToolbarHeight) - btnSize) / 2;
    
    const wchar_t* labels[3] = { L"\uE7A7", L"\uE7A8", L"\uE76D" };
    HFONT font = CreateFontW(Theme::GetFontSize(16), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe MDL2 Assets");
    HFONT old = static_cast<HFONT>(SelectObject(hdc, font));
    SetTextColor(hdc, Theme::TextPrimary);
    SetBkMode(hdc, TRANSPARENT);
    
    for (int i = 0; i < 3; ++i) {
        int bx = startX + i * (btnSize + spacing);
        HBRUSH btnBrush = Theme::CachedBrush(Theme::ButtonDefault);
        RECT btnRc = { bx, y, bx + btnSize, y + btnSize };
        FillRect(hdc, &btnRc, btnBrush);
        
        HPEN border = CreatePen(PS_SOLID, 1, Theme::BorderLight);
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, border));
        HBRUSH nullBr = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
        HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
        Rectangle(hdc, bx, y, bx + btnSize, y + btnSize);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBr);
        DeleteObject(border);
        
        RECT textRc = { bx, y, bx + btnSize, y + btnSize };
        DrawTextW(hdc, labels[i], -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
    }
    
    SelectObject(hdc, old);
    // font cached
    
    // Current color square
    int colorX = startX + 3 * (btnSize + spacing) + Theme::GetSize(8);
    int colorSize = btnSize;
    Color c = m_canvasView ? m_canvasView->GetBrushColor() : Color(0,0,0,255);
    HBRUSH colorBrush = CreateSolidBrush(RGB(c.r, c.g, c.b));
    RECT colorRc = { colorX, y, colorX + colorSize, y + colorSize };
    FillRect(hdc, &colorRc, colorBrush);
    DeleteObject(colorBrush);
    
    HPEN border = CreatePen(PS_SOLID, 1, Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, border));
    HBRUSH nullBr = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
    Rectangle(hdc, colorX, y, colorX + colorSize, y + colorSize);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(border);
}

void Workspace::ShowMenuDropdown(int menuIndex, int screenX, int screenY, int titleWidth) {
    if (!m_dropdownWindow) {
        m_dropdownWindow = MakeScope<MenuDropdownWindow>();
    }
    
    const auto& menu = m_menus[menuIndex];
    int itemHeight = Theme::GetSize(24);
    int dropdownWidth = titleWidth + Theme::GetSize(12);
    
    // Measure widest item
    HDC hdc = GetDC(m_hwnd);
    HFONT font = CreateFontW(Theme::GetFontSize(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    HFONT old = static_cast<HFONT>(SelectObject(hdc, font));
    for (const auto& item : menu.items) {
        SIZE ts;
        GetTextExtentPoint32W(hdc, item.c_str(), static_cast<int>(item.length()), &ts);
        if (ts.cx + Theme::GetSize(24) > dropdownWidth) {
            dropdownWidth = ts.cx + Theme::GetSize(24);
        }
    }
    SelectObject(hdc, old);
    // font cached
    ReleaseDC(m_hwnd, hdc);
    
    int dropdownHeight = static_cast<int>(menu.items.size()) * itemHeight + 4;
    
    if (!m_dropdownWindow->GetHandle()) {
        m_dropdownWindow->Create(L"", Rect(screenX, screenY, screenX + dropdownWidth, screenY + dropdownHeight), nullptr);
    } else {
        m_dropdownWindow->SetBounds(Rect(screenX, screenY, screenX + dropdownWidth, screenY + dropdownHeight));
        m_dropdownWindow->SetVisible(true);
    }
    
    m_dropdownWindow->SetItems(menu.items, menuIndex);
    m_dropdownWindow->SetCallback([this](int menuIdx, int itemIdx) {
        m_openMenuIndex = -1;
        HideMenuDropdown();
        Invalidate();
        if (itemIdx >= 0) {
            OnMenuItemClicked(menuIdx, itemIdx);
        }
    });
    SetWindowPos(m_dropdownWindow->GetHandle(), HWND_TOPMOST, screenX, screenY, dropdownWidth, dropdownHeight, SWP_SHOWWINDOW);
    SetCapture(m_dropdownWindow->GetHandle());
}

void Workspace::HideMenuDropdown() {
    if (m_dropdownWindow) {
        ReleaseCapture();
        m_dropdownWindow->SetVisible(false);
    }
}

void Workspace::MenuDropdownWindow::OnPaint(HDC hdc, const Rect& clip) {
    Rect client = GetClientBounds();
    
    // Background
    HBRUSH bg = Theme::CachedBrush(Theme::PanelBackground);
    RECT rc = client.ToWin32Rect();
    FillRect(hdc, &rc, bg);
    
    // Border
    HPEN pen = CreatePen(PS_SOLID, 1, Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    HBRUSH nullBr = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
    Rectangle(hdc, 0, 0, client.Width(), client.Height());
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(pen);
    
    // Items
    int itemHeight = Theme::GetSize(24);
    HFONT itemFont = CreateFontW(Theme::GetFontSize(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, itemFont));
    SetBkMode(hdc, TRANSPARENT);
    
    for (size_t j = 0; j < m_items.size(); ++j) {
        int iy = static_cast<int>(j) * itemHeight + 2;
        
        if (static_cast<int>(j) == m_hoverIndex) {
            HBRUSH hoverBrush = Theme::CachedBrush(Theme::HighlightBlue);
            RECT hoverRc = { 2, iy, client.Width() - 2, iy + itemHeight };
            FillRect(hdc, &hoverRc, hoverBrush);
            SetTextColor(hdc, RGB(0xFF, 0xFF, 0xFF));
        } else {
            SetTextColor(hdc, Theme::TextPrimary);
        }
        
        RECT itemRc = { Theme::GetSize(8), iy, client.Width() - Theme::GetSize(8), iy + itemHeight };
        DrawTextW(hdc, m_items[j].c_str(), -1, &itemRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    }
    
    SelectObject(hdc, oldFont);
    DeleteObject(itemFont);
}

LRESULT Workspace::MenuDropdownWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CAPTURECHANGED:
            if (m_callback) {
                m_callback(m_menuIndex, -1);
            }
            return 0;
    }
    return Window::HandleMessage(msg, wParam, lParam);
}

void Workspace::MenuDropdownWindow::OnMouseDown(const Point& pos, MouseButton button) {
    if (button == MouseButton::Left) {
        Rect client = GetClientBounds();
        // Click outside window bounds closes menu
        if (pos.x < 0 || pos.x >= client.Width() || pos.y < 0 || pos.y >= client.Height()) {
            if (m_callback) {
                m_callback(m_menuIndex, -1);
            }
            ReleaseCapture();
            return;
        }
        int itemHeight = Theme::GetSize(24);
        int index = (pos.y - 2) / itemHeight;
        if (index >= 0 && index < static_cast<int>(m_items.size())) {
            if (m_callback) {
                m_callback(m_menuIndex, index);
            }
            ReleaseCapture();
        } else {
            // Click inside window but outside items: close menu
            if (m_callback) {
                m_callback(m_menuIndex, -1);
            }
            ReleaseCapture();
        }
    }
}

void Workspace::MenuDropdownWindow::OnMouseMove(const Point& pos) {
    Rect client = GetClientBounds();
    if (pos.x < 0 || pos.x >= client.Width() || pos.y < 0 || pos.y >= client.Height()) {
        SetHoverIndex(-1);
        return;
    }
    int itemHeight = Theme::GetSize(24);
    int index = (pos.y - 2) / itemHeight;
    if (index >= 0 && index < static_cast<int>(m_items.size())) {
        SetHoverIndex(index);
    } else {
        SetHoverIndex(-1);
    }
}

void Workspace::MenuDropdownWindow::OnMouseLeave() {
    SetHoverIndex(-1);
}

void Workspace::DrawStatusBar(HDC hdc) {
    Rect client = GetClientBounds();
    int sbTop = client.Height() - Theme::GetSize(StatusBarHeight);
    
    // Background
    HBRUSH bg = Theme::CachedBrush(Theme::BackgroundDark);
    RECT sbRc = { 0, sbTop, client.Width(), client.Height() };
    FillRect(hdc, &sbRc, bg);
    
    // Top border
    HPEN pen = Theme::Pen(Theme::BorderDark);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    MoveToEx(hdc, 0, sbTop, nullptr);
    LineTo(hdc, client.Width(), sbTop);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
    
    // Left: tool info
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::TextSecondary);
    HFONT font = Theme::GetCachedFont(Theme::FontID::Small);
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
    
    std::wostringstream leftOss;
    leftOss << L"笔刷: " << static_cast<int>(Render::BrushEngine::GetInstance().GetSize()) 
            << L"px | 不透明度: " << static_cast<int>(Render::BrushEngine::GetInstance().GetOpacity() * 100) << L"%";
    RECT leftRc = { Theme::GetSize(8), sbTop, client.Width() / 3, client.Height() };
    DrawTextW(hdc, leftOss.str().c_str(), -1, &leftRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    
    // Center: canvas info
    std::wostringstream centerOss;
    if (m_project) {
        const auto& c = m_project->GetCanvas();
        centerOss << c.widthPx << L" x " << c.heightPx << L" px | " << c.dpi << L" dpi";
    } else {
        centerOss << L"未命名画布";
    }
    RECT centerRc = { client.Width() / 3, sbTop, client.Width() * 2 / 3, client.Height() };
    DrawTextW(hdc, centerOss.str().c_str(), -1, &centerRc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
    
    // Center-right: save status (auto-clear after 3s)
    if (!m_saveStatusText.empty()) {
        if (GetTickCount64() - m_saveStatusTime > 3000) {
            m_saveStatusText.clear();
        } else {
            SetTextColor(hdc, Theme::HighlightBlue);
            RECT statusRc = { client.Width() / 2, sbTop, client.Width() * 2 / 3, client.Height() };
            DrawTextW(hdc, m_saveStatusText.c_str(), -1, &statusRc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
            SetTextColor(hdc, Theme::TextSecondary);
        }
    }
    
    // Right: zoom
    std::wostringstream rightOss;
    if (m_canvasView) {
        rightOss << static_cast<int>(m_canvasView->GetZoom() * 100) << L"%";
    }
    RECT rightRc = { client.Width() * 2 / 3, sbTop, client.Width() - Theme::GetSize(8), client.Height() };
    DrawTextW(hdc, rightOss.str().c_str(), -1, &rightRc, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
    
    SelectObject(hdc, oldFont);
    // font cached
}

void Workspace::OnKeyDown(uint32_t keyCode) {
    // Close open menu on ESC
    if (keyCode == VK_ESCAPE && m_openMenuIndex >= 0) {
        m_openMenuIndex = -1;
        HideMenuDropdown();
        Invalidate();
        return;
    }
    
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    
    switch (keyCode) {
        case 'B':
            if (m_canvasView) {
                m_canvasView->SetCurrentTool(ToolType::Brush);
                m_canvasView->InvalidateCanvas();
            }
            break;
        case 'E':
            if (m_canvasView) {
                m_canvasView->SetCurrentTool(ToolType::Eraser);
                m_canvasView->InvalidateCanvas();
            }
            break;
        case VK_OEM_4: // '[' key
            Render::BrushEngine::GetInstance().SetSize(
                std::max(1.0f, Render::BrushEngine::GetInstance().GetSize() - 2.0f));
            if (m_brushPanel) m_brushPanel->Refresh();
            RefreshStatusBar();
            break;
        case VK_OEM_6: // ']' key
            Render::BrushEngine::GetInstance().SetSize(
                std::min(5000.0f, Render::BrushEngine::GetInstance().GetSize() + 2.0f));
            if (m_brushPanel) m_brushPanel->Refresh();
            RefreshStatusBar();
            break;
        case 'Z':
            if (ctrl) {
                if (shift) {
                    HistoryManager::GetInstance().Redo();
                } else {
                    HistoryManager::GetInstance().Undo();
                }
                if (m_canvasView) {
                    m_canvasView->InvalidateCanvas();
                }
            }
            break;
        case 'S':
            if (ctrl) {
                DoSave(shift || m_currentFilePath.empty());
            }
            break;
        case 'O':
            if (ctrl) {
                wchar_t filePath[MAX_PATH] = {0};
                OPENFILENAMEW ofn = {};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = m_hwnd;
                ofn.lpstrFilter = L"VividPic Project (*.vvp)\0*.vvp\0";
                ofn.lpstrFile = filePath;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_FILEMUSTEXIST;
                if (GetOpenFileNameW(&ofn)) {
                    auto loadedProject = ProjectSerializer::LoadProject(filePath, m_canvasView->GetLayerManager());
                    if (loadedProject && m_canvasView) {
                        m_project = loadedProject;
                        m_currentFilePath = filePath;
                        const auto& canvas = m_project->GetCanvas();
                        Color bgColor = Color::FromHex(0xFFFFFF);
                        m_canvasView->InitializeCanvas(canvas.widthPx, canvas.heightPx, bgColor, canvas.transparent, canvas.initialLayerType, false);
                        if (m_navigatorPanel) {
                            m_navigatorPanel->SetCanvasSize(canvas.widthPx, canvas.heightPx);
                        }
                        if (m_layersPanel) {
                            m_layersPanel->SetLayerManager(m_canvasView->GetLayerManager());
                            m_layersPanel->Refresh();
                        }
                        if (m_canvasView) {
                            m_canvasView->InvalidateCanvas();
                        }
                    }
                }
            }
            break;
        case VK_HOME:
            if (m_canvasView) {
                m_canvasView->ResetView();
            }
            break;
        case '0':
            if (ctrl) {
                if (m_canvasView) {
                    m_canvasView->FitToWindow();
                }
            }
            break;
        case '1':
            if (ctrl) {
                if (m_canvasView) {
                    m_canvasView->SetZoom(1.0f);
                    m_canvasView->InvalidateCanvas();
                }
            }
            break;
        case VK_DELETE:
            if (m_canvasView) {
                auto* lm = m_canvasView->GetLayerManager();
                if (lm && lm->GetLayerCount() > 1) {
                    lm->DeleteLayer(lm->GetActiveLayerIndex());
                    m_canvasView->InvalidateCanvas();
                    if (m_layersPanel) m_layersPanel->Refresh();
                }
            }
            break;
    }
}

void Workspace::SyncPanels() {
    if (m_navigatorPanel && m_canvasView) {
        auto pan = m_canvasView->GetPan();
        m_navigatorPanel->SetCanvasViewTransform(m_canvasView->GetZoom(), pan.x, pan.y, m_canvasView->GetRotation());
    }
    if (m_layersPanel) {
        m_layersPanel->Refresh();
    }
}

// -------------------------------------------------------------------------
// Splitter
// -------------------------------------------------------------------------
Workspace::SplitterSide Workspace::HitTestSplitter(const Point& pos) const {
    int contentTop = Theme::GetSize(MenuBarHeight) + Theme::GetSize(ToolbarHeight);
    int contentBottom = GetClientBounds().Height() - Theme::GetSize(StatusBarHeight);
    if (pos.y < contentTop || pos.y >= contentBottom) return SplitterSide::None;
    
    int half = Theme::GetSize(3);
    int leftX = Theme::GetSize(ToolBarWidth) + m_leftPanelWidth;
    if (pos.x >= leftX - half && pos.x < leftX + half) return SplitterSide::Left;
    
    int rightX = GetClientBounds().Width() - m_rightPanelWidth;
    if (pos.x >= rightX - half && pos.x < rightX + half) return SplitterSide::Right;
    
    return SplitterSide::None;
}

void Workspace::DrawSplitters(HDC hdc) {
    Rect client = GetClientBounds();
    int contentTop = Theme::GetSize(MenuBarHeight) + Theme::GetSize(ToolbarHeight);
    int contentBottom = client.Height() - Theme::GetSize(StatusBarHeight);
    
    // Left splitter
    int leftX = Theme::GetSize(ToolBarWidth) + m_leftPanelWidth;
    uint32_t leftColor = (m_splitterSide == SplitterSide::Left) ? Theme::HighlightBlue : Theme::BorderDark;
    HPEN leftPen = Theme::Pen(leftColor, (m_splitterSide == SplitterSide::Left) ? 2 : 1);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, leftPen));
    MoveToEx(hdc, leftX, contentTop, nullptr);
    LineTo(hdc, leftX, contentBottom);
    SelectObject(hdc, oldPen);
    DeleteObject(leftPen);
    
    // Right splitter
    int rightX = client.Width() - m_rightPanelWidth;
    uint32_t rightColor = (m_splitterSide == SplitterSide::Right) ? Theme::HighlightBlue : Theme::BorderDark;
    HPEN rightPen = Theme::Pen(rightColor, (m_splitterSide == SplitterSide::Right) ? 2 : 1);
    oldPen = static_cast<HPEN>(SelectObject(hdc, rightPen));
    MoveToEx(hdc, rightX, contentTop, nullptr);
    LineTo(hdc, rightX, contentBottom);
    SelectObject(hdc, oldPen);
    DeleteObject(rightPen);
}

void Workspace::OnMouseMove(const Point& pos) {
    if (m_splitterDragging) {
        int delta = pos.x - m_splitterDragStartX;
        if (m_splitterSide == SplitterSide::Left) {
            m_leftPanelWidth = m_splitterDragStartWidth + delta;
            int minW = Theme::GetSize(MinPanelWidth);
            int maxW = Theme::GetSize(MaxPanelWidth);
            if (m_leftPanelWidth < minW) m_leftPanelWidth = minW;
            if (m_leftPanelWidth > maxW) m_leftPanelWidth = maxW;
        } else if (m_splitterSide == SplitterSide::Right) {
            m_rightPanelWidth = m_splitterDragStartWidth - delta;
            int minW = Theme::GetSize(MinPanelWidth);
            int maxW = Theme::GetSize(MaxPanelWidth);
            if (m_rightPanelWidth < minW) m_rightPanelWidth = minW;
            if (m_rightPanelWidth > maxW) m_rightPanelWidth = maxW;
        }
        LayoutPanels();
        return;
    }
    
    SplitterSide side = HitTestSplitter(pos);
    if (side != m_splitterSide) {
        m_splitterSide = side;
        Invalidate();
    }
}

void Workspace::OnMouseUp(const Point& pos, MouseButton button) {
    (void)pos;
    if (button == MouseButton::Left && m_splitterDragging) {
        m_splitterDragging = false;
        m_splitterSide = SplitterSide::None;
        ReleaseCapture();
        Invalidate();
    }
}

// -------------------------------------------------------------------------
// Drop files
// -------------------------------------------------------------------------
void Workspace::OpenProjectFile(const wchar_t* filePath) {
    if (!m_canvasView) return;
    auto& lm = LayerManager::GetInstance();
    auto loadedProject = ProjectSerializer::LoadProject(filePath, &lm);
    if (loadedProject) {
        m_project = loadedProject;
        m_currentFilePath = filePath;
        const auto& canvas = m_project->GetCanvas();
        Color bgColor = Color::FromHex(0xFFFFFF);
        m_canvasView->InitializeCanvas(canvas.widthPx, canvas.heightPx, bgColor, canvas.transparent, canvas.initialLayerType, false);
        if (m_navigatorPanel) {
            m_navigatorPanel->SetCanvasSize(canvas.widthPx, canvas.heightPx);
        }
        if (m_layersPanel) {
            m_layersPanel->SetLayerManager(m_canvasView->GetLayerManager());
        }
        m_canvasView->InvalidateCanvas();
        ShowToast(L"打开成功");
    } else {
        ShowToast(L"无法打开项目文件");
    }
}

LRESULT Workspace::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DROPFILES: {
            HDROP hDrop = reinterpret_cast<HDROP>(wParam);
            UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
            for (UINT i = 0; i < fileCount; ++i) {
                wchar_t filePath[MAX_PATH];
                if (DragQueryFileW(hDrop, i, filePath, MAX_PATH)) {
                    size_t len = wcslen(filePath);
                    if (len > 4 && _wcsicmp(filePath + len - 4, L".vvp") == 0) {
                        OpenProjectFile(filePath);
                        break;
                    }
                }
            }
            DragFinish(hDrop);
            return 0;
        }
        case WM_SETCURSOR: {
            if (LOWORD(lParam) == HTCLIENT) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(m_hwnd, &pt);
                SplitterSide side = HitTestSplitter(Point(pt.x, pt.y));
                if (side != SplitterSide::None || m_splitterDragging) {
                    SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
                    return TRUE;
                }
            }
            break;
        }
    }
    return Window::HandleMessage(msg, wParam, lParam);
}

} // namespace UI
} // namespace VividPic
