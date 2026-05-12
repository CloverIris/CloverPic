#include "UI/Panels/NavigatorPanel.h"
#include "UI/Core/Theme.h"
#include <algorithm>
#include <sstream>

namespace VividPic {
namespace UI {

NavigatorPanel::NavigatorPanel() = default;

NavigatorPanel::~NavigatorPanel() {
    CleanupThumbnail();
}

void NavigatorPanel::CleanupThumbnail() {
    if (m_thumbDC) {
        if (m_thumbOldBitmap) SelectObject(m_thumbDC, m_thumbOldBitmap);
        if (m_thumbBitmap) DeleteObject(m_thumbBitmap);
        DeleteDC(m_thumbDC);
        m_thumbDC = nullptr;
        m_thumbBitmap = nullptr;
        m_thumbOldBitmap = nullptr;
        m_thumbPixels = nullptr;
    }
}

void NavigatorPanel::SetCanvasViewTransform(float zoom, float panX, float panY, float rotation) {
    m_zoom = zoom;
    m_panX = panX;
    m_panY = panY;
    m_rotation = rotation;
    Invalidate();
}

void NavigatorPanel::SetCanvasSize(uint32_t width, uint32_t height) {
    m_canvasWidth = width;
    m_canvasHeight = height;
    m_thumbnailDirty = true;
    Invalidate();
}

void NavigatorPanel::Refresh() {
    m_thumbnailDirty = true;
    Invalidate();
}

void NavigatorPanel::GetThumbRect(int& outX, int& outY, int& outW, int& outH) const {
    Rect client = GetClientBounds();
    int availW = client.Width() - ThumbMargin * 2;
    int availH = client.Height() - Theme::GetSize(80);
    
    float scale = GetThumbScale(availW, availH);
    outW = static_cast<int>(m_canvasWidth * scale);
    outH = static_cast<int>(m_canvasHeight * scale);
    outX = ThumbMargin + (availW - outW) / 2;
    outY = 28 + (availH - outH) / 2;
}

void NavigatorPanel::OnPaint(HDC hdc, const Rect& clip) {
    Rect client = GetClientBounds();
    
    HBRUSH bgBrush = Theme::CachedBrush(Theme::PanelBackground);
    RECT rc = client.ToWin32Rect();
    FillRect(hdc, &rc, bgBrush);
    
    // Modern panel header
    int titleH = Theme::GetSize(22);
    Rect headerRc(0, 0, client.Width(), titleH);
    Theme::DrawPanelHeaderModern(hdc, headerRc, L"导航器", IsCollapsed());
    
    if (m_thumbnailDirty) {
        UpdateThumbnail();
    }
    
    DrawThumbnail(hdc);
    DrawViewRect(hdc);
    
    // Zoom control buttons (icon buttons)
    int btnY = client.Height() - Theme::GetSize(52);
    int btnCount = 4;
    int btnSize = Theme::GetSize(24);
    int pad = Theme::GetSize(8);
    int gap = (client.Width() - pad * 2 - btnSize * btnCount) / (btnCount - 1);
    if (gap < Theme::GetSize(2)) gap = Theme::GetSize(2);
    const wchar_t btnIcons[4] = { L'\uE71F', L'\uE8A3', L'\uE71B', L'\uE73A' }; // ZoomOut, ZoomIn, Page, FullScreen

    for (int i = 0; i < btnCount; ++i) {
        int bx = pad + i * (btnSize + gap);
        Rect btnRc(bx, btnY, bx + btnSize, btnY + btnSize);
        Theme::DrawIconButton(hdc, btnRc, btnIcons[i], false, false);
    }

    // Zoom text
    SetTextColor(hdc, Theme::TextPrimary);
    HFONT valFont = Theme::GetCachedFont(Theme::FontID::Small);
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, valFont));
    
