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
    Rect bounds(50, 50, static_cast<int>(1400 * Theme::Scale), static_cast<int>(900 * Theme::Scale));
    if (!Create(L"ViVidPic - Workspace", bounds, nullptr)) {
        return false;
    }
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
    
    // Create left tool bar
    m_toolBar = MakeScope<ToolBar>();
    Rect toolBarRect(0, Theme::GetSize(MenuBarHeight) + Theme::GetSize(ToolbarHeight), 
                     Theme::GetSize(ToolBarWidth), 900);
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
    int canvasLeft = Theme::GetSize(ToolBarWidth) + Theme::GetSize(LeftPanelWidth);
    int canvasRight = 1400 - Theme::GetSize(RightPanelWidth);
    int contentTop = Theme::GetSize(MenuBarHeight) + Theme::GetSize(ToolbarHeight);
    int contentBottom = 900 - Theme::GetSize(StatusBarHeight);
    Rect canvasRect(canvasLeft, contentTop, canvasRight, contentBottom);
    m_canvasView->Create(L"", canvasRect, this);
    m_canvasView->SetOnToolChanged([this](ToolType tool) {
        if (m_toolBar) m_toolBar->SetCurrentTool(tool);
    });
    
    // Create left panels
    int leftPanelX = Theme::GetSize(ToolBarWidth);
    m_colorsPanel = MakeScope<ColorsPanel>();
    Rect colorsRect(leftPanelX, contentTop, leftPanelX + Theme::GetSize(LeftPanelWidth), 
                    contentTop + Theme::GetSize(260));
    m_colorsPanel->Create(L"", colorsRect, this);
    m_colorsPanel->SetOnColorChanged([this](const Color& color) {
        if (m_canvasView) {
            m_canvasView->SetBrushColor(color);
        }
    });
    
    m_brushPanel = MakeScope<BrushPanel>();
    Rect brushRect(leftPanelX, contentTop + Theme::GetSize(260), leftPanelX + Theme::GetSize(LeftPanelWidth),
                   contentTop + Theme::GetSize(260) + Theme::GetSize(240));
    m_brushPanel->Create(L"", brushRect, this);
    m_brushPanel->SetOnSizeChanged([this](float size) {
        Render::BrushEngine::GetInstance().SetSize(size);
        RefreshStatusBar();
    });
    m_brushPanel->SetOnOpacityChanged([this](float opacity) {
        Render::BrushEngine::GetInstance().SetOpacity(opacity);
        RefreshStatusBar();
    });
    m_brushPanel->SetOnFlowChanged([this](float flow) {
        Render::BrushEngine::GetInstance().SetFlow(flow);
        RefreshStatusBar();
    });
    m_brushPanel->SetOnSpacingChanged([this](float spacing) {
        Render::BrushEngine::GetInstance().SetSpacing(spacing);
        RefreshStatusBar();
    });
    m_brushPanel->SetOnWetMixChanged([this](float wetMix) {
        Render::BrushEngine::GetInstance().SetWetMix(wetMix);
        RefreshStatusBar();
    });
    m_brushPanel->SetOnTipTypeChanged([this](Render::BrushTipType type) {
        Render::BrushEngine::GetInstance().SetTipType(type);
        RefreshStatusBar();
    });
    m_brushPanel->SetOnPresetSelected([this](int index) {
        Render::BrushPresetManager::GetInstance().ApplyPreset(index);
        // Sync panel UI back from engine
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
    int rightPanelX = 1400 - Theme::GetSize(RightPanelWidth);
    m_layersPanel = MakeScope<LayersPanel>();
    Rect layersRect(rightPanelX, contentTop, 
                    rightPanelX + Theme::GetSize(RightPanelWidth), contentTop + Theme::GetSize(300));
    m_layersPanel->Create(L"", layersRect, this);
    
    m_navigatorPanel = MakeScope<NavigatorPanel>();
    Rect navRect(rightPanelX, contentTop + Theme::GetSize(300), 
                 rightPanelX + Theme::GetSize(RightPanelWidth), contentTop + Theme::GetSize(300) + Theme::GetSize(180));
    m_navigatorPanel->Create(L"", navRect, this);
    
    m_brushSizePanel = MakeScope<BrushSizePanel>();
    Rect bsRect(rightPanelX, contentTop + Theme::GetSize(300) + Theme::GetSize(180),
                rightPanelX + Theme::GetSize(RightPanelWidth), contentBottom);
    m_brushSizePanel->Create(L"", bsRect, this);
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
    
    return true;
}

void Workspace::OnSize(const Size& newSize) {
    LayoutPanels();
}

void Workspace::LayoutPanels() {
    Rect client = GetClientBounds();
    int rightPanelLeft = client.Width() - Theme::GetSize(RightPanelWidth);
    int contentTop = Theme::GetSize(MenuBarHeight) + Theme::GetSize(ToolbarHeight);
    int contentBottom = client.Height() - Theme::GetSize(StatusBarHeight);
    int contentHeight = contentBottom - contentTop;
    int leftPanelX = Theme::GetSize(ToolBarWidth);
    int canvasLeft = leftPanelX + Theme::GetSize(LeftPanelWidth);
    
    if (m_toolBar) {
        Rect tbRect(0, contentTop, Theme::GetSize(ToolBarWidth), contentBottom);
        m_toolBar->SetBounds(tbRect);
    }
    
    if (m_canvasView) {
        Rect canvasRect(canvasLeft, contentTop, rightPanelLeft, contentBottom);
        m_canvasView->SetBounds(canvasRect);
    }
    
    if (m_colorsPanel) {
        Rect colorsRect(leftPanelX, contentTop, leftPanelX + Theme::GetSize(LeftPanelWidth), contentTop + Theme::GetSize(260));
        m_colorsPanel->SetBounds(colorsRect);
    }
    
    if (m_brushPanel) {
        int brushBottom = contentTop + contentHeight * 3 / 5;
        if (brushBottom < contentTop + Theme::GetSize(300)) brushBottom = contentTop + Theme::GetSize(300);
        Rect brushRect(leftPanelX, contentTop + Theme::GetSize(260), leftPanelX + Theme::GetSize(LeftPanelWidth), brushBottom);
        m_brushPanel->SetBounds(brushRect);
    }
    
    if (m_layersPanel) {
        Rect layersRect(rightPanelLeft, contentTop, client.Width(), contentTop + contentHeight * 3 / 5);
        m_layersPanel->SetBounds(layersRect);
    }
    
    if (m_navigatorPanel) {
        int navTop = contentTop + contentHeight * 3 / 5;
        int navBottom = navTop + contentHeight * 1 / 5;
        if (navBottom > contentBottom - Theme::GetSize(100)) navBottom = contentBottom - Theme::GetSize(100);
        Rect navRect(rightPanelLeft, navTop, client.Width(), navBottom);
        m_navigatorPanel->SetBounds(navRect);
    }
    
    if (m_brushSizePanel) {
        int bsTop = contentTop + contentHeight * 3 / 5 + contentHeight * 1 / 5;
        if (bsTop > contentBottom - Theme::GetSize(80)) bsTop = contentBottom - Theme::GetSize(80);
        Rect bsRect(rightPanelLeft, bsTop, client.Width(), contentBottom);
        m_brushSizePanel->SetBounds(bsRect);
    }
}

void Workspace::OnPaint(HDC hdc, const Rect& clip) {
    Rect client = GetClientBounds();
    
    HBRUSH bgBrush = Theme::SolidBrush(Theme::CanvasOuter);
    RECT rc = client.ToWin32Rect();
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);
    
    DrawMenuBar(hdc);
    DrawToolbar(hdc);
    DrawStatusBar(hdc);
    
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
            DeleteObject(font);
            ReleaseDC(m_hwnd, memdc);
            
            POINT pt = { x, Theme::GetSize(MenuBarHeight) };
            ClientToScreen(m_hwnd, &pt);
            ShowMenuDropdown(m_openMenuIndex, pt.x, pt.y, ts.cx + Theme::GetSize(12));
        }
    } else if (m_openMenuIndex < 0 && m_dropdownWindow && m_dropdownWindow->IsVisible()) {
        HideMenuDropdown();
    }
}

