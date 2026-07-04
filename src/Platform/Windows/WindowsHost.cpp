#include "Platform/Windows/WindowsHost.h"
#include "Platform/Windows/WindowsCoreServices.h"
#include "Platform/Windows/RenderBackend.h"
#include <gdiplus.h>
#include <shellscalingapi.h>
#include <windowsx.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>

namespace CloverPic::Platform::Windows {

namespace {

uint64_t NowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

} // namespace

WindowsHost::WindowsHost() = default;

WindowsHost::~WindowsHost() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    Render::RenderBackend::GetInstance().Shutdown();
    CoUninitialize();
}

bool WindowsHost::Initialize(HINSTANCE instance) {
    m_instance = instance;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Render::RenderBackend::GetInstance().Initialize();

    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE)) {
            SetProcessDPIAware();
        }
    }

    RegisterClass();

    RECT workArea = {};
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0)) {
        workArea = { 0, 0, 1280, 960 };
    }
    const int workW = std::max<LONG>(800, workArea.right - workArea.left);
    const int workH = std::max<LONG>(600, workArea.bottom - workArea.top);
    int clientH = std::clamp(static_cast<int>(workH * 0.76f), 720, 1080);
    int clientW = clientH * 4 / 3;
    const int maxW = static_cast<int>(workW * 0.86f);
    if (clientW > maxW) {
        clientW = std::max(900, maxW);
        clientH = clientW * 3 / 4;
    }

    RECT rc = { 0, 0, clientW, clientH };
    AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, FALSE, 0);
    const int windowW = rc.right - rc.left;
    const int windowH = rc.bottom - rc.top;
    const int windowX = workArea.left + std::max(0, (workW - windowW) / 2);
    const int windowY = workArea.top + std::max(0, (workH - windowH) / 2);
    m_hwnd = CreateWindowExW(
        0,
        ClassName,
        L"CloverPic",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        windowX,
        windowY,
        windowW,
        windowH,
        nullptr,
        nullptr,
        m_instance,
        this);

    if (!m_hwnd) {
        return false;
    }

    UpdateDpiFromWindow();
    RegisterWindowsCoreServices(m_hwnd);
    m_runtime.Initialize();
    ResizeFromClient();
    SetTimer(m_hwnd, FrameTimerId, 16, nullptr);
    RequestFrame();
    return true;
}

