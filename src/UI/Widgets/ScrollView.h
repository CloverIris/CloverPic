#pragma once

#include "Utils/Types.h"
#include <windows.h>

namespace VividPic {
namespace UI {

// -------------------------------------------------------------------------
// ScrollView — lightweight scroll manager (not a Window)
// Handles scroll offset, scrollbar drawing, and hit-testing.
// Owner panel calls into this from its OnPaint / mouse handlers.
// -------------------------------------------------------------------------
class ScrollView {
public:
    ScrollView();

    // Configuration
    void SetViewportSize(int width, int height);
    void SetContentHeight(int height);
    int  GetContentHeight() const { return m_contentHeight; }
    
    // Scrolling
    int  GetScrollOffset() const { return m_scrollOffset; }
    void SetScrollOffset(int offset);
    void ScrollBy(int delta);
    void ScrollToBottom();
    
    // Hit-test
    bool IsPointInScrollbar(const Rect& viewportRect, const Point& pos) const;
    bool IsPointInThumb(const Rect& viewportRect, const Point& pos) const;
    Rect GetThumbRect(const Rect& viewportRect) const;
    
    // Mouse interaction
    void BeginDrag(const Point& pos, const Rect& viewportRect);
    void UpdateDrag(const Point& pos, const Rect& viewportRect);
    void EndDrag();
    void OnMouseWheel(int delta);
    
    // Drawing
    void Draw(HDC hdc, const Rect& viewportRect);
    
    // State queries
    bool IsScrollbarVisible() const;
    bool IsDraggingThumb() const { return m_draggingThumb; }
    
    // Convert content Y to viewport Y (for painting)
    int ContentToViewport(int contentY) const { return contentY - m_scrollOffset; }
    
    // Convert viewport Y to content Y (for hit-testing)
    int ViewportToContent(int viewportY) const { return viewportY + m_scrollOffset; }
    
    // Scrollbar metrics (for owner clipping)
    int GetScrollbarWidth() const;
    Rect GetScrollbarRect(const Rect& viewportRect) const;

private:
    void ClampOffset();
    int  GetThumbHeight(const Rect& trackRect) const;
    int  GetThumbY(const Rect& trackRect) const;
    Rect GetTrackRect(const Rect& viewportRect) const;
    int  OffsetFromThumbY(int thumbY, const Rect& trackRect) const;
    
    int m_viewportWidth = 0;
    int m_viewportHeight = 0;
    int m_contentHeight = 0;
    int m_scrollOffset = 0;
    
    bool m_draggingThumb = false;
    int m_dragStartY = 0;
    int m_dragStartOffset = 0;
    
    static constexpr int ScrollbarWidthBase = 10;
};

} // namespace UI
} // namespace VividPic