    std::wostringstream oss;
    oss << L"缩放: " << static_cast<int>(m_zoom * 100) << L"%";
    if (std::abs(m_rotation) > 0.5f) {
        oss << L" | 旋转: " << static_cast<int>(m_rotation) << L"°";
    }
    RECT zoomRc = { Theme::GetSize(8), client.Height() - Theme::GetSize(24), client.Width() - Theme::GetSize(8), client.Height() - Theme::GetSize(4) };
    DrawTextW(hdc, oss.str().c_str(), -1, &zoomRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    
    SelectObject(hdc, oldFont);
}

void NavigatorPanel::UpdateThumbnail() {
    m_thumbnailDirty = false;
    
    if (m_canvasWidth == 0 || m_canvasHeight == 0) return;
    
    int thumbX, thumbY, thumbW, thumbH;
    GetThumbRect(thumbX, thumbY, thumbW, thumbH);
    
    m_cachedThumbX = thumbX;
    m_cachedThumbY = thumbY;
    m_cachedThumbW = thumbW;
    m_cachedThumbH = thumbH;
    
    // Recreate DIBSection if size changed
    if (!m_thumbDC || m_cachedThumbW != thumbW || m_cachedThumbH != thumbH) {
        CleanupThumbnail();
        
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = thumbW;
        bmi.bmiHeader.biHeight = -thumbH; // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        
        m_thumbDC = CreateCompatibleDC(nullptr);
        m_thumbBitmap = CreateDIBSection(m_thumbDC, &bmi, DIB_RGB_COLORS,
                                         reinterpret_cast<void**>(&m_thumbPixels), nullptr, 0);
        m_thumbOldBitmap = static_cast<HBITMAP>(SelectObject(m_thumbDC, m_thumbBitmap));
    }
    
    // Checkerboard background
    const int checkSize = 4;
    for (int y = 0; y < thumbH; ++y) {
        for (int x = 0; x < thumbW; ++x) {
            bool dark = ((x / checkSize + y / checkSize) % 2 == 0);
            uint8_t c = dark ? 0x50 : 0x40;
            m_thumbPixels[y * thumbW + x] = RGB(c, c, c);
        }
    }
    
    // Composite visible layers
    if (m_layerManager) {
        float scaleX = static_cast<float>(m_canvasWidth) / thumbW;
        float scaleY = static_cast<float>(m_canvasHeight) / thumbH;
        
        for (int y = 0; y < thumbH; ++y) {
            for (int x = 0; x < thumbW; ++x) {
                uint32_t cx = static_cast<uint32_t>(x * scaleX);
                uint32_t cy = static_cast<uint32_t>(y * scaleY);
                if (cx >= m_canvasWidth) cx = m_canvasWidth - 1;
                if (cy >= m_canvasHeight) cy = m_canvasHeight - 1;
                
                Color c(0, 0, 0, 0);
                // Bottom-up blend of all visible layers
                for (size_t i = 0; i < m_layerManager->GetLayerCount(); ++i) {
                    auto layer = m_layerManager->GetLayer(i);
                    if (!layer || !layer->IsVisible()) continue;
                    Color pc = layer->GetPixel(cx, cy);
                    if (pc.a > 0) {
                        c = BlendOperations::BlendPixel(pc, c, layer->GetBlendMode(), layer->GetOpacity() / 255.0f);
                    }
                }
                
                if (c.a > 0) {
                    // Alpha blend over checkerboard bg
                    uint8_t bg = (((x / checkSize + y / checkSize) % 2 == 0) ? 0x50 : 0x40);
                    uint8_t r = static_cast<uint8_t>(c.r * c.a / 255.0f + bg * (255 - c.a) / 255.0f);
                    uint8_t g = static_cast<uint8_t>(c.g * c.a / 255.0f + bg * (255 - c.a) / 255.0f);
                    uint8_t b = static_cast<uint8_t>(c.b * c.a / 255.0f + bg * (255 - c.a) / 255.0f);
                    m_thumbPixels[y * thumbW + x] = RGB(b, g, r); // BGR for DIB
                }
            }
        }
    }
}

void NavigatorPanel::DrawThumbnail(HDC hdc) {
    if (m_canvasWidth == 0 || m_canvasHeight == 0) return;
    if (!m_thumbDC || !m_thumbBitmap) return;
    
    BitBlt(hdc, m_cachedThumbX, m_cachedThumbY, m_cachedThumbW, m_cachedThumbH,
           m_thumbDC, 0, 0, SRCCOPY);
    
    // Border
    HPEN borderPen = CreatePen(PS_SOLID, 1, Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
    HBRUSH nullBrush = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, nullBrush));
    Rectangle(hdc, m_cachedThumbX, m_cachedThumbY,
              m_cachedThumbX + m_cachedThumbW, m_cachedThumbY + m_cachedThumbH);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(borderPen);
}

void NavigatorPanel::DrawViewRect(HDC hdc) {
    if (m_canvasWidth == 0 || m_canvasHeight == 0) return;
    
    int thumbX = m_cachedThumbX;
    int thumbY = m_cachedThumbY;
    int thumbW = m_cachedThumbW;
    int thumbH = m_cachedThumbH;
    
    if (thumbW <= 0 || thumbH <= 0) return;
    
    float scale = static_cast<float>(thumbW) / m_canvasWidth;
    
    // Calculate view rectangle in thumbnail space
    Window* parent = GetParent();
    Size parentSize = parent ? parent->GetClientBounds().GetSize() : Size(800, 600);
    
    float viewX = -m_panX / m_zoom * scale + thumbX;
    float viewY = -m_panY / m_zoom * scale + thumbY;
    float viewW = parentSize.width / m_zoom * scale;
    float viewH = parentSize.height / m_zoom * scale;
    
    viewW = std::min(viewW, static_cast<float>(thumbW));
    viewH = std::min(viewH, static_cast<float>(thumbH));
    
    if (viewW > 2 && viewH > 2) {
        // Semi-transparent blue fill
        HBRUSH viewBrush = CreateSolidBrush(RGB(0x00, 0x78, 0xD7));
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, viewBrush));
        RECT fillRc = { static_cast<int>(viewX), static_cast<int>(viewY),
                        static_cast<int>(viewX + viewW), static_cast<int>(viewY + viewH) };
        FillRect(hdc, &fillRc, viewBrush);
        SelectObject(hdc, oldBrush);
        DeleteObject(viewBrush);

        // Border
        HPEN viewPen = CreatePen(PS_SOLID, 2, RGB(0x00, 0x78, 0xD7));
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, viewPen));
        HBRUSH nullBrush = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
        oldBrush = static_cast<HBRUSH>(SelectObject(hdc, nullBrush));
        Rectangle(hdc, static_cast<int>(viewX), static_cast<int>(viewY),
                  static_cast<int>(viewX + viewW), static_cast<int>(viewY + viewH));
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(viewPen);
    }
}

