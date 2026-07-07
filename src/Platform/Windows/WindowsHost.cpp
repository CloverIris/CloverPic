#include "Platform/Windows/WindowsHost.h"
#include "Platform/Windows/WindowsCoreServices.h"
#include "Platform/Windows/RenderBackend.h"
#include <shellscalingapi.h>
#include <windowsx.h>
#include <algorithm>
#include <chrono>
#include <cstring>

namespace CloverPic::Platform::Windows {

namespace {

uint64_t NowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

Size WorkAreaSize() {
    RECT workArea = {};
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0)) {
        return Size(1280, 960);
    }
    return Size(std::max<LONG>(800, workArea.right - workArea.left), std::max<LONG>(600, workArea.bottom - workArea.top));
}

Point WorkAreaOrigin() {
    RECT workArea = {};
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0)) {
        return Point(0, 0);
    }
    return Point(workArea.left, workArea.top);
}

void CenterWindow(HWND hwnd, int clientWidth, int clientHeight, bool borderless) {
    RECT rc = {0, 0, clientWidth, clientHeight};
    if (!borderless) {
        AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, FALSE, 0);
    }
    const int windowW = rc.right - rc.left;
    const int windowH = rc.bottom - rc.top;
    const Size work = WorkAreaSize();
    const Point origin = WorkAreaOrigin();
    SetWindowPos(hwnd, nullptr,
                 origin.x + std::max(0, (work.width - windowW) / 2),
                 origin.y + std::max(0, (work.height - windowH) / 2),
                 windowW, windowH,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

} // namespace

WindowsSurfaceWindow::WindowsSurfaceWindow(WindowsHost& owner, SurfaceRole role, Core::RuntimeSurface& runtime)
    : m_owner(owner), m_role(role), m_runtime(runtime) {}

WindowsSurfaceWindow::~WindowsSurfaceWindow() {
    Destroy();
}

bool WindowsSurfaceWindow::Create(HINSTANCE instance, const wchar_t* title, int clientWidth, int clientHeight,
                                  bool visible, HWND owner, bool borderless) {
    m_instance = instance;
    m_borderless = borderless;
    RegisterClass(instance);
    RECT rc = {0, 0, clientWidth, clientHeight};
    const DWORD style = borderless ? WS_POPUP : WS_OVERLAPPEDWINDOW;
    const DWORD exStyle = borderless ? WS_EX_TOOLWINDOW : 0;
    if (!borderless) {
        AdjustWindowRectEx(&rc, style, FALSE, exStyle);
    }
    const int windowW = rc.right - rc.left;
    const int windowH = rc.bottom - rc.top;
    const Point origin = WorkAreaOrigin();
    const Size work = WorkAreaSize();
    m_hwnd = CreateWindowExW(
        exStyle,
        ClassName,
        title,
        style | (visible ? WS_VISIBLE : 0),
        origin.x + std::max(0, (work.width - windowW) / 2),
        origin.y + std::max(0, (work.height - windowH) / 2),
        windowW,
        windowH,
        owner,
        nullptr,
        m_instance,
        this);
    if (!m_hwnd) return false;

    CenterWindow(m_hwnd, clientWidth, clientHeight, borderless);
    UpdateDpiFromWindow();
    m_runtime.Initialize();
    ResizeFromClient();
    SetTimer(m_hwnd, FrameTimerId, 16, nullptr);
    RequestFrame();
    return true;
}

void WindowsSurfaceWindow::Destroy() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
    }
}

void WindowsSurfaceWindow::Show(int command) {
    if (!m_hwnd) return;
    ShowWindow(m_hwnd, command);
    UpdateWindow(m_hwnd);
    RequestFrame();
}

void WindowsSurfaceWindow::Hide() {
    if (m_hwnd) ShowWindow(m_hwnd, SW_HIDE);
}

void WindowsSurfaceWindow::RequestFrame() {
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

LRESULT CALLBACK WindowsSurfaceWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WindowsSurfaceWindow* surface = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        surface = static_cast<WindowsSurfaceWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(surface));
        surface->m_hwnd = hwnd;
    } else {
        surface = reinterpret_cast<WindowsSurfaceWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    return surface ? surface->HandleMessage(msg, wParam, lParam) : DefWindowProcW(hwnd, msg, wParam, lParam);
}

void WindowsSurfaceWindow::RegisterClass(HINSTANCE instance) {
    static bool registered = false;
    if (registered) return;
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = ClassName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    RegisterClassExW(&wc);
    registered = true;
}

