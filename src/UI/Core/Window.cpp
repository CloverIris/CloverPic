#include "UI/Core/Window.h"
#include "UI/Core/Theme.h"
#include <algorithm>
#include <windowsx.h>

namespace VividPic {
namespace UI {

bool Window::s_classRegistered = false;
uint32_t Window::s_windowId = 0;

Window::Window() {
    m_className = BaseClassName;
}

Window::~Window() {
    Destroy();
}

void Window::RegisterWindowClass() {
    if (s_classRegistered) return;
    
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = BaseClassName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr; // We handle background ourselves
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    
    RegisterClassExW(&wc);
    s_classRegistered = true;
}

bool Window::Create(const String& title, const Rect& bounds, Window* parent, DWORD style, DWORD exStyle) {
    RegisterWindowClass();
    
    if (style == 0) style = GetDefaultStyle();
    if (exStyle == 0) exStyle = GetDefaultExStyle();
    
    HWND parentHwnd = parent ? parent->m_hwnd : nullptr;
    
    m_hwnd = CreateWindowExW(
        exStyle,
        m_className.c_str(),
        title.c_str(),
        style,
        bounds.left, bounds.top,
        bounds.Width(), bounds.Height(),
        parentHwnd,
        nullptr,
        GetModuleHandle(nullptr),
        this
    );
    
    if (!m_hwnd) return false;
    
    m_parent = parent;
    if (parent) {
        parent->AddChild(this);
    }
    
    // Query DPI scale for this window
    UINT dpi = 96;
    if (HMODULE hUser32 = GetModuleHandleW(L"user32.dll")) {
        using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
        auto pfn = reinterpret_cast<GetDpiForWindowFn>(GetProcAddress(hUser32, "GetDpiForWindow"));
        if (pfn) {
            dpi = pfn(m_hwnd);
        }
    }
    m_dpiScale = static_cast<float>(dpi) / 96.0f;
    
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    return true;
}

int Window::ScaleSize(int base) {
    return static_cast<int>(base * UI::Theme::Scale);
}

int Window::ScaleFont(int base) {
    return static_cast<int>(base * UI::Theme::Scale);
}

void Window::Destroy() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void Window::AddChild(Window* child) {
    if (std::find(m_children.begin(), m_children.end(), child) == m_children.end()) {
        m_children.push_back(child);
    }
}

void Window::RemoveChild(Window* child) {
    auto it = std::find(m_children.begin(), m_children.end(), child);
    if (it != m_children.end()) {
        m_children.erase(it);
    }
}

void Window::SetBounds(const Rect& bounds) {
    if (m_hwnd) {
        SetWindowPos(m_hwnd, nullptr, bounds.left, bounds.top, 
                     bounds.Width(), bounds.Height(), SWP_NOZORDER);
    }
}

Rect Window::GetBounds() const {
    if (!m_hwnd) return Rect();
    RECT rc;
    GetWindowRect(m_hwnd, &rc);
    if (m_parent) {
        ScreenToClient(m_parent->m_hwnd, reinterpret_cast<LPPOINT>(&rc.left));
        ScreenToClient(m_parent->m_hwnd, reinterpret_cast<LPPOINT>(&rc.right));
    }
    return Rect(rc.left, rc.top, rc.right, rc.bottom);
}

Rect Window::GetClientBounds() const {
    if (!m_hwnd) return Rect();
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    return Rect(rc.left, rc.top, rc.right, rc.bottom);
}

void Window::SetVisible(bool visible) {
    if (m_hwnd) {
        ShowWindow(m_hwnd, visible ? SW_SHOW : SW_HIDE);
    }
}

bool Window::IsVisible() const {
    if (!m_hwnd) return false;
    return IsWindowVisible(m_hwnd) == TRUE;
}

void Window::Invalidate() {
    if (m_hwnd) {
        ::InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void Window::InvalidateRect(const Rect& rect) {
    if (m_hwnd) {
        RECT rc = rect.ToWin32Rect();
        ::InvalidateRect(m_hwnd, &rc, FALSE);
    }
}

void Window::SetFocus() {
    if (m_hwnd) {
        ::SetFocus(m_hwnd);
    }
}

bool Window::HasFocus() const {
    return GetFocus() == m_hwnd;
}

void Window::SetText(const String& text) {
    if (m_hwnd) {
        SetWindowTextW(m_hwnd, text.c_str());
    }
}

String Window::GetText() const {
    if (!m_hwnd) return String();
    int len = GetWindowTextLengthW(m_hwnd);
    String text(len, L'\0');
    GetWindowTextW(m_hwnd, text.data(), len + 1);
    text.resize(len);
    return text;
}

// Default event handlers (no-op)
void Window::OnPaint(HDC hdc, const Rect& clip) {}
void Window::OnMouseMove(const Point& pos) {}
void Window::OnMouseDown(const Point& pos, MouseButton button) {}
void Window::OnMouseUp(const Point& pos, MouseButton button) {}
void Window::OnMouseDoubleClick(const Point& pos, MouseButton button) {}
void Window::OnMouseEnter() {}
void Window::OnMouseLeave() {}
void Window::OnSize(const Size& newSize) {}
void Window::OnKeyDown(uint32_t keyCode) {}
void Window::OnKeyUp(uint32_t keyCode) {}
void Window::OnChar(wchar_t ch) {}
void Window::OnCommand(uint32_t id) {}
void Window::OnNotify(NMHDR* nmhdr) {}
bool Window::OnCreate() { return true; }
void Window::OnDestroy() {}

LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Window* window = nullptr;
    
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = static_cast<Window*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        window->m_hwnd = hwnd;
    } else {
        window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    
    if (window) {
        return window->HandleMessage(msg, wParam, lParam);
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT Window::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            if (!OnCreate()) {
                return -1;
            }
            return 0;
        }
        
        case WM_DESTROY: {
            OnDestroy();
            // If this is a top-level window and no other visible top-level
            // windows remain in this thread, post WM_QUIT to end the app.
            if (!m_parent && m_hwnd) {
                bool hasOtherVisible = false;
                EnumThreadWindows(GetCurrentThreadId(),
                    [](HWND hwnd, LPARAM lParam) -> BOOL {
                        if (IsWindowVisible(hwnd) && ::GetParent(hwnd) == nullptr) {
                            *reinterpret_cast<bool*>(lParam) = true;
                            return FALSE;
                        }
                        return TRUE;
                    }, reinterpret_cast<LPARAM>(&hasOtherVisible));
                if (!hasOtherVisible) {
                    PostQuitMessage(0);
                }
            }
            return 0;
        }
        
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(m_hwnd, &ps);
            Rect clipRect(ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom);
            OnPaint(hdc, clipRect);
            EndPaint(m_hwnd, &ps);
            return 0;
        }
        
        case WM_SIZE: {
            Size newSize(LOWORD(lParam), HIWORD(lParam));
            OnSize(newSize);
            return 0;
        }
        
        case WM_MOUSEMOVE: {
            Point pos(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            OnMouseMove(pos);
            
            if (!m_mouseTracking) {
                TRACKMOUSEEVENT tme = {};
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = m_hwnd;
                TrackMouseEvent(&tme);
                m_mouseTracking = true;
                
                if (!m_mouseInside) {
                    m_mouseInside = true;
                    OnMouseEnter();
                }
            }
            return 0;
        }
        
        case WM_MOUSELEAVE: {
            m_mouseTracking = false;
            m_mouseInside = false;
            OnMouseLeave();
            return 0;
        }
        
        case WM_LBUTTONDOWN: {
            if (GetFocus() != m_hwnd) {
                ::SetFocus(m_hwnd);
            }
            Point pos(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            OnMouseDown(pos, MouseButton::Left);
            return 0;
        }
        
        case WM_LBUTTONUP: {
            Point pos(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            OnMouseUp(pos, MouseButton::Left);
            return 0;
        }
        
        case WM_LBUTTONDBLCLK: {
            Point pos(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            OnMouseDoubleClick(pos, MouseButton::Left);
            return 0;
        }
        
        case WM_RBUTTONDOWN: {
            Point pos(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            OnMouseDown(pos, MouseButton::Right);
            return 0;
        }
        
        case WM_RBUTTONUP: {
            Point pos(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            OnMouseUp(pos, MouseButton::Right);
            return 0;
        }
        
        case WM_KEYDOWN: {
            OnKeyDown(static_cast<uint32_t>(wParam));
            return 0;
        }
        
        case WM_KEYUP: {
            OnKeyUp(static_cast<uint32_t>(wParam));
            return 0;
        }
        
        case WM_CHAR: {
            OnChar(static_cast<wchar_t>(wParam));
            return 0;
        }
        
        case WM_COMMAND: {
            OnCommand(static_cast<uint32_t>(LOWORD(wParam)));
            return 0;
        }
        
        case WM_NOTIFY: {
            OnNotify(reinterpret_cast<NMHDR*>(lParam));
            return 0;
        }
        
        case WM_ERASEBKGND: {
            return 1; // Prevent flicker
        }
    }
    
    return DefWindowProc(m_hwnd, msg, wParam, lParam);
}

} // namespace UI
} // namespace VividPic