void Workspace::DrawMenuBar(HDC hdc) {
    Rect client = GetClientBounds();
    
    HBRUSH brush = Theme::SolidBrush(Theme::BackgroundDark);
    RECT menuRc = { 0, 0, client.Width(), Theme::GetSize(MenuBarHeight) };
    FillRect(hdc, &menuRc, brush);
    DeleteObject(brush);
    
    SetBkMode(hdc, TRANSPARENT);
    HFONT font = CreateFontW(Theme::GetFontSize(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
    
    int x = 8;
    for (const auto& menu : m_menus) {
        SetTextColor(hdc, Theme::TextPrimary);
        SIZE textSize;
        GetTextExtentPoint32W(hdc, menu.name.c_str(), static_cast<int>(menu.name.length()), &textSize);
        
        RECT textRect = { x, 0, x + textSize.cx + 12, Theme::GetSize(MenuBarHeight) };
        DrawTextW(hdc, menu.name.c_str(), -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
        x += textSize.cx + 16;
    }
    
    SelectObject(hdc, oldFont);
    DeleteObject(font);
    
    HPEN linePen = CreatePen(PS_SOLID, 1, Theme::BorderDark);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, linePen));
    MoveToEx(hdc, 0, Theme::GetSize(MenuBarHeight), nullptr);
    LineTo(hdc, client.Width(), Theme::GetSize(MenuBarHeight));
    SelectObject(hdc, oldPen);
    DeleteObject(linePen);
}

void Workspace::DrawToolbar(HDC hdc) {
    Rect client = GetClientBounds();
    
    HBRUSH brush = Theme::SolidBrush(Theme::PanelBackground);
    RECT toolbarRc = { 0, Theme::GetSize(MenuBarHeight), client.Width(), Theme::GetSize(MenuBarHeight) + Theme::GetSize(ToolbarHeight) };
    FillRect(hdc, &toolbarRc, brush);
    DeleteObject(brush);
    
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
                // success
            } else {
                MessageBoxW(m_hwnd, L"保存失败", L"保存", MB_OK | MB_ICONERROR);
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
            if (!ProjectSerializer::ExportPNG(filePath, m_canvasView->GetLayerManager())) {
                MessageBoxW(m_hwnd, L"导出失败", L"导出", MB_OK | MB_ICONERROR);
            }
        }
    } else if (menuIndex == 1 && itemIndex == 0) { // Edit > Undo
        HistoryManager::GetInstance().Undo();
        if (m_canvasView) m_canvasView->InvalidateCanvas();
    } else if (menuIndex == 1 && itemIndex == 1) { // Edit > Redo
        HistoryManager::GetInstance().Redo();
        if (m_canvasView) m_canvasView->InvalidateCanvas();
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

void Workspace::OnMouseDown(const Point& pos, MouseButton button) {
    if (button != MouseButton::Left) return;
    
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
    int x = Theme::GetSize(8);
    HDC hdc = GetDC(m_hwnd);
    HFONT font = CreateFontW(Theme::GetFontSize(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    HFONT old = static_cast<HFONT>(SelectObject(hdc, font));
    for (size_t i = 0; i < m_menus.size(); ++i) {
        SIZE ts;
        GetTextExtentPoint32W(hdc, m_menus[i].name.c_str(), static_cast<int>(m_menus[i].name.length()), &ts);
        int itemW = ts.cx + Theme::GetSize(16);
        if (pos.x >= x && pos.x < x + itemW) {
            SelectObject(hdc, old);
            DeleteObject(font);
            ReleaseDC(m_hwnd, hdc);
            return static_cast<int>(i);
        }
        x += itemW;
    }
    SelectObject(hdc, old);
    DeleteObject(font);
    ReleaseDC(m_hwnd, hdc);
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
        HBRUSH btnBrush = Theme::SolidBrush(Theme::ButtonDefault);
        RECT btnRc = { bx, y, bx + btnSize, y + btnSize };
        FillRect(hdc, &btnRc, btnBrush);
        DeleteObject(btnBrush);
        
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
    DeleteObject(font);
    
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
    DeleteObject(font);
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
    HBRUSH bg = Theme::SolidBrush(Theme::PanelBackground);
    RECT rc = client.ToWin32Rect();
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);
    
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
            HBRUSH hoverBrush = Theme::SolidBrush(Theme::HighlightBlue);
            RECT hoverRc = { 2, iy, client.Width() - 2, iy + itemHeight };
            FillRect(hdc, &hoverRc, hoverBrush);
            DeleteObject(hoverBrush);
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
    HBRUSH bg = Theme::SolidBrush(Theme::BackgroundDark);
    RECT sbRc = { 0, sbTop, client.Width(), client.Height() };
    FillRect(hdc, &sbRc, bg);
    DeleteObject(bg);
    
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
    HFONT font = Theme::GetFont(Theme::FontID::Small);
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
    DeleteObject(font);
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

} // namespace UI
} // namespace VividPic