LRESULT WindowsSurfaceWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_NCHITTEST:
            if (m_borderless) {
                POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                ScreenToClient(m_hwnd, &pt);
                if (m_runtime.IsWindowDragRegion(Point(pt.x, pt.y))) {
                    return HTCAPTION;
                }
                return HTCLIENT;
            }
            break;
        case WM_SETFOCUS:
            m_owner.SetActiveDialogOwner(m_hwnd);
            return 0;
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
            CheckRuntimeCloseRequest();
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
            CheckRuntimeCloseRequest();
            RequestFrame();
            return 0;
        case WM_LBUTTONUP:
            ReleaseCapture();
            m_runtime.HandlePointer(MakeMouseEvent(Input::PointerAction::Up, lParam, MouseButton::Left));
            CheckRuntimeCloseRequest();
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
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
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
            CheckRuntimeCloseRequest();
            RequestFrame();
            return 0;
        case WM_KEYUP:
            m_runtime.HandleKey(MakeKeyEvent(Input::KeyAction::Up, wParam, lParam));
            RequestFrame();
            return 0;
        case WM_CLOSE:
            if (m_role == SurfaceRole::Home) {
                m_owner.OnProgramManagerCloseRequested();
            } else {
                m_owner.OnWorkspaceCloseRequested();
            }
            return 0;
        case WM_DESTROY:
            m_closed = true;
            KillTimer(m_hwnd, FrameTimerId);
            m_hwnd = nullptr;
            m_owner.OnSurfaceDestroyed(m_role);
            return 0;
        case WM_ERASEBKGND:
            return 1;
    }
    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

void WindowsSurfaceWindow::ResizeFromClient() {
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

void WindowsSurfaceWindow::Paint() {
    if (!m_hwnd) return;
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hwnd, &ps);
    std::vector<Rect> dirtyRects;
    const auto& frame = m_runtime.Render(NowMs(), dirtyRects);
    PresentToHdc(hdc, frame, dirtyRects);
    EndPaint(m_hwnd, &ps);
}

void WindowsSurfaceWindow::PresentToHdc(HDC hdc, const Core::RgbaFrame& frame, const std::vector<Rect>& dirtyRects) {
    if (!hdc || frame.IsEmpty()) return;
    const auto blitRect = [&](const Rect& rect) {
        StretchDIBits(hdc, rect.left, rect.top, rect.Width(), rect.Height(),
                      rect.left, rect.top, rect.Width(), rect.Height(),
                      frame.pixels.data(), &m_bitmapInfo, DIB_RGB_COLORS, SRCCOPY);
    };
    if (m_role == SurfaceRole::Workspace || dirtyRects.empty()) {
        blitRect(Rect(0, 0, static_cast<int32_t>(frame.width), static_cast<int32_t>(frame.height)));
    } else {
        for (const auto& rect : dirtyRects) {
            if (rect.Width() > 0 && rect.Height() > 0) blitRect(rect);
        }
    }
}

Input::PointerEvent WindowsSurfaceWindow::MakeMouseEvent(Input::PointerAction action, LPARAM lParam, MouseButton button) const {
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

Input::KeyEvent WindowsSurfaceWindow::MakeKeyEvent(Input::KeyAction action, WPARAM wParam, LPARAM lParam) const {
    Input::KeyEvent event;
    event.action = action;
    event.keyCode = static_cast<uint32_t>(wParam);
    event.modifiers = CurrentModifiers();
    event.isRepeat = (lParam & (1 << 30)) != 0;
    return event;
}

uint32_t WindowsSurfaceWindow::CurrentModifiers() const {
    uint32_t modifiers = Input::ModifierNone;
    if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) modifiers |= Input::ModifierShift;
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) modifiers |= Input::ModifierControl;
    if ((GetKeyState(VK_MENU) & 0x8000) != 0) modifiers |= Input::ModifierAlt;
    return modifiers;
}