int WindowsHost::Run() {
    MSG msg = {};
    while (m_running && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

void WindowsHost::RequestFrame() {
    if (m_hwnd) {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void WindowsHost::Present(const Core::RgbaFrame& frame, const std::vector<Rect>& dirtyRects) {
    if (!m_hwnd || frame.IsEmpty()) {
        return;
    }

    HDC hdc = GetDC(m_hwnd);
    PresentToHdc(hdc, frame, dirtyRects);
    ReleaseDC(m_hwnd, hdc);
}

void WindowsHost::PresentToHdc(HDC hdc, const Core::RgbaFrame& frame, const std::vector<Rect>& dirtyRects) {
    if (!hdc || frame.IsEmpty()) {
        return;
    }

    const auto blitRect = [&](const Rect& rect) {
        StretchDIBits(
            hdc,
            rect.left,
            rect.top,
            rect.Width(),
            rect.Height(),
            rect.left,
            rect.top,
            rect.Width(),
            rect.Height(),
            frame.pixels.data(),
            &m_bitmapInfo,
            DIB_RGB_COLORS,
            SRCCOPY);
    };

    if (dirtyRects.empty()) {
        blitRect(Rect(0, 0, static_cast<int32_t>(frame.width), static_cast<int32_t>(frame.height)));
    } else {
        for (const auto& rect : dirtyRects) {
            if (rect.Width() > 0 && rect.Height() > 0) {
                blitRect(rect);
            }
        }
    }
}

LRESULT CALLBACK WindowsHost::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WindowsHost* host = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        host = static_cast<WindowsHost*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(host));
        host->m_hwnd = hwnd;
    } else {
        host = reinterpret_cast<WindowsHost*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    return host ? host->HandleMessage(msg, wParam, lParam) : DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT WindowsHost::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE:
            ResizeFromClient();
            RequestFrame();
            return 0;

        case WM_DPICHANGED: {
            m_dpiScale = std::max(1.0f, static_cast<float>(HIWORD(wParam)) / 96.0f);
            if (auto* suggested = reinterpret_cast<RECT*>(lParam)) {
                SetWindowPos(m_hwnd, nullptr, suggested->left, suggested->top,
                             suggested->right - suggested->left,
                             suggested->bottom - suggested->top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
            }
            ResizeFromClient();
            RequestFrame();
            return 0;
        }

        case WM_PAINT:
            Paint();
            return 0;

        case WM_TIMER:
            if (wParam == FrameTimerId && m_runtime.NeedsFrame(NowMs())) {
                RequestFrame();
                return 0;
            }
            break;

        case WM_LBUTTONDOWN:
            SetCapture(m_hwnd);
            m_runtime.HandlePointer(MakeMouseEvent(Input::PointerAction::Down, lParam, MouseButton::Left));
            RequestFrame();
            return 0;
        case WM_LBUTTONUP:
            ReleaseCapture();
            m_runtime.HandlePointer(MakeMouseEvent(Input::PointerAction::Up, lParam, MouseButton::Left));
            RequestFrame();
            return 0;
        case WM_RBUTTONDOWN:
            m_runtime.HandlePointer(MakeMouseEvent(Input::PointerAction::Down, lParam, MouseButton::Right));
            RequestFrame();
            return 0;
        case WM_RBUTTONUP:
            m_runtime.HandlePointer(MakeMouseEvent(Input::PointerAction::Up, lParam, MouseButton::Right));
            RequestFrame();
            return 0;
        case WM_MBUTTONDOWN:
            SetCapture(m_hwnd);
            m_runtime.HandlePointer(MakeMouseEvent(Input::PointerAction::Down, lParam, MouseButton::Middle));
            RequestFrame();
            return 0;
        case WM_MBUTTONUP:
            ReleaseCapture();
            m_runtime.HandlePointer(MakeMouseEvent(Input::PointerAction::Up, lParam, MouseButton::Middle));
            RequestFrame();
            return 0;
        case WM_MOUSEMOVE:
            m_runtime.HandlePointer(MakeMouseEvent(Input::PointerAction::Move, lParam, MouseButton::None));
            RequestFrame();
            return 0;
        case WM_MOUSEWHEEL: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(m_hwnd, &pt);
            m_runtime.HandleWheel(GET_WHEEL_DELTA_WPARAM(wParam), Point(pt.x, pt.y));
            RequestFrame();
            return 0;
        }

        case WM_POINTERDOWN:
        case WM_POINTERUPDATE:
        case WM_POINTERUP: {
            Input::PointerEvent event;
            if (TranslatePointerPen(msg, wParam, event)) {
                m_runtime.HandlePointer(event);
                RequestFrame();
                return 0;
            }
            break;
        }

        case WM_KEYDOWN:
            m_runtime.HandleKey(MakeKeyEvent(Input::KeyAction::Down, wParam, lParam));
            if (m_runtime.WantsQuit()) RequestQuit();
            RequestFrame();
            return 0;
        case WM_KEYUP:
            m_runtime.HandleKey(MakeKeyEvent(Input::KeyAction::Up, wParam, lParam));
            RequestFrame();
            return 0;

        case WM_CLOSE:
            RequestQuit();
            return 0;
        case WM_DESTROY:
            m_running = false;
            PostQuitMessage(0);
            return 0;
        case WM_ERASEBKGND:
            return 1;
    }

    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

void WindowsHost::RegisterClass() {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = m_instance;
    wc.lpszClassName = ClassName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    RegisterClassExW(&wc);
}

void WindowsHost::ResizeFromClient() {
    if (!m_hwnd) return;
    RECT rc = {};
    GetClientRect(m_hwnd, &rc);
    m_viewport = Size(std::max(1L, rc.right - rc.left), std::max(1L, rc.bottom - rc.top));
    m_runtime.Resize(static_cast<uint32_t>(m_viewport.width), static_cast<uint32_t>(m_viewport.height), m_dpiScale);

    m_bitmapInfo = {};
    m_bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    m_bitmapInfo.bmiHeader.biWidth = m_viewport.width;
    m_bitmapInfo.bmiHeader.biHeight = -m_viewport.height;
    m_bitmapInfo.bmiHeader.biPlanes = 1;
    m_bitmapInfo.bmiHeader.biBitCount = 32;
    m_bitmapInfo.bmiHeader.biCompression = BI_RGB;
}

void WindowsHost::Paint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hwnd, &ps);

    std::vector<Rect> dirtyRects;
    const auto& frame = m_runtime.Render(NowMs(), dirtyRects);
    PresentToHdc(hdc, frame, dirtyRects);
    EndPaint(m_hwnd, &ps);
}

