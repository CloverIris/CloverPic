#include "UI/Panels/NavigatorPanel.h"
#include "UI/Core/Theme.h"
#include <algorithm>
#include <sstream>

namespace VividPic {
namespace UI {

NavigatorPanel::NavigatorPanel() = default;

void NavigatorPanel::SetCanvasViewTransform(float zoom, float panX, float panY) {
    m_zoom = zoom;
    m_panX = panX;
    m_panY = panY;
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

void NavigatorPanel::OnPaint(HDC hdc, const Rect& clip) {
    Rect client = GetClientBounds();
    
    HBRUSH bgBrush = Theme::SolidBrush(Theme::PanelBackground);
    RECT rc = client.ToWin32Rect();
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);
    
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::TextSecondary);
    HFONT font = CreateFontW(12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
    
    RECT titleRc = { 8, 4, client.Width() - 8, 20 };
    DrawTextW(hdc, L"导航器", -1, &titleRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    SelectObject(hdc, oldFont);
    DeleteObject(font);
    
    HPEN sepPen = CreatePen(PS_SOLID, 1, Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, sepPen));
    MoveToEx(hdc, 8, 22, nullptr);
    LineTo(hdc, client.Width() - 8, 22);
    SelectObject(hdc, oldPen);
    DeleteObject(sepPen);
    
    DrawThumbnail(hdc);
    DrawViewRect(hdc);
    
    // Zoom text
    SetTextColor(hdc, Theme::TextPrimary);
    HFONT valFont = CreateFontW(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    oldFont = static_cast<HFONT>(SelectObject(hdc, valFont));
    
    std::wostringstream oss;
    oss << L"缩放: " << static_cast<int>(m_zoom * 100) << L"%";
    RECT zoomRc = { 8, client.Height() - 24, client.Width() - 8, client.Height() - 4 };
    DrawTextW(hdc, oss.str().c_str(), -1, &zoomRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    
    SelectObject(hdc, oldFont);
    DeleteObject(valFont);
}

void NavigatorPanel::DrawThumbnail(HDC hdc) {
    if (m_canvasWidth == 0 || m_canvasHeight == 0) return;
    
    Rect client = GetClientBounds();
    int availW = client.Width() - ThumbMargin * 2;
    int availH = client.Height() - 50; // Leave room for title and zoom text
    
    float scale = GetThumbScale(availW, availH);
    int thumbW = static_cast<int>(m_canvasWidth * scale);
    int thumbH = static_cast<int>(m_canvasHeight * scale);
    int thumbX = ThumbMargin + (availW - thumbW) / 2;
    int thumbY = 28 + (availH - thumbH) / 2;
    
    // Background
    HBRUSH bgBrush = CreateSolidBrush(RGB(0x40, 0x40, 0x40));
    RECT bgRc = { thumbX, thumbY, thumbX + thumbW, thumbY + thumbH };
    FillRect(hdc, &bgRc, bgBrush);
    DeleteObject(bgBrush);
    
    // Checkerboard for transparent areas
    const int checkSize = 4;
    for (int y = 0; y < thumbH; y += checkSize) {
        for (int x = 0; x < thumbW; x += checkSize) {
            COLORREF c = ((x / checkSize + y / checkSize) % 2 == 0) ? RGB(0x50, 0x50, 0x50) : RGB(0x40, 0x40, 0x40);
            HBRUSH checkBrush = CreateSolidBrush(c);
            RECT cr = { thumbX + x, thumbY + y, thumbX + std::min(x + checkSize, thumbW), thumbY + std::min(y + checkSize, thumbH) };
            FillRect(hdc, &cr, checkBrush);
            DeleteObject(checkBrush);
        }
    }
    
    // Draw simple representation from layer manager
    if (m_layerManager) {
        // For M3, draw a gradient representing the canvas
        for (int y = 0; y < thumbH; ++y) {
            for (int x = 0; x < thumbW; ++x) {
                uint32_t cx = static_cast<uint32_t>(x / scale);
                uint32_t cy = static_cast<uint32_t>(y / scale);
                Color c = Color(200, 200, 200, 255); // Placeholder
                
                // Sample from top visible layer that has a pixel
                for (int i = static_cast<int>(m_layerManager->GetLayerCount()) - 1; i >= 0; --i) {
                    auto layer = m_layerManager->GetLayer(i);
                    if (!layer || !layer->IsVisible()) continue;
                    Color pc = layer->GetPixel(cx, cy);
                    if (pc.a > 0) {
                        c = BlendOperations::AlphaBlend(pc, c, 1.0f);
                        break;
                    }
                }
                
                SetPixel(hdc, thumbX + x, thumbY + y, RGB(c.r, c.g, c.b));
            }
        }
    }
    
    // Border
    HPEN borderPen = CreatePen(PS_SOLID, 1, Theme::BorderLight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
    HBRUSH nullBrush = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, nullBrush));
    Rectangle(hdc, thumbX, thumbY, thumbX + thumbW, thumbY + thumbH);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(borderPen);
}

void NavigatorPanel::DrawViewRect(HDC hdc) {
    if (m_canvasWidth == 0 || m_canvasHeight == 0) return;
    
    Rect client = GetClientBounds();
    int availW = client.Width() - ThumbMargin * 2;
    int availH = client.Height() - 50;
    
    float scale = GetThumbScale(availW, availH);
    int thumbW = static_cast<int>(m_canvasWidth * scale);
    int thumbH = static_cast<int>(m_canvasHeight * scale);
    int thumbX = ThumbMargin + (availW - thumbW) / 2;
    int thumbY = 28 + (availH - thumbH) / 2;
    
    // Calculate view rectangle in thumbnail space
    // The view shows the client area of CanvasView
    // We need the parent Window's client size to know the view size
    Window* parent = GetParent();
    Size parentSize = parent ? parent->GetClientBounds().GetSize() : Size(800, 600);
    
    float viewX = -m_panX / m_zoom * scale + thumbX;
    float viewY = -m_panY / m_zoom * scale + thumbY;
    float viewW = parentSize.width / m_zoom * scale;
    float viewH = parentSize.height / m_zoom * scale;
    
    viewW = std::min(viewW, static_cast<float>(thumbW));
    viewH = std::min(viewH, static_cast<float>(thumbH));
    
    if (viewW > 2 && viewH > 2) {
        HPEN viewPen = CreatePen(PS_SOLID, 2, RGB(0x00, 0x78, 0xD7));
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, viewPen));
        HBRUSH nullBrush = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, nullBrush));
        Rectangle(hdc, static_cast<int>(viewX), static_cast<int>(viewY),
                  static_cast<int>(viewX + viewW), static_cast<int>(viewY + viewH));
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(viewPen);
    }
}