void NavigatorPanel::OnMouseDown(const Point& pos, MouseButton button) {
    if (button != MouseButton::Left) return;

    if (IsCollapsible() && pos.y < Theme::GetSize(26)) {
        ToggleCollapsed();
        return;
    }

    Rect client = GetClientBounds();
    
    // Check zoom buttons
    int btnY = client.Height() - Theme::GetSize(52);
    int btnCount = 4;
    int btnSize = Theme::GetSize(24);
    int pad = Theme::GetSize(8);
    int gap = (client.Width() - pad * 2 - btnSize * btnCount) / (btnCount - 1);
    if (gap < Theme::GetSize(2)) gap = Theme::GetSize(2);
    if (pos.y >= btnY && pos.y < btnY + btnSize) {
        int btnIndex = -1;
        for (int i = 0; i < btnCount; ++i) {
            int bx = pad + i * (btnSize + gap);
            if (pos.x >= bx && pos.x < bx + btnSize) {
                btnIndex = i;
                break;
            }
        }
        if (btnIndex >= 0 && btnIndex < btnCount && m_onZoomChanged) {
            switch (btnIndex) {
                case 0: m_onZoomChanged(std::max(0.01f, m_zoom * 0.8f)); break;
                case 1: m_onZoomChanged(std::min(16.0f, m_zoom * 1.25f)); break;
                case 2: m_onZoomChanged(1.0f); break;
                case 3: m_onZoomChanged(-1.0f); break; // Fit signal
            }
        }
        return;
    }
    
    int thumbX = m_cachedThumbX;
    int thumbY = m_cachedThumbY;
    int thumbW = m_cachedThumbW;
    int thumbH = m_cachedThumbH;
    
    if (pos.x >= thumbX && pos.x < thumbX + thumbW && pos.y >= thumbY && pos.y < thumbY + thumbH) {
        m_draggingView = true;
        float scale = static_cast<float>(thumbW) / m_canvasWidth;
        float canvasX = (pos.x - thumbX) / scale;
        float canvasY = (pos.y - thumbY) / scale;
        
        Window* parent = GetParent();
        Size parentSize = parent ? parent->GetClientBounds().GetSize() : Size(800, 600);
        
        m_panX = parentSize.width / 2.0f - canvasX * m_zoom;
        m_panY = parentSize.height / 2.0f - canvasY * m_zoom;
        if (m_onPanChanged) {
            m_onPanChanged(m_panX, m_panY);
        }
        Invalidate();
    }
}

void NavigatorPanel::OnMouseMove(const Point& pos) {
    if (!m_draggingView) return;
    
    int thumbX = m_cachedThumbX;
    int thumbY = m_cachedThumbY;
    int thumbW = m_cachedThumbW;
    int thumbH = m_cachedThumbH;
    
    if (thumbW <= 0) return;
    
    float scale = static_cast<float>(thumbW) / m_canvasWidth;
    float canvasX = (pos.x - thumbX) / scale;
    float canvasY = (pos.y - thumbY) / scale;
    
    Window* parent = GetParent();
    Size parentSize = parent ? parent->GetClientBounds().GetSize() : Size(800, 600);
    
    m_panX = parentSize.width / 2.0f - canvasX * m_zoom;
    m_panY = parentSize.height / 2.0f - canvasY * m_zoom;
    if (m_onPanChanged) {
        m_onPanChanged(m_panX, m_panY);
    }
    Invalidate();
}

void NavigatorPanel::OnMouseUp(const Point& pos, MouseButton button) {
    m_draggingView = false;
}

float NavigatorPanel::GetThumbScale(int availWidth, int availHeight) const {
    if (m_canvasWidth == 0 || m_canvasHeight == 0) return 1.0f;
    float sx = static_cast<float>(availWidth) / m_canvasWidth;
    float sy = static_cast<float>(availHeight) / m_canvasHeight;
    return std::min(sx, sy);
}

} // namespace UI
} // namespace VividPic
