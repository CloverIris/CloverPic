#include "UI/Panels/LayersPanel.h"
#include "UI/Core/Theme.h"
#include <algorithm>
#include <sstream>

namespace VividPic {
namespace UI {

const BlendMode LayersPanel::BlendModes[BlendModeCount] = {
    BlendMode::Normal, BlendMode::Multiply, BlendMode::Screen, BlendMode::Overlay,
    BlendMode::Difference, BlendMode::Add, BlendMode::Subtract, BlendMode::Darken,
    BlendMode::Lighten, BlendMode::ColorDodge, BlendMode::ColorBurn, BlendMode::HardLight,
    BlendMode::SoftLight, BlendMode::Exclusion, BlendMode::Hue, BlendMode::Saturation,
    BlendMode::Color, BlendMode::Luminosity
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
        case BlendMode::Lighten: return L"变亮";
        case BlendMode::ColorDodge: return L"颜色减淡";
        case BlendMode::ColorBurn: return L"颜色加深";
        case BlendMode::HardLight: return L"强光";
        case BlendMode::SoftLight: return L"柔光";
        case BlendMode::Exclusion: return L"排除";
        case BlendMode::Hue: return L"色相";
        case BlendMode::Saturation: return L"饱和度";
        case BlendMode::Color: return L"颜色";
        case BlendMode::Luminosity: return L"明度";
        default: return L"正常";
    }
}

void LayersPanel::OnPaint(HDC hdc, const Rect& clip) {
    Rect client = GetClientBounds();

    // Background
    HBRUSH bgBrush = Theme::SolidBrush(Theme::PanelBackground);
    RECT rc = client.ToWin32Rect();
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    if (!m_layerManager) return;

    SetBkMode(hdc, TRANSPARENT);

    // Title
    SetTextColor(hdc, Theme::TextSecondary);
    HFONT titleFont = Theme::GetFont(Theme::FontID::PanelTitle);
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, titleFont));
    RECT titleRc = { 8, 4, client.Width() - 8, 20 };
    DrawTextW(hdc, L"图层", -1, &titleRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    SelectObject(hdc, oldFont);
    DeleteObject(titleFont);

    // Separator
    HPEN sepPen = Theme::Pen(Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, sepPen));
    int sepY = Theme::GetSize(22);
    MoveToEx(hdc, Theme::GetSize(8), sepY, nullptr);
    LineTo(hdc, client.Width() - Theme::GetSize(8), sepY);
    SelectObject(hdc, oldPen);
    DeleteObject(sepPen);

    // Blend mode dropdown for active layer
    int dropdownY = Theme::GetSize(26);
    DrawBlendDropdown(hdc, Theme::GetSize(8), dropdownY, client.Width() - Theme::GetSize(16));

    if (m_blendDropdownOpen) {
        DrawBlendDropdownList(hdc, Theme::GetSize(8), dropdownY + DropdownHeight, client.Width() - Theme::GetSize(16));
    }

    // Opacity slider
    int opacityY = dropdownY + DropdownHeight + Theme::GetSize(4);
    auto activeLayer = m_layerManager ? m_layerManager->GetActiveLayer() : nullptr;
    uint8_t opacity = activeLayer ? activeLayer->GetOpacity() : 255;
    DrawOpacitySlider(hdc, Theme::GetSize(8), opacityY, client.Width() - Theme::GetSize(16), opacity);

    // Layer items (top to bottom order, but display top layers at top)
    int y = opacityY + Theme::GetSize(24) + Theme::GetSize(4);
    if (m_blendDropdownOpen) {
        y += BlendModeCount * DropdownHeight;
    }

    HFONT itemFont = CreateFontW(Theme::GetFontSize(11), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    oldFont = static_cast<HFONT>(SelectObject(hdc, itemFont));

    size_t count = m_layerManager->GetLayerCount();
    for (int i = static_cast<int>(count) - 1; i >= 0; --i) {
        auto layer = m_layerManager->GetLayer(i);
        if (!layer) continue;

        bool isActive = (static_cast<size_t>(i) == m_layerManager->GetActiveLayerIndex());

        // Drag insertion line
        if (m_draggingLayer && m_dragTargetIndex >= 0 && m_dragTargetIndex != m_dragLayerIndex && i == m_dragTargetIndex) {
            HPEN linePen = CreatePen(PS_SOLID, Theme::GetSize(2), Theme::HighlightBlue);
            HPEN oldLinePen = static_cast<HPEN>(SelectObject(hdc, linePen));
            MoveToEx(hdc, 0, y, nullptr);
            LineTo(hdc, client.Width(), y);
            SelectObject(hdc, oldLinePen);
            DeleteObject(linePen);
        }

        // Dragged item highlight
        if (m_draggingLayer && i == m_dragLayerIndex) {
            HPEN dragPen = CreatePen(PS_DOT, 1, Theme::HighlightBlue);
            HPEN oldDragPen = static_cast<HPEN>(SelectObject(hdc, dragPen));
            HBRUSH nullBr = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
            HBRUSH oldNullBr = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
            Rectangle(hdc, 2, y + 2, client.Width() - 2, y + ItemHeight - 2);
            SelectObject(hdc, oldDragPen);
            SelectObject(hdc, oldNullBr);
            DeleteObject(dragPen);
        }

        // Background
        if (isActive) {
            HBRUSH activeBrush = Theme::SolidBrush(Theme::HighlightBlue);
            RECT itemBg = { Theme::GetSize(4), y, client.Width() - Theme::GetSize(4), y + ItemHeight };
            FillRect(hdc, &itemBg, activeBrush);
            DeleteObject(activeBrush);
        }

        // Thumbnail (real layer content preview)
        layer->UpdateThumbnail();
        const auto& thumbData = layer->GetThumbnail();
        if (!thumbData.empty()) {
            BITMAPINFO bmi = {};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = 64;
            bmi.bmiHeader.biHeight = -64; // top-down
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;
            StretchDIBits(hdc, Theme::GetSize(8), y + 4, ThumbSize, ThumbSize,
                          0, 0, 64, 64, thumbData.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);
        } else {
            HBRUSH thumbBrush = Theme::SolidBrush(Theme::ButtonDefault);
            RECT thumbRc = { Theme::GetSize(8), y + Theme::GetSize(4), Theme::GetSize(8) + ThumbSize, y + Theme::GetSize(4) + ThumbSize };
            FillRect(hdc, &thumbRc, thumbBrush);
            DeleteObject(thumbBrush);
        }

        // Visibility icon (eye)
        SetTextColor(hdc, layer->IsVisible() ? Theme::TextPrimary : Theme::TextDisabled);
        RECT eyeRc = { client.Width() - Theme::GetSize(48), y + Theme::GetSize(4), client.Width() - Theme::GetSize(28), y + Theme::GetSize(20) };
        DrawTextW(hdc, layer->IsVisible() ? L"👁" : L"⊘", -1, &eyeRc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);

        // Lock icon
        SetTextColor(hdc, layer->IsLocked() ? Theme::TextPrimary : Theme::TextDisabled);
        RECT lockRc = { client.Width() - Theme::GetSize(26), y + Theme::GetSize(4), client.Width() - Theme::GetSize(6), y + Theme::GetSize(20) };
        DrawTextW(hdc, layer->IsLocked() ? L"🔒" : L"○", -1, &lockRc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);

        // Solo button
        bool isSolo = m_layerManager->IsSoloActive() && m_layerManager->GetSoloLayerIndex() == static_cast<size_t>(i);
        SetTextColor(hdc, isSolo ? Theme::HighlightBlue : Theme::TextDisabled);
        RECT soloRc = { client.Width() - Theme::GetSize(96), y + Theme::GetSize(4), client.Width() - Theme::GetSize(76), y + Theme::GetSize(20) };
        DrawTextW(hdc, L"S", -1, &soloRc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);

        // Protect alpha checkbox
        Rect paRc(client.Width() - Theme::GetSize(72), y + Theme::GetSize(4),
                  client.Width() - Theme::GetSize(52), y + Theme::GetSize(20));
        Theme::DrawCheckBox(hdc, paRc, layer->IsProtectAlpha(), false, nullptr);

        // Name
        SetTextColor(hdc, isActive ? Theme::TextInverse : Theme::TextPrimary);
        RECT nameRc = { Theme::GetSize(8) + ThumbSize + Theme::GetSize(8), y + Theme::GetSize(4), client.Width() - Theme::GetSize(100), y + Theme::GetSize(20) };
        DrawTextW(hdc, layer->GetName().c_str(), -1, &nameRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);

        // Opacity + Blend mode text
        SetTextColor(hdc, Theme::TextSecondary);
        RECT opRc = { Theme::GetSize(8) + ThumbSize + Theme::GetSize(8), y + Theme::GetSize(24), client.Width() - Theme::GetSize(52), y + ItemHeight - Theme::GetSize(2) };
        std::wostringstream oss;
        oss << GetBlendModeName(layer->GetBlendMode()) << L" | "
            << static_cast<int>(layer->GetOpacity() * 100 / 255) << L"%";
        DrawTextW(hdc, oss.str().c_str(), -1, &opRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

        y += ItemHeight + 2;
    }

    SelectObject(hdc, oldFont);
    DeleteObject(itemFont);

    // Bottom toolbar area
    int toolbarY = client.Height() - Theme::GetSize(32);
    HBRUSH toolbarBrush = Theme::SolidBrush(Theme::BackgroundDark);
    RECT toolbarRc = { 0, toolbarY, client.Width(), client.Height() };
    FillRect(hdc, &toolbarRc, toolbarBrush);
    DeleteObject(toolbarBrush);

    HPEN topLine = Theme::Pen(Theme::BorderLight);
    oldPen = static_cast<HPEN>(SelectObject(hdc, topLine));
    MoveToEx(hdc, 0, toolbarY, nullptr);
    LineTo(hdc, client.Width(), toolbarY);
    SelectObject(hdc, oldPen);
    DeleteObject(topLine);

    // Toolbar buttons: New, Duplicate, Merge Down, Delete
    int btnCount = 4;
    int btnWidth = client.Width() / btnCount;
    const wchar_t* btnLabels[4] = { L"+", L"⧉", L"↓", L"🗑" };
    HFONT btnFont = Theme::GetFont(Theme::FontID::Button);
    HFONT oldBtnFont = static_cast<HFONT>(SelectObject(hdc, btnFont));
    SetTextColor(hdc, Theme::TextPrimary);
    for (int i = 0; i < btnCount; ++i) {
        int bx = i * btnWidth;
        RECT btnRc = { bx, toolbarY, bx + btnWidth, client.Height() };
        DrawTextW(hdc, btnLabels[i], -1, &btnRc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
    }
    SelectObject(hdc, oldBtnFont);
    DeleteObject(btnFont);
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
    HFONT font = CreateFontW(Theme::GetFontSize(11), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
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

        HBRUSH bg = Theme::SolidBrush(hovered ? Theme::HighlightBlue : Theme::PanelBackground);
        RECT rc = { x, iy, x + width, iy + DropdownHeight };
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        SetTextColor(hdc, hovered ? Theme::TextInverse : Theme::TextPrimary);
        HFONT font = Theme::GetFont(Theme::FontID::Label);
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
    int toolbarY = client.Height() - Theme::GetSize(32);

    if (pos.y >= toolbarY) {
        // Toolbar buttons
        int btnIndex = HitTestToolbarButton(pos);
        switch (btnIndex) {
            case 0: { // New layer
                size_t count = m_layerManager->GetLayerCount();
                std::wostringstream name;
                name << L"图层 " << (count + 1);
                m_layerManager->AddLayer(name.str(), LayerType::Color);
                break;
            }
            case 1: { // Duplicate
                size_t active = m_layerManager->GetActiveLayerIndex();
                m_layerManager->DuplicateLayer(active);
                break;
            }
            case 2: { // Merge down
                size_t active = m_layerManager->GetActiveLayerIndex();
                if (active < m_layerManager->GetLayerCount() - 1) {
                    m_layerManager->MergeDown(active);
                }
                break;
            }
            case 3: { // Delete
                size_t active = m_layerManager->GetActiveLayerIndex();
                if (m_layerManager->GetLayerCount() > 1) {
                    m_layerManager->DeleteLayer(active);
                }
                break;
            }
        }
        Invalidate();
        return;
    }

    // Opacity slider
    if (HitTestOpacitySlider(pos) >= 0) {
        m_opacityDragging = true;
        SetOpacityFromSliderPos(pos.x, Theme::GetSize(8), client.Width() - Theme::GetSize(16));
        Invalidate();
        return;
    }

    // Layer selection / drag start
    int layerIdx = HitTestLayer(pos);
    if (layerIdx >= 0) {
        int btn = HitTestButton(pos, layerIdx);
        if (btn == 0) {
            m_layerManager->ToggleLayerVisibility(layerIdx);
            Invalidate();
            return;
        } else if (btn == 1) {
            m_layerManager->ToggleLayerLock(layerIdx);
            Invalidate();
            return;
        } else if (btn == 2) {
            auto layer = m_layerManager->GetLayer(layerIdx);
            if (layer) {
                layer->SetProtectAlpha(!layer->IsProtectAlpha());
                Invalidate();
            }
            return;
        } else if (btn == 3) {
            m_layerManager->ToggleSolo(layerIdx);
            Invalidate();
            return;
        }
        
        m_layerManager->SetActiveLayer(layerIdx);
        m_draggingLayer = true;
        m_dragLayerIndex = layerIdx;
        m_dragTargetIndex = layerIdx;
        m_dragStartY = pos.y;
        SetCapture(m_hwnd);
        Invalidate();
    }
}

int LayersPanel::HitTestLayer(const Point& pos) const {
    if (!m_layerManager) return -1;

    int dropdownY = Theme::GetSize(26);
    int y = dropdownY + DropdownHeight + Theme::GetSize(8);
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
    int dropdownY = Theme::GetSize(26);
    int y = dropdownY + DropdownHeight + Theme::GetSize(8);
    if (m_blendDropdownOpen) {
        y += BlendModeCount * DropdownHeight;
    }

    size_t count = m_layerManager->GetLayerCount();
    for (int i = static_cast<int>(count) - 1; i >= 0; --i) {
        if (i == layerIndex) {
            if (pos.y >= y && pos.y < y + ItemHeight) {
                if (pos.x >= client.Width() - Theme::GetSize(96) && pos.x < client.Width() - Theme::GetSize(76)) return 3; // solo
                if (pos.x >= client.Width() - Theme::GetSize(72) && pos.x < client.Width() - Theme::GetSize(52)) return 2; // protectAlpha
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
    int x = Theme::GetSize(8);
    int y = Theme::GetSize(26);
    int width = client.Width() - Theme::GetSize(16);
    return pos.x >= x && pos.x < x + width && pos.y >= y && pos.y < y + DropdownHeight;
}

int LayersPanel::HitTestBlendItem(const Point& pos) const {
    if (!m_blendDropdownOpen) return -1;
    Rect client = GetClientBounds();
    int x = Theme::GetSize(8);
    int y = Theme::GetSize(26) + DropdownHeight;
    int width = client.Width() - 16;
    for (int i = 0; i < BlendModeCount; ++i) {
        int iy = y + i * DropdownHeight;
        if (pos.x >= x && pos.x < x + width && pos.y >= iy && pos.y < iy + DropdownHeight) {
            return i;
        }
    }
    return -1;
}

void LayersPanel::DrawOpacitySlider(HDC hdc, int x, int y, int width, uint8_t opacity) {
    // Label
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::TextSecondary);
    HFONT font = Theme::GetFont(Theme::FontID::Label);
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
    std::wostringstream oss;
    oss << L"不透明度: " << (opacity * 100 / 255) << L"%";
    RECT labelRc = { x, y, x + width, y + Theme::GetSize(14) };
    DrawTextW(hdc, oss.str().c_str(), -1, &labelRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

    // Track
    int trackY = y + Theme::GetSize(16);
    int trackHeight = Theme::GetSize(4);
    Rect sliderRc(x, trackY, x + width, trackY + trackHeight);
    float value01 = opacity / 255.0f;
    Theme::DrawSlider(hdc, sliderRc, value01, false);

    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

void LayersPanel::SetOpacityFromSliderPos(int x, int trackX, int trackWidth) {
    if (!m_layerManager) return;
    auto layer = m_layerManager->GetActiveLayer();
    if (!layer) return;
    int relX = x - trackX;
    if (relX < 0) relX = 0;
    if (relX > trackWidth) relX = trackWidth;
    uint8_t opacity = static_cast<uint8_t>(relX * 255 / trackWidth);
    layer->SetOpacity(opacity);
    m_layerManager->MarkCompositeDirty();
}

int LayersPanel::HitTestOpacitySlider(const Point& pos) const {
    Rect client = GetClientBounds();
    int dropdownY = Theme::GetSize(26);
    int opacityY = dropdownY + DropdownHeight + Theme::GetSize(4);
    int x = Theme::GetSize(8);
    int width = client.Width() - Theme::GetSize(16);
    int trackY = opacityY + Theme::GetSize(16);
    int trackHeight = Theme::GetSize(4);
    if (pos.x >= x && pos.x < x + width && pos.y >= trackY - Theme::GetSize(6) && pos.y < trackY + trackHeight + Theme::GetSize(6)) {
        return 0;
    }
    return -1;
}

int LayersPanel::HitTestToolbarButton(const Point& pos) const {
    Rect client = GetClientBounds();
    int toolbarY = client.Height() - Theme::GetSize(32);
    if (pos.y < toolbarY) return -1;
    int btnCount = 4;
    int btnWidth = client.Width() / btnCount;
    int btnIndex = pos.x / btnWidth;
    if (btnIndex < 0) btnIndex = 0;
    if (btnIndex >= btnCount) btnIndex = btnCount - 1;
    return btnIndex;
}

void LayersPanel::OnMouseMove(const Point& pos) {
    if (m_opacityDragging) {
        Rect client = GetClientBounds();
        SetOpacityFromSliderPos(pos.x, Theme::GetSize(8), client.Width() - Theme::GetSize(16));
        Invalidate();
    }
    
    if (m_draggingLayer && m_layerManager) {
        int target = HitTestLayer(pos);
        if (target >= 0 && target != m_dragLayerIndex) {
            m_dragTargetIndex = target;
            Invalidate();
        }
    }
}

void LayersPanel::OnMouseUp(const Point& pos, MouseButton button) {
    if (button == MouseButton::Left) {
        m_opacityDragging = false;
        
        if (m_draggingLayer) {
            m_draggingLayer = false;
            ReleaseCapture();
            if (m_dragTargetIndex >= 0 && m_dragTargetIndex != m_dragLayerIndex && m_layerManager) {
                m_layerManager->MoveLayer(m_dragLayerIndex, m_dragTargetIndex);
            }
            m_dragLayerIndex = -1;
            m_dragTargetIndex = -1;
            Invalidate();
        }
    }
}

void LayersPanel::OnSize(const Size& newSize) {
    Invalidate();
}

} // namespace UI
} // namespace VividPic