void NavigatorPanel::OnMouseDown(const Point& pos, MouseButton button) {
    if (button != MouseButton::Left) return;
    
    Rect client = GetClientBounds();
    int availW = client.Width() - ThumbMargin * 2;
    int availH = client.Height() - 50;
    
    float scale = GetThumbScale(availW, availH);
    int thumbW = static_cast<int>(m_canvasWidth * scale);
    int thumbH = static_cast<int>(m_canvasHeight * scale);
    int thumbX = ThumbMargin + (availW - thumbW) / 2;
    int thumbY = 28 + (availH - thumbH) / 2;
    
    if (pos.x >= thumbX && pos.x < thumbX + thumbW && pos.y >= thumbY && pos.y < thumbY + thumbH) {
        m_draggingView = true;
        // Calculate new pan based on click position
        float canvasX = (pos.x - thumbX) / scale;
        float canvasY = (pos.y - thumbY) / scale;
        
        Window* parent = GetParent();
        Size parentSize = parent ? parent->GetClientBounds().GetSize() : Size(800, 600);
        
        m_panX = parentSize.width / 2.0f - canvasX * m_zoom;
        m_panY = parentSize.height / 2.0f - canvasY * m_zoom;
        Invalidate();
    }
}

void NavigatorPanel::OnMouseMove(const Point& pos) {
    if (!m_draggingView) return;
    
    Rect client = GetClientBounds();
    int availW = client.Width() - ThumbMargin * 2;
    int availH = client.Height() - 50;
    
    float scale = GetThumbScale(availW, availH);
    int thumbW = static_cast<int>(m_canvasWidth * scale);
    int thumbH = static_cast<int>(m_canvasHeight * scale);
    int thumbX = ThumbMargin + (availW - thumbW) / 2;
    int thumbY = 28 + (availH - thumbH) / 2;
    
    float canvasX = (pos.x - thumbX) / scale;
    float canvasY = (pos.y - thumbY) / scale;
    
    Window* parent = GetParent();
    Size parentSize = parent ? parent->GetClientBounds().GetSize() : Size(800, 600);
    
    m_panX = parentSize.width / 2.0f - canvasX * m_zoom;
    m_panY = parentSize.height / 2.0f - canvasY * m_zoom;
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

void NavigatorPanel::UpdateThumbnail() {
    m_thumbnailDirty = false;
}

} // namespace UI
} // namespace VividPic
