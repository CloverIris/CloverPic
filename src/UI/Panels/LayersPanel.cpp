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

wchar_t LayersPanel::GetLayerIcon(int btnType, bool state) {
    switch (btnType) {
        case 0: return state ? L'\uE8B5' : L'\uE8B6'; // View / Hide
        case 1: return state ? L'\uE72E' : L'\uE785'; // Lock / Unlock
        case 2: return state ? L'\uE73E' : L'\uE73E'; // Solo (same, colored differently)
        default: return L'?';
    }
}

// ------------------------------------------------------------------
// Layout helpers
// ------------------------------------------------------------------

int LayersPanel::GetLayerListY() const {
    int dropdownY = Theme::GetSize(26);
    int opacityY = dropdownY + DropdownHeight + Theme::GetSize(4);
    return opacityY + Theme::GetSize(24) + Theme::GetSize(4);
}

Rect LayersPanel::GetListViewportRect() const {
    Rect client = GetClientBounds();
    int listY = GetLayerListY();
    int toolbarY = client.Height() - Theme::GetSize(32);
    if (toolbarY < listY) toolbarY = listY;
    return Rect(0, listY, client.Width(), toolbarY);
}

void LayersPanel::UpdateScrollView() {
    Rect vp = GetListViewportRect();
    m_scrollView.SetViewportSize(vp.Width(), vp.Height());
    if (m_layerManager) {
        int contentH = static_cast<int>(m_layerManager->GetLayerCount()) * (ItemHeight + 2);
        m_scrollView.SetContentHeight(contentH);
    } else {
        m_scrollView.SetContentHeight(0);
    }
}

Rect LayersPanel::GetLayerItemRect(int layerIndex) const {
    if (!m_layerManager) return Rect();
    int y = GetLayerListY() - m_scrollView.GetScrollOffset();
    size_t count = m_layerManager->GetLayerCount();
    for (int i = static_cast<int>(count) - 1; i >= 0; --i) {
        if (i == layerIndex) {
            return Rect(0, y, GetClientBounds().Width(), y + ItemHeight);
        }
        y += ItemHeight + 2;
    }
    return Rect();
}

void LayersPanel::GetLayerItemButtonRects(int layerIndex, Rect& outSolo, Rect& outPA, Rect& outEye, Rect& outLock) const {
    Rect item = GetLayerItemRect(layerIndex);
    if (item.Width() <= 0 || item.Height() <= 0) return;
    int y = item.top + Theme::GetSize(4);
    int h = Theme::GetSize(20);
    int right = item.right - Theme::GetSize(4);

    outLock = Rect(right - BtnWidth, y, right, y + h);
    right -= BtnWidth + BtnGap;
    outEye = Rect(right - BtnWidth, y, right, y + h);
    right -= BtnWidth + BtnGap;
    outPA = Rect(right - BtnWidth, y, right, y + h);
    right -= BtnWidth + BtnGap;
    outSolo = Rect(right - BtnWidth, y, right, y + h);
}

// ------------------------------------------------------------------
// Painting
// ------------------------------------------------------------------

