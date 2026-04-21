#include "UI/Panels/LayersPanel.h"
#include "UI/Core/Theme.h"
#include <algorithm>
#include <sstream>

namespace VividPic {
namespace UI {

LayersPanel::LayersPanel() = default;

void LayersPanel::Refresh() {
    Invalidate();
}

void LayersPanel::OnPaint(HDC hdc, const Rect& clip) {
    Rect client = GetClientBounds();
    
    // Background
    HBRUSH bgBrush = CreateSolidBrush(RGB(0x3C, 0x3C, 0x3C));
    RECT rc = client.ToWin32Rect();
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);
    
    if (!m_layerManager) return;
    
    SetBkMode(hdc, TRANSPARENT);
    
    // Title
    SetTextColor(hdc, RGB(0xA0, 0xA0, 0xA0));
    HFONT titleFont = CreateFontW(12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, titleFont));
    RECT titleRc = { 8, 4, client.Width() - 8, 20 };
    DrawTextW(hdc, L"图层", -1, &titleRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    SelectObject(hdc, oldFont);
    DeleteObject(titleFont);
    
    // Separator
    HPEN sepPen = CreatePen(PS_SOLID, 1, RGB(0x55, 0x55, 0x55));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, sepPen));
    MoveToEx(hdc, 8, 22, nullptr);
    LineTo(hdc, client.Width() - 8, 22);
    SelectObject(hdc, oldPen);
    DeleteObject(sepPen);
    
    // Layer items (top to bottom order, but display top layers at top)
    int y = 28;
    HFONT itemFont = CreateFontW(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    oldFont = static_cast<HFONT>(SelectObject(hdc, itemFont));
    
    size_t count = m_layerManager->GetLayerCount();
    for (int i = static_cast<int>(count) - 1; i >= 0; --i) {
        auto layer = m_layerManager->GetLayer(i);
        if (!layer) continue;
        
        bool isActive = (static_cast<size_t>(i) == m_layerManager->GetActiveLayerIndex());
        
        // Background
        if (isActive) {
            HBRUSH activeBrush = CreateSolidBrush(RGB(0x00, 0x78, 0xD7));
            RECT itemBg = { 4, y, client.Width() - 4, y + ItemHeight };
            FillRect(hdc, &itemBg, activeBrush);
            DeleteObject(activeBrush);
        }
        
        // Thumbnail placeholder (colored square)
        HBRUSH thumbBrush = CreateSolidBrush(RGB(0x50, 0x50, 0x50));
        RECT thumbRc = { 8, y + 4, 8 + ThumbSize, y + 4 + ThumbSize };
        FillRect(hdc, &thumbRc, thumbBrush);
        DeleteObject(thumbBrush);
        
        // Visibility icon (eye)
        SetTextColor(hdc, layer->IsVisible() ? RGB(0xE0, 0xE0, 0xE0) : RGB(0x60, 0x60, 0x60));
        RECT eyeRc = { client.Width() - 48, y + 4, client.Width() - 28, y + 20 };
        DrawTextW(hdc, layer->IsVisible() ? L"👁" : L"⊘", -1, &eyeRc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
        
        // Lock icon
        SetTextColor(hdc, layer->IsLocked() ? RGB(0xE0, 0xE0, 0xE0) : RGB(0x60, 0x60, 0x60));
        RECT lockRc = { client.Width() - 26, y + 4, client.Width() - 6, y + 20 };
        DrawTextW(hdc, layer->IsLocked() ? L"🔒" : L"○", -1, &lockRc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
        
        // Name
        SetTextColor(hdc, isActive ? RGB(0xFF, 0xFF, 0xFF) : RGB(0xE0, 0xE0, 0xE0));
        RECT nameRc = { 8 + ThumbSize + 8, y + 4, client.Width() - 52, y + 20 };
        DrawTextW(hdc, layer->GetName().c_str(), -1, &nameRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
        
        // Opacity text
        SetTextColor(hdc, RGB(0xA0, 0xA0, 0xA0));
        RECT opRc = { 8 + ThumbSize + 8, y + 24, client.Width() - 52, y + ItemHeight - 2 };
        std::wostringstream oss;
        oss << L"不透明度: " << static_cast<int>(layer->GetOpacity() * 100 / 255) << L"%";
        DrawTextW(hdc, oss.str().c_str(), -1, &opRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        
        y += ItemHeight + 2;
    }
    
    SelectObject(hdc, oldFont);
    DeleteObject(itemFont);
    
    // Bottom toolbar area
    int toolbarY = client.Height() - 32;
    HBRUSH toolbarBrush = CreateSolidBrush(RGB(0x2B, 0x2B, 0x2B));
    RECT toolbarRc = { 0, toolbarY, client.Width(), client.Height() };
    FillRect(hdc, &toolbarRc, toolbarBrush);
    DeleteObject(toolbarBrush);
    
    HPEN topLine = CreatePen(PS_SOLID, 1, RGB(0x55, 0x55, 0x55));
    oldPen = static_cast<HPEN>(SelectObject(hdc, topLine));
    MoveToEx(hdc, 0, toolbarY, nullptr);
    LineTo(hdc, client.Width(), toolbarY);
    SelectObject(hdc, oldPen);
    DeleteObject(topLine);
}

void LayersPanel::OnMouseDown(const Point& pos, MouseButton button) {
    if (button != MouseButton::Left || !m_layerManager) return;
    
    Rect client = GetClientBounds();
    int toolbarY = client.Height() - 32;
    
    if (pos.y >= toolbarY) {
        // Toolbar buttons
        int btnWidth = client.Width() / 4;
        int btnIndex = pos.x / btnWidth;
        
        switch (btnIndex) {
            case 0: { // New layer
                size_t count = m_layerManager->GetLayerCount();
                std::wostringstream name;
                name << L"图层 " << (count + 1);
                m_layerManager->AddLayer(name.str(), LayerType::Color);
                break;
            }
            case 1: { // Delete
                size_t active = m_layerManager->GetActiveLayerIndex();
                if (m_layerManager->GetLayerCount() > 1) {
                    m_layerManager->DeleteLayer(active);
                }
                break;
            }
            case 2: { // Move up (in stack = lower index)
                size_t active = m_layerManager->GetActiveLayerIndex();
                if (active > 0) {
                    m_layerManager->MoveLayer(active, active - 1);
                }
                break;
            }
            case 3: { // Move down
                size_t active = m_layerManager->GetActiveLayerIndex();
                if (active < m_layerManager->GetLayerCount() - 1) {
                    m_layerManager->MoveLayer(active, active + 1);
                }
                break;
            }
        }
        Invalidate();
        return;
    }
    
    // Layer selection
    int layerIdx = HitTestLayer(pos);
    if (layerIdx >= 0) {
        m_layerManager->SetActiveLayer(layerIdx);
        Invalidate();
    }
    
    // Check for visibility/lock button clicks
    int btn = HitTestButton(pos, layerIdx);
    if (btn == 0) {
        m_layerManager->ToggleLayerVisibility(layerIdx);
        Invalidate();
    } else if (btn == 1) {
        m_layerManager->ToggleLayerLock(layerIdx);
        Invalidate();
    }
}

int LayersPanel::HitTestLayer(const Point& pos) const {
    if (!m_layerManager) return -1;
    
    int y = 28;
    size_t count = m_layerManager->GetLayerCount();
    for (int i = static_cast<int>(count) - 1; i >= 0; --i) {
        if (pos.y >= y && pos.y < y + ItemHeight) {
            return i;
        }
        y += ItemHeight + 2;
    }
    return -1;
}

int LayersPanel::HitTestButton(const Point& pos, int layerIndex) const {
    if (layerIndex < 0) return -1;
    
    Rect client = GetClientBounds();
    int y = 28;
    size_t count = m_layerManager->GetLayerCount();
    for (int i = static_cast<int>(count) - 1; i >= 0; --i) {
        if (i == layerIndex) {
            if (pos.y >= y && pos.y < y + ItemHeight) {
                if (pos.x >= client.Width() - 48 && pos.x < client.Width() - 28) return 0; // visibility
                if (pos.x >= client.Width() - 26 && pos.x < client.Width() - 6) return 1; // lock
            }
            break;
        }
        y += ItemHeight + 2;
    }
    return -1;
}

void LayersPanel::OnSize(const Size& newSize) {
    Invalidate();
}

} // namespace UI
} // namespace VividPic