Input::PointerEvent WindowsHost::MakeMouseEvent(Input::PointerAction action, LPARAM lParam, MouseButton button) const {
    Input::PointerEvent event;
    event.device = Input::PointerDeviceType::Mouse;
    event.action = action;
    event.position = Point(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
    event.button = button;
    event.pressure = 1.0f;
    event.inContact = action != Input::PointerAction::Up &&
        (((GetKeyState(VK_LBUTTON) & 0x8000) != 0) || ((GetKeyState(VK_MBUTTON) & 0x8000) != 0));
    event.modifiers = CurrentModifiers();
    return event;
}

Input::KeyEvent WindowsHost::MakeKeyEvent(Input::KeyAction action, WPARAM wParam, LPARAM lParam) const {
    Input::KeyEvent event;
    event.action = action;
    event.keyCode = static_cast<uint32_t>(wParam);
    event.modifiers = CurrentModifiers();
    event.isRepeat = (lParam & (1 << 30)) != 0;
    return event;
}

uint32_t WindowsHost::CurrentModifiers() const {
    uint32_t modifiers = Input::ModifierNone;
    if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) modifiers |= Input::ModifierShift;
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) modifiers |= Input::ModifierControl;
    if ((GetKeyState(VK_MENU) & 0x8000) != 0) modifiers |= Input::ModifierAlt;
    return modifiers;
}

bool WindowsHost::TranslatePointerPen(UINT msg, WPARAM wParam, Input::PointerEvent& outEvent) const {
    POINTER_INPUT_TYPE pointerType;
    if (!GetPointerType(GET_POINTERID_WPARAM(wParam), &pointerType)) {
        return false;
    }
    if (pointerType != PT_PEN && pointerType != PT_TOUCH) {
        return false;
    }

    if (pointerType == PT_TOUCH) {
        POINTER_INFO pointerInfo = {};
        if (!GetPointerInfo(GET_POINTERID_WPARAM(wParam), &pointerInfo)) {
            return false;
        }
        POINT pt = { pointerInfo.ptPixelLocation.x, pointerInfo.ptPixelLocation.y };
        ScreenToClient(m_hwnd, &pt);
        outEvent.device = Input::PointerDeviceType::Touch;
        outEvent.action = msg == WM_POINTERDOWN
            ? Input::PointerAction::Down
            : (msg == WM_POINTERUP ? Input::PointerAction::Up : Input::PointerAction::Move);
        outEvent.position = Point(pt.x, pt.y);
        outEvent.button = MouseButton::Left;
        outEvent.pressure = 1.0f;
        outEvent.inContact = (pointerInfo.pointerFlags & POINTER_FLAG_INCONTACT) != 0;
        outEvent.modifiers = CurrentModifiers();
        return true;
    }

    POINTER_PEN_INFO penInfo = {};
    if (!GetPointerPenInfo(GET_POINTERID_WPARAM(wParam), &penInfo)) {
        return false;
    }

    POINT pt = { penInfo.pointerInfo.ptPixelLocation.x, penInfo.pointerInfo.ptPixelLocation.y };
    ScreenToClient(m_hwnd, &pt);
    outEvent.device = Input::PointerDeviceType::Pen;
    outEvent.action = msg == WM_POINTERDOWN
        ? Input::PointerAction::Down
        : (msg == WM_POINTERUP ? Input::PointerAction::Up : Input::PointerAction::Move);
    outEvent.position = Point(pt.x, pt.y);
    outEvent.button = MouseButton::Left;
    outEvent.pressure = std::clamp(static_cast<float>(penInfo.pressure) / 1024.0f, 0.0f, 1.0f);
    outEvent.tiltX = static_cast<float>(penInfo.tiltX) / 10.0f;
    outEvent.tiltY = static_cast<float>(penInfo.tiltY) / 10.0f;
    outEvent.rotation = static_cast<float>(penInfo.rotation);
    outEvent.inContact = (penInfo.pointerInfo.pointerFlags & POINTER_FLAG_INCONTACT) != 0;
    outEvent.eraser = (penInfo.penFlags & PEN_FLAG_ERASER) != 0;
    outEvent.modifiers = CurrentModifiers();
    return true;
}

void WindowsHost::UpdateDpiFromWindow() {
    UINT dpi = 96;
    if (m_hwnd) {
        if (HMODULE hUser32 = GetModuleHandleW(L"user32.dll")) {
            using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
            FARPROC proc = GetProcAddress(hUser32, "GetDpiForWindow");
            GetDpiForWindowFn pfn = nullptr;
            static_assert(sizeof(proc) == sizeof(pfn));
            std::memcpy(&pfn, &proc, sizeof(pfn));
            if (pfn) dpi = pfn(m_hwnd);
        }
    }
    m_dpiScale = std::max(1.0f, dpi / 96.0f);
}

} // namespace CloverPic::Platform::Windows