void LayersPanel::OnPaint(HDC hdc, const Rect& clip) {
    (void)clip;
    Rect client = GetClientBounds();

    // Background
    HBRUSH bgBrush = Theme::CachedBrush(Theme::PanelBackground);
    RECT rc = client.ToWin32Rect();
    FillRect(hdc, &rc, bgBrush);

    if (!m_layerManager) return;

    SetBkMode(hdc, TRANSPARENT);

    // Modern panel header
    int titleH = Theme::GetSize(22);
    Rect headerRc(0, 0, client.Width(), titleH);
    Theme::DrawPanelHeaderModern(hdc, headerRc, L"图层", IsCollapsed());

    if (IsCollapsed()) return;

    // Blend mode dropdown for active layer
    int dropdownY = Theme::GetSize(26);
    DrawBlendDropdown(hdc, Theme::GetSize(8), dropdownY, client.Width() - Theme::GetSize(16));

    // Opacity slider
    int opacityY = dropdownY + DropdownHeight + Theme::GetSize(4);
    auto activeLayer = m_layerManager->GetActiveLayer();
    uint8_t opacity = activeLayer ? activeLayer->GetOpacity() : 255;
    DrawOpacitySlider(hdc, Theme::GetSize(8), opacityY, client.Width() - Theme::GetSize(16), opacity);

    bool drawDropdownAfter = m_blendDropdownOpen;

    // Layer items — clip to viewport
    Rect listVp = GetListViewportRect();
    HRGN oldClip = CreateRectRgn(0, 0, 0, 0);
    if (GetClipRgn(hdc, oldClip) == 0) {
        DeleteObject(oldClip);
        oldClip = nullptr;
    }
    HRGN listClip = CreateRectRgn(listVp.left, listVp.top, listVp.right, listVp.bottom);
    ExtSelectClipRgn(hdc, listClip, RGN_AND);
    DeleteObject(listClip);

    int y = GetLayerListY() - m_scrollView.GetScrollOffset();

    HFONT itemFont = Theme::GetCachedFont(Theme::FontID::Label);
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, itemFont));

    size_t count = m_layerManager->GetLayerCount();
    for (int i = static_cast<int>(count) - 1; i >= 0; --i) {
        auto layer = m_layerManager->GetLayer(i);
        if (!layer) continue;

        // Skip items outside viewport
        if (y + ItemHeight < listVp.top || y > listVp.bottom) {
            y += ItemHeight + 2;
            continue;
        }

        bool isActive = (static_cast<size_t>(i) == m_layerManager->GetActiveLayerIndex());
        bool isHovered = (i == m_hoverLayerIndex);

        // Drag insertion line
        if (m_draggingLayer && m_dragTargetIndex >= 0 && m_dragTargetIndex != m_dragLayerIndex && i == m_dragTargetIndex) {
            HPEN linePen = CreatePen(PS_SOLID, Theme::GetSize(2), Theme::HighlightBlue);
            HPEN oldLinePen = static_cast<HPEN>(SelectObject(hdc, linePen));
            MoveToEx(hdc, 0, y, nullptr);
            LineTo(hdc, client.Width(), y);
            SelectObject(hdc, oldLinePen);
            DeleteObject(linePen);
        }

        // Item background
        Rect itemRc(Theme::GetSize(4), y, client.Width() - Theme::GetSize(4), y + ItemHeight);
        if (isActive) {
            // Left accent bar + tinted background
            int accentW = Theme::GetSize(3);
            HBRUSH accentBrush = Theme::CachedBrush(Theme::HighlightBlue);
            RECT accentRc = { itemRc.left, itemRc.top, itemRc.left + accentW, itemRc.bottom };
            FillRect(hdc, &accentRc, accentBrush);

            // Semi-transparent highlight background (approximated with solid color)
            uint32_t hlColor = 0x1A3A5A; // Approximate #0078D7 @ 20% over #3C3C3C
            HBRUSH hlBrush = Theme::CachedBrush(hlColor);
            RECT hlRc = { itemRc.left + accentW, itemRc.top, itemRc.right, itemRc.bottom };
            FillRect(hdc, &hlRc, hlBrush);
        } else if (isHovered) {
            HBRUSH hoverBrush = Theme::CachedBrush(0x424242);
            RECT hoverRc = itemRc.ToWin32Rect();
            FillRect(hdc, &hoverRc, hoverBrush);
        } else if ((count - 1 - i) % 2 == 1) {
            HBRUSH altBrush = Theme::CachedBrush(0x383838);
            RECT altRc = itemRc.ToWin32Rect();
            FillRect(hdc, &altRc, altBrush);
        }

        // Rounded corners for item (subtle)
        HPEN itemBorder = CreatePen(PS_SOLID, 1, isActive ? Theme::HighlightBlue : (isHovered ? Theme::BorderLight : Theme::BorderDark));
        HPEN oldItemPen = static_cast<HPEN>(SelectObject(hdc, itemBorder));
        HBRUSH nullBr = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
        HBRUSH oldNullBr = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
        int r = Theme::GetSize(2);
        RoundRect(hdc, itemRc.left, itemRc.top, itemRc.right, itemRc.bottom, r * 2, r * 2);
        SelectObject(hdc, oldItemPen);
        SelectObject(hdc, oldNullBr);
        DeleteObject(itemBorder);

        // Dragged item highlight
        if (m_draggingLayer && i == m_dragLayerIndex) {
            HPEN dragPen = CreatePen(PS_DOT, 1, Theme::HighlightBlue);
            HPEN oldDragPen = static_cast<HPEN>(SelectObject(hdc, dragPen));
            HBRUSH nullBrush = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
            HBRUSH oldNullBrush = static_cast<HBRUSH>(SelectObject(hdc, nullBrush));
            Rectangle(hdc, 2, y + 2, client.Width() - 2, y + ItemHeight - 2);
            SelectObject(hdc, oldDragPen);
            SelectObject(hdc, oldNullBrush);
            DeleteObject(dragPen);
        }

        // Thumbnail
        layer->UpdateThumbnail();
        const auto& thumbData = layer->GetThumbnail();
        int thumbX = Theme::GetSize(8);
        int thumbY = y + Theme::GetSize(4);
        if (!thumbData.empty()) {
            BITMAPINFO bmi = {};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = 64;
            bmi.bmiHeader.biHeight = -64;
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;
            StretchDIBits(hdc, thumbX, thumbY, ThumbSize, ThumbSize,
                          0, 0, 64, 64, thumbData.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);
        } else {
            HBRUSH thumbBrush = Theme::CachedBrush(Theme::ButtonDefault);
            RECT thumbRc = { thumbX, thumbY, thumbX + ThumbSize, thumbY + ThumbSize };
            FillRect(hdc, &thumbRc, thumbBrush);
        }

        // Button rects
        Rect soloRc, paRc, eyeRc, lockRc;
        GetLayerItemButtonRects(i, soloRc, paRc, eyeRc, lockRc);

        // Icon font
        HFONT iconFont = Theme::GetCachedFont(Theme::FontID::Icon);
        HFONT oldIconFont = static_cast<HFONT>(SelectObject(hdc, iconFont));

        // Solo button
        bool isSolo = m_layerManager->IsSoloActive() && m_layerManager->GetSoloLayerIndex() == static_cast<size_t>(i);
        SetTextColor(hdc, isSolo ? Theme::HighlightBlue : Theme::TextDisabled);
        RECT sRc = soloRc.ToWin32Rect();
        std::wstring soloStr(1, GetLayerIcon(2, isSolo));
        DrawTextW(hdc, soloStr.c_str(), -1, &sRc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);

        // Protect alpha checkbox
        Theme::DrawCheckBox(hdc, paRc, layer->IsProtectAlpha(), false, nullptr);

        // Visibility icon
        SetTextColor(hdc, layer->IsVisible() ? Theme::TextPrimary : Theme::TextDisabled);
        RECT eRc = eyeRc.ToWin32Rect();
        std::wstring eyeStr(1, GetLayerIcon(0, layer->IsVisible()));
        DrawTextW(hdc, eyeStr.c_str(), -1, &eRc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);

        // Lock icon
        SetTextColor(hdc, layer->IsLocked() ? Theme::TextPrimary : Theme::TextDisabled);
        RECT lRc = lockRc.ToWin32Rect();
        std::wstring lockStr(1, GetLayerIcon(1, layer->IsLocked()));
        DrawTextW(hdc, lockStr.c_str(), -1, &lRc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);

        SelectObject(hdc, oldIconFont);

        // Name (or rename field)
        int nameX = thumbX + ThumbSize + Theme::GetSize(8);
        Rect nameRc(nameX, y + Theme::GetSize(4), soloRc.left - Theme::GetSize(4), y + Theme::GetSize(20));

        if (m_renamingLayer && m_renameLayerIndex == i) {
            DrawRenameField(hdc, nameRc);
        } else {
            SetTextColor(hdc, isActive ? Theme::TextInverse : Theme::TextPrimary);
            RECT nRc = nameRc.ToWin32Rect();
            DrawTextW(hdc, layer->GetName().c_str(), -1, &nRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
        }

        // Opacity + Blend mode text
        SetTextColor(hdc, Theme::TextSecondary);
        RECT opRc = { nameX, y + Theme::GetSize(24), soloRc.left - Theme::GetSize(4), y + ItemHeight - Theme::GetSize(2) };
        std::wostringstream oss;
        oss << GetBlendModeName(layer->GetBlendMode()) << L" | "
            << static_cast<int>(layer->GetOpacity() * 100 / 255) << L"%";
        DrawTextW(hdc, oss.str().c_str(), -1, &opRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

        y += ItemHeight + 2;
    }

    SelectObject(hdc, oldFont);

    // Restore clip
    if (oldClip) {
        SelectClipRgn(hdc, oldClip);
        DeleteObject(oldClip);
    } else {
        SelectClipRgn(hdc, nullptr);
    }

    // Scrollbar
    m_scrollView.Draw(hdc, listVp);

    // Blend dropdown overlay
    if (drawDropdownAfter) {
        DrawBlendDropdownList(hdc, Theme::GetSize(8), dropdownY + DropdownHeight, client.Width() - Theme::GetSize(16));
    }

    // Bottom toolbar
    DrawToolbar(hdc);
}

void LayersPanel::DrawToolbar(HDC hdc) {
    Rect client = GetClientBounds();
    int toolbarY = client.Height() - Theme::GetSize(32);
    HBRUSH toolbarBrush = Theme::CachedBrush(Theme::BackgroundDark);
    RECT toolbarRc = { 0, toolbarY, client.Width(), client.Height() };
    FillRect(hdc, &toolbarRc, toolbarBrush);

    HPEN topLine = Theme::Pen(Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, topLine));
    MoveToEx(hdc, 0, toolbarY, nullptr);
    LineTo(hdc, client.Width(), toolbarY);
    SelectObject(hdc, oldPen);
    DeleteObject(topLine);

    // 4 icon buttons: New, Duplicate, Merge Down, Delete
    int btnCount = 4;
    int pad = Theme::GetSize(4);
    int btnSize = Theme::GetSize(24);
    int gap = (client.Width() - pad * 2 - btnSize * btnCount) / (btnCount - 1);
    if (gap < Theme::GetSize(2)) gap = Theme::GetSize(2);

    const wchar_t icons[4] = { L'\uE710', L'\uE8C8', L'\uE74B', L'\uE74D' };

    for (int i = 0; i < btnCount; ++i) {
        int bx = pad + i * (btnSize + gap);
        int by = toolbarY + (client.Height() - toolbarY - btnSize) / 2;
        Rect btnRc(bx, by, bx + btnSize, by + btnSize);
        bool hovered = (m_hoverToolbarBtn == i);
        Theme::DrawIconButton(hdc, btnRc, icons[i], false, hovered);
    }
}

