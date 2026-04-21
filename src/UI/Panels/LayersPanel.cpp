#include "UI/Panels/LayersPanel.h"
#include "UI/Core/Theme.h"
#include <algorithm>
#include <sstream>

namespace VividPic {
namespace UI {

const BlendMode LayersPanel::BlendModes[BlendModeCount] = {
    BlendMode::Normal, BlendMode::Multiply, BlendMode::Screen, BlendMode::Overlay,
    BlendMode::Difference, BlendMode::Add, BlendMode::Subtract, BlendMode::Darken
};

LayersPanel::LayersPanel() = default;

void LayersPanel::Refresh() {
    Invalidate();
}

const wchar_t* LayersPanel::GetBlendModeName(BlendMode mode) {
    switch (mode) {
        case BlendMode::Normal: return L"正常";
        case BlendMode::Multiply: return L"正片叠底";
        case BlendMode::Screen: return L"滤色";
        case BlendMode::Overlay: return L"叠加";
        case BlendMode::Difference: return L"差值";
        case BlendMode::Add: return L"相加";
        case BlendMode::Subtract: return L"减去";
        case BlendMode::Darken: return L"变暗";
        default: return L"正常";
    }
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

    // Blend mode dropdown for active layer
    int dropdownY = 26;
    DrawBlendDropdown(hdc, 8, dropdownY, client.Width() - 16);

    if (m_blendDropdownOpen) {
        DrawBlendDropdownList(hdc, 8, dropdownY + DropdownHeight, client.Width() - 16);
    }

    // Layer items (top to bottom order, but display top layers at top)
    int y = dropdownY + DropdownHeight + 8;
    if (m_blendDropdownOpen) {
        y += BlendModeCount * DropdownHeight;
    }

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

        // Opacity + Blend mode text
        SetTextColor(hdc, RGB(0xA0, 0xA0, 0xA0));
        RECT opRc = { 8 + ThumbSize + 8, y + 24, client.Width() - 52, y + ItemHeight - 2 };
        std::wostringstream oss;
        oss << GetBlendModeName(layer->GetBlendMode()) << L" | "
            << static_cast<int>(layer->GetOpacity() * 100 / 255) << L"%";
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

void LayersPanel::DrawBlendDropdown(HDC hdc, int x, int y, int width) {
    // Background
    HBRUSH bg = Theme::SolidBrush(Theme::ButtonDefault);
    RECT rc = { x, y, x + width, y + DropdownHeight };
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);

    // Border
    HPEN pen = CreatePen(PS_SOLID, 1, Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    HBRUSH nullBr = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
    Rectangle(hdc, x, y, x + width, y + DropdownHeight);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(pen);

    // Text: current blend mode of active layer
    const wchar_t* name = L"正常";
    auto activeLayer = m_layerManager ? m_layerManager->GetActiveLayer() : nullptr;
    if (activeLayer) {
        name = GetBlendModeName(activeLayer->GetBlendMode());
    }

    SetTextColor(hdc, Theme::TextPrimary);
    HFONT font = CreateFontW(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
    RECT textRc = { x + 6, y, x + width - 20, y + DropdownHeight };
    DrawTextW(hdc, name, -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

    // Dropdown arrow
    SetTextColor(hdc, Theme::TextSecondary);
    RECT arrowRc = { x + width - 18, y, x + width - 4, y + DropdownHeight };
    DrawTextW(hdc, m_blendDropdownOpen ? L"▲" : L"▼", -1, &arrowRc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

void LayersPanel::DrawBlendDropdownList(HDC hdc, int x, int y, int width) {
    for (int i = 0; i < BlendModeCount; ++i) {
        int iy = y + i * DropdownHeight;
        bool hovered = (i == m_blendHoverIndex);

        HBRUSH bg = Theme::SolidBrush(hovered ? RGB(0x00, 0x78, 0xD7) : Theme::PanelBackground);
        RECT rc = { x, iy, x + width, iy + DropdownHeight };
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        SetTextColor(hdc, hovered ? RGB(0xFF, 0xFF, 0xFF) : Theme::TextPrimary);
        HFONT font = CreateFontW(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
        HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
        RECT textRc = { x + 6, iy, x + width - 6, iy + DropdownHeight };
        DrawTextW(hdc, GetBlendModeName(BlendModes[i]), -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        SelectObject(hdc, oldFont);
        DeleteObject(font);
    }

    // Border around list
    HPEN pen = CreatePen(PS_SOLID, 1, Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    HBRUSH nullBr = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
    Rectangle(hdc, x, y, x + width, y + BlendModeCount * DropdownHeight);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(pen);
}

void LayersPanel::OnMouseDown(const Point& pos, MouseButton button) {
    if (button != MouseButton::Left || !m_layerManager) return;

    // Check blend dropdown
    if (m_blendDropdownOpen) {
        int blendItem = HitTestBlendItem(pos);
        if (blendItem >= 0) {
            auto layer = m_layerManager->GetActiveLayer();
            if (layer) {
                layer->SetBlendMode(BlendModes[blendItem]);
                m_layerManager->MarkCompositeDirty();
            }
            m_blendDropdownOpen = false;
            Invalidate();
            return;
        } else if (!HitTestBlendDropdown(pos)) {
            m_blendDropdownOpen = false;
            Invalidate();
            return;
        }
    } else if (HitTestBlendDropdown(pos)) {
        m_blendDropdownOpen = true;
        Invalidate();
        return;
    }

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

    int dropdownY = 26;
    int y = dropdownY + DropdownHeight + 8;
    if (m_blendDropdownOpen) {
        y += BlendModeCount * DropdownHeight;
    }

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
    int dropdownY = 26;
    int y = dropdownY + DropdownHeight + 8;
    if (m_blendDropdownOpen) {
        y += BlendModeCount * DropdownHeight;
    }

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

bool LayersPanel::HitTestBlendDropdown(const Point& pos) const {
    Rect client = GetClientBounds();
    int x = 8;
    int y = 26;
    int width = client.Width() - 16;
    return pos.x >= x && pos.x < x + width && pos.y >= y && pos.y < y + DropdownHeight;
}

int LayersPanel::HitTestBlendItem(const Point& pos) const {
    if (!m_blendDropdownOpen) return -1;
    Rect client = GetClientBounds();
    int x = 8;
    int y = 26 + DropdownHeight;
    int width = client.Width() - 16;
    for (int i = 0; i < BlendModeCount; ++i) {
        int iy = y + i * DropdownHeight;
        if (pos.x >= x && pos.x < x + width && pos.y >= iy && pos.y < iy + DropdownHeight) {
            return i;
        }
    }
    return -1;
}

void LayersPanel::OnSize(const Size& newSize) {
    Invalidate();
}

} // namespace UI
} // namespace VividPic
