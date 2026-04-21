#include "Tablet/TabletInput.h"
#include <windowsx.h>

namespace VividPic {
namespace TabletInput {

// WindowsInkDriver implementation
WindowsInkDriver::WindowsInkDriver() = default;
WindowsInkDriver::~WindowsInkDriver() = default;

bool WindowsInkDriver::Initialize(HWND hwnd) {
    m_hwnd = hwnd;
    m_initialized = true;
    return true;
}

bool WindowsInkDriver::ProcessPointerMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!m_initialized) return false;
    
    switch (msg) {
        case WM_POINTERDOWN:
        case WM_POINTERUPDATE:
        case WM_POINTERUP: {
            POINTER_INPUT_TYPE pointerType;
            if (!GetPointerType(GET_POINTERID_WPARAM(wParam), &pointerType)) {
                return false;
            }
            
            if (pointerType != PT_PEN && pointerType != PT_TOUCH) {
                return false;
            }
            
            POINTER_PEN_INFO penInfo = {};
            if (GetPointerPenInfo(GET_POINTERID_WPARAM(wParam), &penInfo)) {
                m_state.x = static_cast<float>(penInfo.pointerInfo.ptPixelLocation.x);
                m_state.y = static_cast<float>(penInfo.pointerInfo.ptPixelLocation.y);
                m_state.pressure = static_cast<float>(penInfo.pressure) / 1024.0f;
                m_state.tiltX = static_cast<float>(penInfo.tiltX) / 10.0f;
                m_state.tiltY = static_cast<float>(penInfo.tiltY) / 10.0f;
                m_state.rotation = static_cast<float>(penInfo.rotation);
                m_state.isEraser = (penInfo.penFlags & PEN_FLAG_ERASER) != 0;
                m_state.isTouching = (penInfo.penFlags & PEN_FLAG_BARREL) != 0 || msg == WM_POINTERDOWN || msg == WM_POINTERUPDATE;
                return true;
            }
            break;
        }
    }
    return false;
}

// WinTabDriver implementation
WinTabDriver::WinTabDriver() = default;
WinTabDriver::~WinTabDriver() {
    UnloadWintab32();
}

bool WinTabDriver::Initialize(HWND hwnd) {
    m_hwnd = hwnd;
    
    if (!LoadWintab32()) {
        return false;
    }
    
    m_initialized = true;
    return true;
}

bool WinTabDriver::LoadWintab32() {
    m_hWintab = LoadLibraryW(L"Wintab32.dll");
    if (!m_hWintab) {
        return false;
    }
    
    pWTInfoA = reinterpret_cast<WTINFOA_Fn>(GetProcAddress(m_hWintab, "WTInfoA"));
    pWTOpenA = reinterpret_cast<WTOPENA_Fn>(GetProcAddress(m_hWintab, "WTOpenA"));
    pWTClose = reinterpret_cast<WTCLOSE_Fn>(GetProcAddress(m_hWintab, "WTClose"));
    pWTPacket = reinterpret_cast<WTPACKET_Fn>(GetProcAddress(m_hWintab, "WTPacket"));
    pWTEnable = reinterpret_cast<WTENABLE_Fn>(GetProcAddress(m_hWintab, "WTEnable"));
    
    if (!pWTInfoA || !pWTOpenA || !pWTClose || !pWTPacket) {
        UnloadWintab32();
        return false;
    }
    
    // TODO: Open tablet context (M4 full implementation)
    return true;
}

void WinTabDriver::UnloadWintab32() {
    if (m_hCtx && pWTClose) {
        pWTClose(m_hCtx);
        m_hCtx = nullptr;
    }
    if (m_hWintab) {
        FreeLibrary(m_hWintab);
        m_hWintab = nullptr;
    }
    pWTInfoA = nullptr;
    pWTOpenA = nullptr;
    pWTClose = nullptr;
    pWTPacket = nullptr;
    pWTEnable = nullptr;
}

bool WinTabDriver::ProcessPacket(WPARAM wParam, LPARAM lParam) {
    if (!m_initialized || !pWTPacket) return false;
    
    // TODO: Full packet processing in M4
    // For now, stub implementation
    return false;
}

// TabletManager implementation
TabletManager& TabletManager::GetInstance() {
    static TabletManager instance;
    return instance;
}

bool TabletManager::Initialize(HWND hwnd) {
    // Try WinTab first (preferred for professional tablets)
    if (m_wintabDriver.Initialize(hwnd)) {
        m_activeDriver = DriverType::WinTab;
        m_initialized = true;
        return true;
    }
    
    // Fall back to Windows Ink
    if (m_inkDriver.Initialize(hwnd)) {
        m_activeDriver = DriverType::WindowsInk;
        m_initialized = true;
        return true;
    }
    
    // Mouse fallback
    m_activeDriver = DriverType::Mouse;
    m_initialized = true;
    return true;
}

void TabletManager::Shutdown() {
    m_wintabDriver.UnloadWintab32();
    m_initialized = false;
    m_activeDriver = DriverType::None;
}

bool TabletManager::ProcessMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!m_initialized) return false;
    
    switch (m_activeDriver) {
        case DriverType::WinTab:
            if (msg == TabletInput::WT_PACKET_MSG) {
                return m_wintabDriver.ProcessPacket(wParam, lParam);
            }
            break;
            
        case DriverType::WindowsInk:
            if (msg == WM_POINTERDOWN || msg == WM_POINTERUPDATE || msg == WM_POINTERUP) {
                return m_inkDriver.ProcessPointerMessage(msg, wParam, lParam);
            }
            break;
            
        case DriverType::Mouse:
        default:
            // Mouse messages handled elsewhere
            break;
    }
    return false;
}

TabletState TabletManager::GetState() const {
    switch (m_activeDriver) {
        case DriverType::WinTab:
            return m_wintabDriver.GetState();
        case DriverType::WindowsInk:
            return m_inkDriver.GetState();
        default:
            return TabletState();
    }
}

bool TabletManager::HasPressure() const {
    return m_activeDriver == DriverType::WinTab || m_activeDriver == DriverType::WindowsInk;
}

} // namespace TabletInput
} // namespace VividPic
