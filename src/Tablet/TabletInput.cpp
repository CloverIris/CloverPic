#include "Tablet/TabletInput.h"
#include <windowsx.h>
#include <cmath>

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

    if (!OpenContext(hwnd)) {
        UnloadWintab32();
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

    return true;
}

bool WinTabDriver::OpenContext(HWND hwnd) {
    if (!pWTInfoA || !pWTOpenA) return false;

    WintabLogContext lc = {};
    if (!pWTInfoA(WTI_DEFCONTEXT, 0, &lc)) {
        return false;
    }

    // Request packet data: x, y, z, normal pressure, buttons, orientation
    lc.lcPktData = 0x0001 | 0x0002 | 0x0004 | 0x0040 | 0x0100 | 0x0200; // PK_X|Y|Z|NORMAL_PRESSURE|BUTTONS|ORIENTATION
    lc.lcPktMode = 0;
    lc.lcMoveMask = lc.lcPktData;
    lc.lcBtnUpMask = lc.lcPktData;

    // Output to screen coordinates
    lc.lcOutOrgX = 0;
    lc.lcOutOrgY = 0;
    lc.lcOutExtX = GetSystemMetrics(SM_CXSCREEN);
    lc.lcOutExtY = GetSystemMetrics(SM_CYSCREEN);

    m_hCtx = pWTOpenA(hwnd, &lc, TRUE);
    if (!m_hCtx) {
        return false;
    }

    m_contextOpened = true;

    // Query max pressure
    struct PressureRange {
        LONG pMin;
        LONG pMax;
    } pr = {};
    if (pWTInfoA(WTI_DEVICES, DVC_NPRESSURE, &pr)) {
        if (pr.pMax > pr.pMin) {
            m_maxPressure = static_cast<UINT>(pr.pMax);
        }
    }

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
    pWTGetA = nullptr;
    m_contextOpened = false;
}

bool WinTabDriver::ProcessPacket(WPARAM wParam, LPARAM lParam) {
    if (!m_initialized || !pWTPacket || !m_contextOpened) return false;

    WintabPacket packet = {};
    if (!pWTPacket(m_hCtx, static_cast<UINT>(wParam), &packet)) {
        return false;
    }

    // Map to screen coordinates (already done by WinTab context setup)
    m_state.x = static_cast<float>(packet.pkX);
    m_state.y = static_cast<float>(packet.pkY);

    // Normalize pressure
    if (m_maxPressure > 0) {
        m_state.pressure = std::clamp(static_cast<float>(packet.pkNormalPressure) / static_cast<float>(m_maxPressure), 0.0f, 1.0f);
    } else {
        m_state.pressure = 0.0f;
    }

    // Orientation to tilt/rotation
    if (packet.pkChanged & 0x0200) { // PK_ORIENTATION
        // Azimuth: 0 to 3600 (tenths of degrees), 0 = +x axis
        float azimuthRad = static_cast<float>(packet.pkOrientation.orAzimuth) * 3.14159265f / 1800.0f;
        // Altitude: 0 to 900 (tenths of degrees), 0 = parallel to tablet, 900 = perpendicular
        float altitudeRad = static_cast<float>(packet.pkOrientation.orAltitude) * 3.14159265f / 1800.0f;

        // Convert to tilt angles
        float altCos = std::cos(altitudeRad);
        if (altCos < 0.001f) altCos = 0.001f;
        m_state.tiltX = std::clamp(std::cos(azimuthRad) / altCos * 45.0f, -90.0f, 90.0f);
        m_state.tiltY = std::clamp(std::sin(azimuthRad) / altCos * 45.0f, -90.0f, 90.0f);
        m_state.rotation = static_cast<float>(packet.pkOrientation.orTwist) / 10.0f;
    }

    // Buttons: bit 0 = tip, bit 1 = lower barrel, bit 2 = upper barrel
    m_state.isTouching = (packet.pkButtons & 0x01) != 0;
    m_state.isEraser = (packet.pkButtons & 0x04) != 0;

    return true;
}

// TabletManager implementation
TabletManager& TabletManager::GetInstance() {
    static TabletManager instance;
    return instance;
}

bool TabletManager::Initialize(HWND hwnd) {
    // Try Windows Ink first (modern, reliable on Win10+)
    if (m_inkDriver.Initialize(hwnd)) {
        m_activeDriver = DriverType::WindowsInk;
        m_initialized = true;
        return true;
    }

    // Fall back to WinTab (legacy Wacom driver)
    if (m_wintabDriver.Initialize(hwnd)) {
        m_activeDriver = DriverType::WinTab;
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
