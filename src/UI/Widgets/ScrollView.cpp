#include "UI/Widgets/ScrollView.h"
#include "UI/Core/Theme.h"
#include <algorithm>

namespace VividPic {
namespace UI {

ScrollView::ScrollView() = default;

void ScrollView::SetViewportSize(int width, int height) {
    m_viewportWidth = width;
    m_viewportHeight = height;
    ClampOffset();
}

void ScrollView::SetContentHeight(int height) {
    m_contentHeight = height;
    ClampOffset();
}

void ScrollView::SetScrollOffset(int offset) {
    m_scrollOffset = offset;
    ClampOffset();
}

void ScrollView::ScrollBy(int delta) {
    m_scrollOffset += delta;
    ClampOffset();
}

void ScrollView::ScrollToBottom() {
    m_scrollOffset = m_contentHeight - m_viewportHeight;
    ClampOffset();
}

void ScrollView::ClampOffset() {
    if (m_scrollOffset < 0) m_scrollOffset = 0;
    int maxOffset = std::max(0, m_contentHeight - m_viewportHeight);
    if (m_scrollOffset > maxOffset) m_scrollOffset = maxOffset;
}

bool ScrollView::IsScrollbarVisible() const {
    return m_contentHeight > m_viewportHeight && m_viewportHeight > 0;
}

int ScrollView::GetScrollbarWidth() const {
    return Theme::GetSize(ScrollbarWidthBase);
}

Rect ScrollView::GetTrackRect(const Rect& viewportRect) const {
    int sbw = GetScrollbarWidth();
    int x = viewportRect.right - sbw;
    return Rect(x, viewportRect.top, viewportRect.right, viewportRect.bottom);
}

Rect ScrollView::GetScrollbarRect(const Rect& viewportRect) const {
    return GetTrackRect(viewportRect);
}

int ScrollView::GetThumbHeight(const Rect& trackRect) const {
    if (!IsScrollbarVisible()) return 0;
    int trackH = trackRect.Height();
    float ratio = static_cast<float>(m_viewportHeight) / m_contentHeight;
    int thumbH = static_cast<int>(trackH * ratio);
    int minThumb = Theme::GetSize(16);
    if (thumbH < minThumb) thumbH = minThumb;
    if (thumbH > trackH) thumbH = trackH;
    return thumbH;
}

int ScrollView::GetThumbY(const Rect& trackRect) const {
    if (!IsScrollbarVisible()) return trackRect.top;
    int trackH = trackRect.Height();
    int thumbH = GetThumbHeight(trackRect);
    int maxOffset = std::max(1, m_contentHeight - m_viewportHeight);
    float scrollRatio = static_cast<float>(m_scrollOffset) / maxOffset;
    int maxThumbY = trackH - thumbH;
    return trackRect.top + static_cast<int>(scrollRatio * maxThumbY);
}

Rect ScrollView::GetThumbRect(const Rect& trackRect) const {
    if (!IsScrollbarVisible()) return Rect();
    int thumbH = GetThumbHeight(trackRect);
    int thumbY = GetThumbY(trackRect);
    return Rect(trackRect.left, thumbY, trackRect.right, thumbY + thumbH);
}

int ScrollView::OffsetFromThumbY(int thumbY, const Rect& trackRect) const {
    int trackH = trackRect.Height();
    int thumbH = GetThumbHeight(trackRect);
    int maxThumbY = trackH - thumbH;
    if (maxThumbY <= 0) return 0;
    int relY = thumbY - trackRect.top;
    if (relY < 0) relY = 0;
    if (relY > maxThumbY) relY = maxThumbY;
    float ratio = static_cast<float>(relY) / maxThumbY;
    int maxOffset = std::max(1, m_contentHeight - m_viewportHeight);
    return static_cast<int>(ratio * maxOffset);
}

bool ScrollView::IsPointInScrollbar(const Rect& viewportRect, const Point& pos) const {
    if (!IsScrollbarVisible()) return false;
    Rect track = GetTrackRect(viewportRect);
    return track.Contains(pos);
}

bool ScrollView::IsPointInThumb(const Rect& viewportRect, const Point& pos) const {
    if (!IsScrollbarVisible()) return false;
    Rect track = GetTrackRect(viewportRect);
    Rect thumb = GetThumbRect(track);
    return thumb.Contains(pos);
}

void ScrollView::BeginDrag(const Point& pos, const Rect& viewportRect) {
    if (!IsScrollbarVisible()) return;
    Rect track = GetTrackRect(viewportRect);
    Rect thumb = GetThumbRect(track);
    m_draggingThumb = true;
    m_dragStartY = pos.y;
    m_dragStartOffset = m_scrollOffset;
    (void)thumb;
}

void ScrollView::UpdateDrag(const Point& pos, const Rect& viewportRect) {
    if (!m_draggingThumb) return;
    Rect track = GetTrackRect(viewportRect);
    int deltaY = pos.y - m_dragStartY;
    int trackH = track.Height();
    int thumbH = GetThumbHeight(track);
    int maxThumbY = trackH - thumbH;
    if (maxThumbY <= 0) {
        m_scrollOffset = 0;
        return;
    }
    int newThumbY = GetThumbY(track) + deltaY;
    m_scrollOffset = OffsetFromThumbY(newThumbY, track);
    ClampOffset();
    // Update drag start so next delta is relative
    m_dragStartY = pos.y;
    m_dragStartOffset = m_scrollOffset;
}

void ScrollView::EndDrag() {
    m_draggingThumb = false;
}

void ScrollView::OnMouseWheel(int delta) {
    if (!IsScrollbarVisible()) return;
    int linesPerWheel = Theme::GetSize(48); // approx one layer item height
    ScrollBy(-delta * linesPerWheel / WHEEL_DELTA);
}

void ScrollView::Draw(HDC hdc, const Rect& viewportRect) {
    if (!IsScrollbarVisible()) return;
    
    Rect track = GetTrackRect(viewportRect);
    
    // Track background
    HBRUSH trackBrush = Theme::CachedBrush(Theme::BackgroundDark);
    RECT trackRc = track.ToWin32Rect();
    FillRect(hdc, &trackRc, trackBrush);
    
    // Thumb
    Rect thumb = GetThumbRect(track);
    uint32_t thumbColor = m_draggingThumb ? Theme::ButtonHover : Theme::ButtonDefault;
    HBRUSH thumbBrush = Theme::CachedBrush(thumbColor);
    RECT thumbRc = thumb.ToWin32Rect();
    FillRect(hdc, &thumbRc, thumbBrush);
    
    // Border
    HPEN borderPen = Theme::Pen(Theme::BorderDark);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
    MoveToEx(hdc, track.left, track.top, nullptr);
    LineTo(hdc, track.left, track.bottom);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);
}

} // namespace UI
} // namespace VividPic
