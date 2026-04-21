#include "UI/Screens/Workspace.h"
#include "App/Application.h"
#include "Render/BrushEngine.h"
#include "Render/BrushPresetManager.h"
#include "UI/Core/Theme.h"
#include "Core/History.h"
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
        Color bgColor = Color::FromHex(0xFFFFFF);
        m_canvasView->InitializeCanvas(canvas.widthPx, canvas.heightPx, bgColor);
        
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
    
    // Create CanvasView
    m_canvasView = MakeScope<CanvasView>();
    int canvasLeft = Theme::GetSize(ToolBarWidth) + Theme::GetSize(LeftPanelWidth);
    int canvasRight = 1400 - Theme::GetSize(RightPanelWidth);
    int contentTop = Theme::GetSize(MenuBarHeight) + Theme::GetSize(ToolbarHeight);
    int contentBottom = 900 - Theme::GetSize(StatusBarHeight);
    Rect canvasRect(canvasLeft, contentTop, canvasRight, contentBottom);
    m_canvasView->Create(L"", canvasRect, this);
    
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
    });
    m_brushPanel->SetOnOpacityChanged([this](float opacity) {
        Render::BrushEngine::GetInstance().SetOpacity(opacity);
    });
    m_brushPanel->SetOnFlowChanged([this](float flow) {
        Render::BrushEngine::GetInstance().SetFlow(flow);
    });
    m_brushPanel->SetOnSpacingChanged([this](float spacing) {
        Render::BrushEngine::GetInstance().SetSpacing(spacing);
    });
    m_brushPanel->SetOnTipTypeChanged([this](Render::BrushTipType type) {
        Render::BrushEngine::GetInstance().SetTipType(type);
    });
    m_brushPanel->SetOnPresetSelected([this](int index) {
        Render::BrushPresetManager::GetInstance().ApplyPreset(index);
        // Sync panel UI back from engine
        auto& engine = Render::BrushEngine::GetInstance();
        m_brushPanel->SetBrushSize(engine.GetSize());
        m_brushPanel->SetBrushOpacity(engine.GetOpacity());
        m_brushPanel->SetBrushFlow(engine.GetFlow());
        m_brushPanel->SetBrushSpacing(engine.GetSpacing());
        m_brushPanel->SetTipType(engine.GetTipType());
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
    });
    m_navigatorPanel->SetOnPanChanged([this](float x, float y) {
        if (m_canvasView) {
            m_canvasView->SetPan(x, y);
            m_canvasView->InvalidateCanvas();
        }
    });
    
    if (m_project) {
        const auto& canvas = m_project->GetCanvas();
        Color bgColor = Color::FromHex(0xFFFFFF);
        m_canvasView->InitializeCanvas(canvas.widthPx, canvas.heightPx, bgColor);
        
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
    DrawMenuDropdown(hdc);
    DrawStatusBar(hdc);
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
    m_menus.push_back({ L"文件(F)", { L"新建窗口", L"保存", L"导出", L"退出" } });
    m_menus.push_back({ L"编辑(E)", { L"撤销", L"重做", L"首选项" } });
    m_menus.push_back({ L"图层(L)", { L"新建图层", L"删除图层", L"合并" } });
    m_menus.push_back({ L"滤镜(R)", { L"高斯模糊", L"锐化" } });
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
    } else if (menuIndex == 0 && itemIndex == 1) { // File > Save
        MessageBoxW(m_hwnd, L"保存功能将在 M6 实现", L"保存", MB_OK);
    } else if (menuIndex == 1 && itemIndex == 0) { // Edit > Undo
        HistoryManager::GetInstance().Undo();
        if (m_canvasView) m_canvasView->InvalidateCanvas();
    } else if (menuIndex == 1 && itemIndex == 1) { // Edit > Redo
        HistoryManager::GetInstance().Redo();
        if (m_canvasView) m_canvasView->InvalidateCanvas();
    } else {
        std::wstring msg = L"菜单: " + m_menus[menuIndex].name + L" > " + m_menus[menuIndex].items[itemIndex];
        MessageBoxW(m_hwnd, msg.c_str(), L"功能占位符", MB_OK);
    }
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
    
    // Check menu dropdown
    if (m_openMenuIndex >= 0) {
        int x = Theme::GetSize(8);
        for (int i = 0; i < m_openMenuIndex; ++i) {
            SIZE ts;
            HDC hdc = GetDC(m_hwnd);
            HFONT font = CreateFontW(Theme::GetFontSize(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
            HFONT old = static_cast<HFONT>(SelectObject(hdc, font));
            GetTextExtentPoint32W(hdc, m_menus[i].name.c_str(), static_cast<int>(m_menus[i].name.length()), &ts);
            SelectObject(hdc, old);
            DeleteObject(font);
            ReleaseDC(m_hwnd, hdc);
            x += ts.cx + Theme::GetSize(16);
        }
        SIZE ts;
        HDC hdc = GetDC(m_hwnd);
        HFONT font = CreateFontW(Theme::GetFontSize(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
        HFONT old = static_cast<HFONT>(SelectObject(hdc, font));
        GetTextExtentPoint32W(hdc, m_menus[m_openMenuIndex].name.c_str(), static_cast<int>(m_menus[m_openMenuIndex].name.length()), &ts);
        SelectObject(hdc, old);
        DeleteObject(font);
        ReleaseDC(m_hwnd, hdc);
        
        int dropdownY = Theme::GetSize(MenuBarHeight);
        int itemHeight = Theme::GetSize(22);
        int dropdownWidth = ts.cx + Theme::GetSize(12);
        for (size_t j = 0; j < m_menus[m_openMenuIndex].items.size(); ++j) {
            int iy = dropdownY + static_cast<int>(j) * itemHeight;
            if (pos.x >= x && pos.x < x + dropdownWidth && pos.y >= iy && pos.y < iy + itemHeight) {
                OnMenuItemClicked(m_openMenuIndex, static_cast<int>(j));
                m_openMenuIndex = -1;
                Invalidate();
                return;
            }
        }
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
    
    const wchar_t* labels[3] = { L"↶", L"↷", L"B/E" };
    HFONT font = CreateFontW(Theme::GetFontSize(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
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

void Workspace::DrawMenuDropdown(HDC hdc) {
    if (m_openMenuIndex < 0 || m_openMenuIndex >= static_cast<int>(m_menus.size())) return;
    
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
    
    int dropdownY = Theme::GetSize(MenuBarHeight);
    int itemHeight = Theme::GetSize(22);
    int dropdownWidth = ts.cx + Theme::GetSize(12);
    int dropdownHeight = static_cast<int>(m_menus[m_openMenuIndex].items.size()) * itemHeight;
    
    // Background
    HBRUSH bg = Theme::SolidBrush(Theme::PanelBackground);
    RECT dropRc = { x, dropdownY, x + dropdownWidth, dropdownY + dropdownHeight };
    FillRect(hdc, &dropRc, bg);
    DeleteObject(bg);
    
    // Border
    HPEN pen = CreatePen(PS_SOLID, 1, Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    HBRUSH nullBr = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
    Rectangle(hdc, x, dropdownY, x + dropdownWidth, dropdownY + dropdownHeight);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(pen);
    
    // Items
    HFONT itemFont = CreateFontW(Theme::GetFontSize(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, itemFont));
    SetBkMode(hdc, TRANSPARENT);
    for (size_t j = 0; j < m_menus[m_openMenuIndex].items.size(); ++j) {
        int iy = dropdownY + static_cast<int>(j) * itemHeight;
        RECT itemRc = { x + Theme::GetSize(6), iy, x + dropdownWidth - Theme::GetSize(6), iy + itemHeight };
        SetTextColor(hdc, Theme::TextPrimary);
        DrawTextW(hdc, m_menus[m_openMenuIndex].items[j].c_str(), -1, &itemRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    }
    SelectObject(hdc, oldFont);
    DeleteObject(itemFont);
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
    switch (keyCode) {
        case 'B':
            // Brush tool
            break;
        case 'E':
            // Eraser tool
            break;
        case VK_OEM_4: // '[' key
            Render::BrushEngine::GetInstance().SetSize(
                std::max(1.0f, Render::BrushEngine::GetInstance().GetSize() - 2.0f));
            if (m_brushPanel) m_brushPanel->Refresh();
            break;
        case VK_OEM_6: // ']' key
            Render::BrushEngine::GetInstance().SetSize(
                std::min(5000.0f, Render::BrushEngine::GetInstance().GetSize() + 2.0f));
            if (m_brushPanel) m_brushPanel->Refresh();
            break;
        case 'Z':
            if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
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
            if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                MessageBoxW(m_hwnd, L"保存 (M6 实现)", L"文件", MB_OK);
            }
            break;
    }
}

void Workspace::SyncPanels() {
    if (m_navigatorPanel && m_canvasView) {
        auto pan = m_canvasView->GetPan();
        m_navigatorPanel->SetCanvasViewTransform(m_canvasView->GetZoom(), pan.x, pan.y);
    }
    if (m_layersPanel) {
        m_layersPanel->Refresh();
    }
}

} // namespace UI
} // namespace VividPic