void LayersPanel::DrawRenameField(HDC hdc, const Rect& nameRc) {
    // Input background
    HBRUSH editBg = Theme::CachedBrush(Theme::InputBackground);
    RECT editRc = nameRc.ToWin32Rect();
    FillRect(hdc, &editRc, editBg);

    HPEN borderPen = CreatePen(PS_SOLID, 1, Theme::HighlightBlue);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
    HBRUSH nullBr = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
    Rectangle(hdc, nameRc.left, nameRc.top, nameRc.right, nameRc.bottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(borderPen);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::TextPrimary);
    HFONT font = Theme::GetCachedFont(Theme::FontID::Label);
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
    RECT textRc = { nameRc.left + Theme::GetSize(2), nameRc.top, nameRc.right - Theme::GetSize(2), nameRc.bottom };
    DrawTextW(hdc, m_renameText.c_str(), -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

    // Caret
    if (HasFocus()) {
        HDC memDC = CreateCompatibleDC(hdc);
        HFONT oldF = static_cast<HFONT>(SelectObject(memDC, font));
        SIZE sz;
        GetTextExtentPoint32W(memDC, m_renameText.c_str(), m_renameCursorPos, &sz);
        SelectObject(memDC, oldF);
        DeleteDC(memDC);
        int caretX = nameRc.left + Theme::GetSize(2) + sz.cx;
        int caretY1 = nameRc.top + Theme::GetSize(2);
        int caretY2 = nameRc.bottom - Theme::GetSize(2);
        HPEN caretPen = CreatePen(PS_SOLID, 1, Theme::HighlightBlue);
        oldPen = static_cast<HPEN>(SelectObject(hdc, caretPen));
        MoveToEx(hdc, caretX, caretY1, nullptr);
        LineTo(hdc, caretX, caretY2);
        SelectObject(hdc, oldPen);
        DeleteObject(caretPen);
    }
    SelectObject(hdc, oldFont);
}

void LayersPanel::DrawBlendDropdown(HDC hdc, int x, int y, int width) {
    int radius = Theme::GetSize(2);
    HBRUSH bg = Theme::CachedBrush(Theme::ButtonDefault);
    RECT rc = { x, y, x + width, y + DropdownHeight };
    FillRect(hdc, &rc, bg);

    HPEN pen = Theme::Pen(Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    HBRUSH nullBr = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
    RoundRect(hdc, x, y, x + width, y + DropdownHeight, radius * 2, radius * 2);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(pen);

    const wchar_t* name = L"正常";
    auto activeLayer = m_layerManager ? m_layerManager->GetActiveLayer() : nullptr;
    if (activeLayer) {
        name = GetBlendModeName(activeLayer->GetBlendMode());
    }

    SetTextColor(hdc, Theme::TextPrimary);
    HFONT font = Theme::GetCachedFont(Theme::FontID::Label);
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
    RECT textRc = { x + 6, y, x + width - 20, y + DropdownHeight };
    DrawTextW(hdc, name, -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

    SetTextColor(hdc, Theme::TextSecondary);
    RECT arrowRc = { x + width - 18, y, x + width - 4, y + DropdownHeight };
    DrawTextW(hdc, m_blendDropdownOpen ? L"\u25B2" : L"\u25BC", -1, &arrowRc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
    SelectObject(hdc, oldFont);
}

void LayersPanel::DrawBlendDropdownList(HDC hdc, int x, int y, int width) {
    int listH = BlendModeCount * DropdownHeight;
    int radius = Theme::GetSize(2);

    // Background with shadow-like border
    HBRUSH bg = Theme::CachedBrush(Theme::PanelBackground);
    RECT rc = { x, y, x + width, y + listH };
    FillRect(hdc, &rc, bg);

    HPEN pen = Theme::Pen(Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    HBRUSH nullBr = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
    RoundRect(hdc, x, y, x + width, y + listH, radius * 2, radius * 2);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(pen);

    auto activeLayer = m_layerManager ? m_layerManager->GetActiveLayer() : nullptr;
    BlendMode currentMode = activeLayer ? activeLayer->GetBlendMode() : BlendMode::Normal;

    for (int i = 0; i < BlendModeCount; ++i) {
        int iy = y + i * DropdownHeight;
        bool hovered = (i == m_blendHoverIndex);
        bool isCurrent = (BlendModes[i] == currentMode);

        if (hovered) {
            HBRUSH itemBg = Theme::CachedBrush(Theme::HighlightBlue);
            RECT itemRc = { x + 1, iy, x + width - 1, iy + DropdownHeight };
            FillRect(hdc, &itemRc, itemBg);
        } else if (isCurrent) {
            HBRUSH itemBg = Theme::CachedBrush(0x2A4A6A); // subtle blue tint
            RECT itemRc = { x + 1, iy, x + width - 1, iy + DropdownHeight };
            FillRect(hdc, &itemRc, itemBg);
        }

        // Left blue mark for current item
        if (isCurrent) {
            HBRUSH markBrush = Theme::CachedBrush(Theme::HighlightBlue);
            RECT markRc = { x + 1, iy + 4, x + Theme::GetSize(3), iy + DropdownHeight - 4 };
            FillRect(hdc, &markRc, markBrush);
        }

        SetTextColor(hdc, hovered ? Theme::TextInverse : Theme::TextPrimary);
        HFONT font = Theme::GetCachedFont(Theme::FontID::Label);
        HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
        RECT textRc = { x + (isCurrent ? Theme::GetSize(6) : 6), iy, x + width - 6, iy + DropdownHeight };
        DrawTextW(hdc, GetBlendModeName(BlendModes[i]), -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        SelectObject(hdc, oldFont);
    }
}

void LayersPanel::DrawOpacitySlider(HDC hdc, int x, int y, int width, uint8_t opacity) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::TextSecondary);
    HFONT font = Theme::GetCachedFont(Theme::FontID::Label);
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
    std::wostringstream oss;
    if (m_opacityEditing) {
        oss << L"不透明度: " << m_opacityText << L"%";
    } else {
        oss << L"不透明度: " << (opacity * 100 / 255) << L"%";
    }
    RECT labelRc = { x, y, x + width, y + Theme::GetSize(14) };
    DrawTextW(hdc, oss.str().c_str(), -1, &labelRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

    int trackY = y + Theme::GetSize(16);
    int trackHeight = Theme::GetSize(4);
    Rect sliderRc(x, trackY, x + width, trackY + trackHeight);
    float value01 = opacity / 255.0f;
    Theme::DrawSlider(hdc, sliderRc, value01, false);

    SelectObject(hdc, oldFont);
}

// ------------------------------------------------------------------
// Rename inline edit
// ------------------------------------------------------------------

void LayersPanel::StartRename(int layerIndex) {
    if (!m_layerManager) return;
    auto layer = m_layerManager->GetLayer(layerIndex);
    if (!layer) return;
    m_renamingLayer = true;
    m_renameLayerIndex = layerIndex;
    m_renameText = layer->GetName();
    m_renameCursorPos = static_cast<int>(m_renameText.length());
    SetFocus();
    Invalidate();
}

void LayersPanel::EndRename(bool apply) {
    if (!m_renamingLayer) return;
    if (apply && m_layerManager && m_renameLayerIndex >= 0) {
        auto layer = m_layerManager->GetLayer(m_renameLayerIndex);
        if (layer && !m_renameText.empty()) {
            layer->SetName(m_renameText);
        }
    }
    m_renamingLayer = false;
    m_renameLayerIndex = -1;
    m_renameText.clear();
    m_renameCursorPos = 0;
    Invalidate();
}

void LayersPanel::RenameInsertChar(wchar_t ch) {
    if (m_renameText.length() >= 64) return;
    if (ch < 32) return;
    m_renameText.insert(m_renameCursorPos, 1, ch);
    m_renameCursorPos++;
    Invalidate();
}

void LayersPanel::RenameDeleteChar() {
    if (m_renameCursorPos > 0) {
        m_renameText.erase(m_renameCursorPos - 1, 1);
        m_renameCursorPos--;
        Invalidate();
    }
}

void LayersPanel::RenameMoveCursor(int delta) {
    m_renameCursorPos += delta;
    if (m_renameCursorPos < 0) m_renameCursorPos = 0;
    if (m_renameCursorPos > static_cast<int>(m_renameText.length())) m_renameCursorPos = static_cast<int>(m_renameText.length());
    Invalidate();
}

// ------------------------------------------------------------------
// Opacity inline edit
// ------------------------------------------------------------------

void LayersPanel::StartOpacityEdit() {
    if (!m_layerManager) return;
    auto layer = m_layerManager->GetActiveLayer();
    if (!layer) return;
    m_opacityEditing = true;
    std::wostringstream oss;
    oss << (layer->GetOpacity() * 100 / 255);
    m_opacityText = oss.str();
    m_opacityCursorPos = static_cast<int>(m_opacityText.length());
    SetFocus();
    Invalidate();
}

void LayersPanel::EndOpacityEdit(bool apply) {
    if (!m_opacityEditing) return;
    if (apply && m_layerManager) {
        auto layer = m_layerManager->GetActiveLayer();
        if (layer) {
            int val = 0;
            for (wchar_t ch : m_opacityText) {
                if (ch >= L'0' && ch <= L'9') {
                    val = val * 10 + (ch - L'0');
                }
            }
            val = std::clamp(val, 0, 100);
            layer->SetOpacity(static_cast<uint8_t>(val * 255 / 100));
            m_layerManager->MarkCompositeDirty();
        }
    }
    m_opacityEditing = false;
    m_opacityText.clear();
    m_opacityCursorPos = 0;
    Invalidate();
}

// ------------------------------------------------------------------
// Mouse handling
// ------------------------------------------------------------------

void LayersPanel::OnMouseDown(const Point& pos, MouseButton button) {
    if (button != MouseButton::Left || !m_layerManager) return;

    if (IsCollapsible() && pos.y < Theme::GetSize(26)) {
        ToggleCollapsed();
        return;
    }

    // End inline edits on outside click
    if (m_renamingLayer) {
        int renamedLayer = HitTestLayer(pos);
        if (renamedLayer != m_renameLayerIndex) {
            EndRename(true);
            return;
        }
    }
    if (m_opacityEditing) {
        if (HitTestOpacitySlider(pos) < 0) {
            EndOpacityEdit(true);
            return;
        }
    }

    // Blend dropdown list (overlay)
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

    // Scrollbar
    Rect listVp = GetListViewportRect();
    if (m_scrollView.IsPointInThumb(listVp, pos)) {
        m_scrollbarDragging = true;
        m_scrollView.BeginDrag(pos, listVp);
        SetCapture(m_hwnd);
        Invalidate();
        return;
    }
    if (m_scrollView.IsPointInScrollbar(listVp, pos)) {
        Rect thumbRc = m_scrollView.GetThumbRect(listVp);
        int pageDelta = listVp.Height() * 3 / 4;
        if (pos.y < thumbRc.top) {
            m_scrollView.ScrollBy(-pageDelta);
        } else {
            m_scrollView.ScrollBy(pageDelta);
        }
        Invalidate();
        return;
    }

    Rect client = GetClientBounds();
    int toolbarY = client.Height() - Theme::GetSize(32);

    // Toolbar
    if (pos.y >= toolbarY) {
        int btnIndex = HitTestToolbarButton(pos);
        switch (btnIndex) {
            case 0: {
                size_t count = m_layerManager->GetLayerCount();
                std::wostringstream name;
                name << L"图层 " << (count + 1);
                m_layerManager->AddLayer(name.str(), LayerType::Color);
                UpdateScrollView();
                break;
            }
            case 1: {
                size_t active = m_layerManager->GetActiveLayerIndex();
                m_layerManager->DuplicateLayer(active);
                UpdateScrollView();
                break;
            }
            case 2: {
                size_t active = m_layerManager->GetActiveLayerIndex();
                if (active < m_layerManager->GetLayerCount() - 1) {
                    m_layerManager->MergeDown(active);
                }
                UpdateScrollView();
                break;
            }
            case 3: {
                size_t active = m_layerManager->GetActiveLayerIndex();
                if (m_layerManager->GetLayerCount() > 1) {
                    m_layerManager->DeleteLayer(active);
                }
                UpdateScrollView();
                break;
            }
        }
        Invalidate();
        return;
    }

    // Opacity slider
    if (HitTestOpacitySlider(pos) >= 0) {
        // Check for double-click on label area (above track)
        int dropdownY = Theme::GetSize(26);
        int opacityY = dropdownY + DropdownHeight + Theme::GetSize(4);
        if (pos.y >= opacityY && pos.y < opacityY + Theme::GetSize(14)) {
            StartOpacityEdit();
        } else {
            m_opacityDragging = true;
            SetOpacityFromSliderPos(pos.x, Theme::GetSize(8), client.Width() - Theme::GetSize(16));
        }
        Invalidate();
        return;
    }

    // Layer selection / drag start / buttons
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

        // Double-click detection for rename
        uint64_t now = GetTickCount64();
        if (m_lastClickLayer == layerIdx && (now - m_lastClickTime) < 400) {
            m_lastClickTime = 0;
            m_lastClickLayer = -1;
            StartRename(layerIdx);
            return;
        }
        m_lastClickTime = now;
        m_lastClickLayer = layerIdx;

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
    int y = GetLayerListY() - m_scrollView.GetScrollOffset();
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
    Rect soloRc, paRc, eyeRc, lockRc;
    GetLayerItemButtonRects(layerIndex, soloRc, paRc, eyeRc, lockRc);
    if (lockRc.Contains(pos)) return 1;
    if (eyeRc.Contains(pos)) return 0;
    if (paRc.Contains(pos)) return 2;
    if (soloRc.Contains(pos)) return 3;
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
    int width = client.Width() - Theme::GetSize(16);
    for (int i = 0; i < BlendModeCount; ++i) {
        int iy = y + i * DropdownHeight;
        if (pos.x >= x && pos.x < x + width && pos.y >= iy && pos.y < iy + DropdownHeight) {
            return i;
        }
    }
    return -1;
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
    int pad = Theme::GetSize(4);
    int btnSize = Theme::GetSize(24);
    int gap = (client.Width() - pad * 2 - btnSize * btnCount) / (btnCount - 1);
    if (gap < Theme::GetSize(2)) gap = Theme::GetSize(2);
    for (int i = 0; i < btnCount; ++i) {
        int bx = pad + i * (btnSize + gap);
        int by = toolbarY + (client.Height() - toolbarY - btnSize) / 2;
        if (pos.x >= bx && pos.x < bx + btnSize && pos.y >= by && pos.y < by + btnSize) {
            return i;
        }
    }
    return -1;
}

void LayersPanel::OnMouseMove(const Point& pos) {
    // Toolbar hover
    int oldHoverToolbar = m_hoverToolbarBtn;
    m_hoverToolbarBtn = HitTestToolbarButton(pos);
    if (oldHoverToolbar != m_hoverToolbarBtn) {
        Invalidate();
    }

    // Layer hover
    int oldHoverLayer = m_hoverLayerIndex;
    m_hoverLayerIndex = HitTestLayer(pos);
    if (oldHoverLayer != m_hoverLayerIndex) {
        Invalidate();
    }

    if (m_scrollbarDragging) {
        Rect listVp = GetListViewportRect();
        m_scrollView.UpdateDrag(pos, listVp);
        Invalidate();
        return;
    }

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

    // Blend dropdown hover
    if (m_blendDropdownOpen) {
        int newHover = HitTestBlendItem(pos);
        if (newHover != m_blendHoverIndex) {
            m_blendHoverIndex = newHover;
            Invalidate();
        }
    }
}

void LayersPanel::OnMouseUp(const Point& pos, MouseButton button) {
    (void)pos;
    if (button == MouseButton::Left) {
        m_opacityDragging = false;
        m_scrollbarDragging = false;
        m_scrollView.EndDrag();

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

void LayersPanel::OnMouseLeave() {
    if (m_hoverToolbarBtn >= 0 || m_hoverLayerIndex >= 0) {
        m_hoverToolbarBtn = -1;
        m_hoverLayerIndex = -1;
        Invalidate();
    }
}

void LayersPanel::OnMouseWheel(int delta) {
    m_scrollView.ScrollBy(delta);
    Invalidate();
}

void LayersPanel::OnSize(const Size& newSize) {
    (void)newSize;
    UpdateScrollView();
    Invalidate();
}

// ------------------------------------------------------------------
// Keyboard (for inline edits)
// ------------------------------------------------------------------

void LayersPanel::OnKeyDown(uint32_t keyCode) {
    if (m_renamingLayer) {
        switch (keyCode) {
            case VK_LEFT: RenameMoveCursor(-1); break;
            case VK_RIGHT: RenameMoveCursor(1); break;
            case VK_HOME: m_renameCursorPos = 0; Invalidate(); break;
            case VK_END: m_renameCursorPos = static_cast<int>(m_renameText.length()); Invalidate(); break;
            case VK_DELETE:
                if (m_renameCursorPos < static_cast<int>(m_renameText.length())) {
                    m_renameText.erase(m_renameCursorPos, 1);
                    Invalidate();
                }
                break;
            case VK_RETURN: EndRename(true); break;
            case VK_ESCAPE: EndRename(false); break;
        }
        return;
    }
    if (m_opacityEditing) {
        switch (keyCode) {
            case VK_LEFT: m_opacityCursorPos = std::max(0, m_opacityCursorPos - 1); Invalidate(); break;
            case VK_RIGHT: m_opacityCursorPos = std::min(static_cast<int>(m_opacityText.length()), m_opacityCursorPos + 1); Invalidate(); break;
            case VK_HOME: m_opacityCursorPos = 0; Invalidate(); break;
            case VK_END: m_opacityCursorPos = static_cast<int>(m_opacityText.length()); Invalidate(); break;
            case VK_DELETE:
                if (m_opacityCursorPos < static_cast<int>(m_opacityText.length())) {
                    m_opacityText.erase(m_opacityCursorPos, 1);
                    Invalidate();
                }
                break;
            case VK_RETURN: EndOpacityEdit(true); break;
            case VK_ESCAPE: EndOpacityEdit(false); break;
        }
        return;
    }
}

void LayersPanel::OnChar(wchar_t ch) {
    if (m_renamingLayer) {
        if (ch == L'\b') {
            RenameDeleteChar();
        } else if (ch >= 32) {
            RenameInsertChar(ch);
        }
        return;
    }
    if (m_opacityEditing) {
        if (ch == L'\b') {
            if (m_opacityCursorPos > 0) {
                m_opacityText.erase(m_opacityCursorPos - 1, 1);
                m_opacityCursorPos--;
                Invalidate();
            }
        } else if (ch >= L'0' && ch <= L'9' && m_opacityText.length() < 3) {
            m_opacityText.insert(m_opacityCursorPos, 1, ch);
            m_opacityCursorPos++;
            Invalidate();
        }
        return;
    }
}

} // namespace UI
} // namespace VividPic