bool WindowsSurfaceWindow::TranslatePointerPen(UINT msg, WPARAM wParam, Input::PointerEvent& outEvent) const {
    POINTER_INPUT_TYPE pointerType;
    if (!GetPointerType(GET_POINTERID_WPARAM(wParam), &pointerType)) return false;
    if (pointerType != PT_PEN && pointerType != PT_TOUCH) return false;

    if (pointerType == PT_TOUCH) {
        POINTER_INFO pointerInfo = {};
        if (!GetPointerInfo(GET_POINTERID_WPARAM(wParam), &pointerInfo)) return false;
        POINT pt = {pointerInfo.ptPixelLocation.x, pointerInfo.ptPixelLocation.y};
        ScreenToClient(m_hwnd, &pt);
        outEvent.device = Input::PointerDeviceType::Touch;
        outEvent.action = msg == WM_POINTERDOWN ? Input::PointerAction::Down : (msg == WM_POINTERUP ? Input::PointerAction::Up : Input::PointerAction::Move);
        outEvent.position = Point(pt.x, pt.y);
        outEvent.button = MouseButton::Left;
        outEvent.pressure = 1.0f;
        outEvent.inContact = (pointerInfo.pointerFlags & POINTER_FLAG_INCONTACT) != 0;
        outEvent.modifiers = CurrentModifiers();
        return true;
    }

    POINTER_PEN_INFO penInfo = {};
    if (!GetPointerPenInfo(GET_POINTERID_WPARAM(wParam), &penInfo)) return false;
    POINT pt = {penInfo.pointerInfo.ptPixelLocation.x, penInfo.pointerInfo.ptPixelLocation.y};
    ScreenToClient(m_hwnd, &pt);
    outEvent.device = Input::PointerDeviceType::Pen;
    outEvent.action = msg == WM_POINTERDOWN ? Input::PointerAction::Down : (msg == WM_POINTERUP ? Input::PointerAction::Up : Input::PointerAction::Move);
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

void WindowsSurfaceWindow::UpdateDpiFromWindow() {
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

void WindowsSurfaceWindow::CheckRuntimeCloseRequest() {
    if (!m_runtime.WantsQuit()) return;
    if (m_role == SurfaceRole::Home) {
        m_owner.OnProgramManagerCloseRequested();
    } else {
        m_owner.OnWorkspaceCloseRequested();
    }
}

WindowsHost::WindowsHost() = default;

WindowsHost::~WindowsHost() {
    m_workspaceWindow.reset();
    m_homeWindow.reset();
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
    RegisterWindowsCoreServices([this]() { return GetActiveDialogOwner(); });
    CreateWorkspaceWindow();
    CreateHomeWindow();
    return m_workspaceWindow && m_workspaceWindow->GetHwnd() && m_homeWindow && m_homeWindow->GetHwnd();
}

int WindowsHost::Run() {
    MSG msg = {};
    while (m_running && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        PumpRuntimeRequests();
    }
    return static_cast<int>(msg.wParam);
}

HWND WindowsHost::GetActiveDialogOwner() const {
    if (m_activeDialogOwner && IsWindow(m_activeDialogOwner)) return m_activeDialogOwner;
    if (m_workspaceWindow && m_workspaceWindow->GetHwnd()) return m_workspaceWindow->GetHwnd();
    return m_homeWindow ? m_homeWindow->GetHwnd() : nullptr;
}

void WindowsHost::SetActiveDialogOwner(HWND hwnd) {
    m_activeDialogOwner = hwnd;
}

void WindowsHost::RequestQuit() {
    m_running = false;
    PostQuitMessage(0);
}

void WindowsHost::OnProgramManagerCloseRequested() {
    if (m_workspaceRuntime && m_workspaceWindow && m_workspaceWindow->GetHwnd()) {
        if (m_homeRuntime) m_homeRuntime->ClearQuitRequest();
        if (m_homeWindow) m_homeWindow->Hide();
        m_activeDialogOwner = m_workspaceWindow ? m_workspaceWindow->GetHwnd() : nullptr;
        return;
    }
    RequestQuit();
}

void WindowsHost::OnWorkspaceCloseRequested() {
    if (!ConfirmWorkspaceDestructiveAction(L"closing CloverPic")) {
        if (m_workspaceRuntime) m_workspaceRuntime->ClearQuitRequest();
        return;
    }
    if (m_workspaceWindow && m_workspaceWindow->GetHwnd()) {
        DestroyWindow(m_workspaceWindow->GetHwnd());
    }
}

void WindowsHost::OnSurfaceDestroyed(SurfaceRole role) {
    if (role == SurfaceRole::Workspace) {
        RequestQuit();
    } else {
        RequestQuit();
    }
}

void WindowsHost::CreateHomeWindow() {
    m_homeRuntime = std::make_unique<Core::ProjectManagerRuntime>();
    m_homeWindow = std::make_unique<WindowsSurfaceWindow>(*this, SurfaceRole::Home, *m_homeRuntime);
    const Size client = ComputeHomeClientSize();
    const HWND owner = m_workspaceWindow ? m_workspaceWindow->GetHwnd() : nullptr;
    m_homeWindow->Create(m_instance, L"CloverPic Program Manager", client.width, client.height, true, owner, true);
    m_activeDialogOwner = m_homeWindow->GetHwnd();
}

void WindowsHost::CreateWorkspaceWindow() {
    m_workspaceRuntime = std::make_unique<Core::WorkspaceRuntime>(Core::WorkspaceLaunchRequest{});
    m_workspaceWindow = std::make_unique<WindowsSurfaceWindow>(*this, SurfaceRole::Workspace, *m_workspaceRuntime);
    const Size client = ComputeWorkspaceClientSize();
    m_workspaceWindow->Create(m_instance, L"CloverPic Workspace", client.width, client.height, true);
    m_activeDialogOwner = m_workspaceWindow->GetHwnd();
}

void WindowsHost::OpenWorkspace(Core::WorkspaceLaunchRequest request) {
    if (!m_workspaceRuntime || !m_workspaceWindow) {
        CreateWorkspaceWindow();
    }
    if (request.kind != Core::WorkspaceLaunchKind::None && !ConfirmWorkspaceDestructiveAction(
            request.kind == Core::WorkspaceLaunchKind::OpenProject ? L"opening another project" : L"creating a new image")) {
        return;
    }
    m_workspaceRuntime->AcceptWorkspaceLaunchRequest(std::move(request));
    m_workspaceWindow->Show(SW_SHOW);
    if (m_homeWindow) m_homeWindow->Hide();
    m_activeDialogOwner = m_workspaceWindow->GetHwnd();
}

bool WindowsHost::ConfirmWorkspaceDestructiveAction(const String& actionLabel) {
    if (!m_workspaceRuntime || !m_workspaceRuntime->HasUnsavedChanges()) {
        return true;
    }
    auto* dialogs = CoreServices::GetFileDialogService();
    if (!dialogs) {
        return false;
    }
    switch (dialogs->PromptUnsavedChanges(actionLabel)) {
        case UnsavedChangesChoice::Save:
            return m_workspaceRuntime->SaveCurrentProjectInteractive(false);
        case UnsavedChangesChoice::Discard:
            return true;
        case UnsavedChangesChoice::Cancel:
        default:
            break;
    }
    return false;
}

void WindowsHost::CloseWorkspaceAndReturnHome() {
    m_workspaceWindow.reset();
    m_workspaceRuntime.reset();
    m_workspaceDestroyed = false;
    if (m_homeRuntime) m_homeRuntime->RefreshRecentFiles();
    if (m_homeWindow) {
        m_homeWindow->Show(SW_SHOW);
        m_activeDialogOwner = m_homeWindow->GetHwnd();
    }
}

void WindowsHost::PumpRuntimeRequests() {
    if (m_workspaceDestroyed) {
        CloseWorkspaceAndReturnHome();
    }
    if (m_workspaceRuntime && m_workspaceRuntime->ConsumeProgramManagerRequest()) {
        const bool showSettings = m_workspaceRuntime->ConsumeProgramManagerSettingsRequest();
        if (m_homeRuntime) {
            m_homeRuntime->RefreshRecentFiles();
            if (showSettings) {
                m_homeRuntime->ShowSettingsPage();
            }
        }
        if (m_homeWindow) {
            m_homeWindow->Show(SW_SHOW);
            m_activeDialogOwner = m_homeWindow->GetHwnd();
        }
    }
    if (m_homeRuntime && m_homeRuntime->HasLaunchRequest()) {
        OpenWorkspace(m_homeRuntime->ConsumeLaunchRequest());
    }
}

Size WindowsHost::ComputeHomeClientSize() const {
    const Size work = WorkAreaSize();
    int clientH = std::clamp(static_cast<int>(work.height * 0.62f), 570, 840);
    int clientW = clientH * 4 / 3;
    const int maxW = std::min(1120, static_cast<int>(work.width * 0.78f));
    if (clientW > maxW) {
        clientW = std::max(760, maxW);
        clientH = clientW * 3 / 4;
    }
    clientW = std::clamp(clientW, 760, 1120);
    clientH = std::clamp(clientH, 570, 840);
    return Size(clientW, clientH);
}

Size WindowsHost::ComputeWorkspaceClientSize() const {
    const Size work = WorkAreaSize();
    return Size(std::clamp(static_cast<int>(work.width * 0.88f), 1180, 1920),
                std::clamp(static_cast<int>(work.height * 0.86f), 780, 1280));
}

} // namespace CloverPic::Platform::Windows
