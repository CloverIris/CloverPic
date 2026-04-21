#include "UI/Screens/Workspace.h"
#include "App/Application.h"
#include "Render/BrushEngine.h"
#include "Render/BrushPresetManager.h"
#include "UI/Core/Theme.h"

namespace VividPic {
namespace UI {

Workspace::Workspace() = default;

bool Workspace::Initialize(Ref<Project> project) {
    m_project = project;
    Rect bounds(50, 50, 1400, 900);
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
    
    // Create CanvasView
    m_canvasView = MakeScope<CanvasView>();
    Rect canvasRect(LeftPanelWidth, MenuBarHeight + ToolbarHeight, 
                    1400 - RightPanelWidth, 900 - MenuBarHeight - ToolbarHeight);
    m_canvasView->Create(L"", canvasRect, this);
    
    // Create left panels
    m_colorsPanel = MakeScope<ColorsPanel>();
    Rect colorsRect(0, MenuBarHeight + ToolbarHeight, LeftPanelWidth, 
                    MenuBarHeight + ToolbarHeight + 260);
    m_colorsPanel->Create(L"", colorsRect, this);
    m_colorsPanel->SetOnColorChanged([this](const Color& color) {
        if (m_canvasView) {
            m_canvasView->SetBrushColor(color);
        }
    });
    
    m_brushPanel = MakeScope<BrushPanel>();
    Rect brushRect(0, MenuBarHeight + ToolbarHeight + 260, LeftPanelWidth,
                   MenuBarHeight + ToolbarHeight + 260 + 240);
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
    m_layersPanel = MakeScope<LayersPanel>();
    Rect layersRect(1400 - RightPanelWidth, MenuBarHeight + ToolbarHeight, 
                    1400, MenuBarHeight + ToolbarHeight + 300);
    m_layersPanel->Create(L"", layersRect, this);
    
    m_navigatorPanel = MakeScope<NavigatorPanel>();
    Rect navRect(1400 - RightPanelWidth, MenuBarHeight + ToolbarHeight + 300, 
                 1400, MenuBarHeight + ToolbarHeight + 300 + 220);
    m_navigatorPanel->Create(L"", navRect, this);
    
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
    int rightPanelLeft = client.Width() - RightPanelWidth;
    int contentTop = MenuBarHeight + ToolbarHeight;
    int contentHeight = client.Height() - contentTop;
    
    if (m_canvasView) {
        Rect canvasRect(LeftPanelWidth, contentTop, rightPanelLeft, client.Height());
        m_canvasView->SetBounds(canvasRect);
    }
    
    if (m_colorsPanel) {
        Rect colorsRect(0, contentTop, LeftPanelWidth, contentTop + 260);
        m_colorsPanel->SetBounds(colorsRect);
    }
    
    if (m_brushPanel) {
        Rect brushRect(0, contentTop + 260, LeftPanelWidth, contentTop + 500);
        m_brushPanel->SetBounds(brushRect);
    }
    
    if (m_layersPanel) {
        Rect layersRect(rightPanelLeft, contentTop, client.Width(), contentTop + contentHeight * 3 / 5);
        m_layersPanel->SetBounds(layersRect);
    }
    
    if (m_navigatorPanel) {
        Rect navRect(rightPanelLeft, contentTop + contentHeight * 3 / 5, 
                     client.Width(), client.Height());
        m_navigatorPanel->SetBounds(navRect);
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
}

void Workspace::DrawMenuBar(HDC hdc) {
    Rect client = GetClientBounds();
    
    HBRUSH brush = Theme::SolidBrush(Theme::BackgroundDark);
    RECT menuRc = { 0, 0, client.Width(), MenuBarHeight };
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
        
        RECT textRect = { x, 0, x + textSize.cx + 12, MenuBarHeight };
        DrawTextW(hdc, menu.name.c_str(), -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
        x += textSize.cx + 16;
    }
    
    SelectObject(hdc, oldFont);
    DeleteObject(font);
    
    HPEN linePen = CreatePen(PS_SOLID, 1, Theme::BorderDark);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, linePen));
    MoveToEx(hdc, 0, MenuBarHeight, nullptr);
    LineTo(hdc, client.Width(), MenuBarHeight);
    SelectObject(hdc, oldPen);
    DeleteObject(linePen);
}

void Workspace::DrawToolbar(HDC hdc) {
    Rect client = GetClientBounds();
    
    HBRUSH brush = Theme::SolidBrush(Theme::PanelBackground);
    RECT toolbarRc = { 0, MenuBarHeight, client.Width(), MenuBarHeight + ToolbarHeight };
    FillRect(hdc, &toolbarRc, brush);
    DeleteObject(brush);
    
    HPEN linePen = CreatePen(PS_SOLID, 1, Theme::BorderDark);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, linePen));
    MoveToEx(hdc, 0, MenuBarHeight + ToolbarHeight, nullptr);
    LineTo(hdc, client.Width(), MenuBarHeight + ToolbarHeight);
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
    }
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
                    MessageBoxW(m_hwnd, L"重做 (M6 实现)", L"编辑", MB_OK);
                } else {
                    MessageBoxW(m_hwnd, L"撤销 (M6 实现)", L"编辑", MB_OK);
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
