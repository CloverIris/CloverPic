#pragma once

#include "Utils/Types.h"
#include <Windows.h>

namespace VividPic {
namespace TabletInput {

struct TabletState {
    float x = 0.0f;           // Canvas coordinates
    float y = 0.0f;
    float pressure = 0.0f;    // 0.0f ~ 1.0f (normalized)
    float tiltX = 0.0f;       // -90.0f ~ +90.0f
    float tiltY = 0.0f;       // -90.0f ~ +90.0f
    float rotation = 0.0f;    // Degrees
    bool isEraser = false;
    bool isTouching = false;
};

// WinTab simplified types (self-contained to avoid external headers)
using HCTX = HANDLE;

struct WintabLogContext {
    char lcName[40];
    UINT lcOptions;
    UINT lcStatus;
    UINT lcLocks;
    UINT lcMsgBase;
    UINT lcDevice;
    UINT lcPktRate;
    DWORD lcPktData;
    DWORD lcPktMode;
    DWORD lcMoveMask;
    DWORD lcBtnDnMask;
    DWORD lcBtnUpMask;
    LONG lcInOrgX;
    LONG lcInOrgY;
    LONG lcInOrgZ;
    LONG lcInExtX;
    LONG lcInExtY;
    LONG lcInExtZ;
    LONG lcOutOrgX;
    LONG lcOutOrgY;
    LONG lcOutOrgZ;
    LONG lcOutExtX;
    LONG lcOutExtY;
    LONG lcOutExtZ;
    LONG lcSensX;
    LONG lcSensY;
    LONG lcSensZ;
    BOOL lcSysMode;
    int lcSysOrgX;
    int lcSysOrgY;
    int lcSysExtX;
    int lcSysExtY;
    LONG lcSysSensX;
    LONG lcSysSensY;
};

static constexpr UINT WT_PACKET_MSG = 0x7FF0;
static constexpr UINT WTI_DEVICES = 100;
static constexpr UINT DVC_NAME = 1;

// Windows Ink API driver (Windows 8+ Pointer API)
class WindowsInkDriver {
public:
    WindowsInkDriver();
    ~WindowsInkDriver();
    
    bool Initialize(HWND hwnd);
    bool ProcessPointerMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    TabletState GetState() const { return m_state; }
    bool IsAvailable() const { return m_initialized; }
    
private:
    HWND m_hwnd = nullptr;
    TabletState m_state;
    bool m_initialized = false;
};

// WinTab API driver (Wacom compatible)
class WinTabDriver {
public:
    WinTabDriver();
    ~WinTabDriver();
    
    bool Initialize(HWND hwnd);
    bool LoadWintab32();
    void UnloadWintab32();
    bool ProcessPacket(WPARAM wParam, LPARAM lParam);
    TabletState GetState() const { return m_state; }
    bool IsAvailable() const { return m_initialized && m_hWintab != nullptr; }
    
private:
    HWND m_hwnd = nullptr;
    HMODULE m_hWintab = nullptr;
    TabletState m_state;
    bool m_initialized = false;
    
    using WTINFOA_Fn = UINT(WINAPI*)(UINT, UINT, LPVOID);
    using WTOPENA_Fn = HCTX(WINAPI*)(HWND, WintabLogContext*, BOOL);
    using WTCLOSE_Fn = BOOL(WINAPI*)(HCTX);
    using WTPACKET_Fn = BOOL(WINAPI*)(HCTX, UINT, LPVOID);
    using WTENABLE_Fn = BOOL(WINAPI*)(HCTX, BOOL);
    
    WTINFOA_Fn pWTInfoA = nullptr;
    WTOPENA_Fn pWTOpenA = nullptr;
    WTCLOSE_Fn pWTClose = nullptr;
    WTPACKET_Fn pWTPacket = nullptr;
    WTENABLE_Fn pWTEnable = nullptr;
    
    HCTX m_hCtx = nullptr;
};

// Unified tablet manager
class TabletManager {
public:
    static TabletManager& GetInstance();
    
    bool Initialize(HWND hwnd);
    void Shutdown();
    
    bool ProcessMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    TabletState GetState() const;
    
    enum class DriverType {
        None,
        WinTab,
        WindowsInk,
        Mouse
    };
    
    DriverType GetActiveDriver() const { return m_activeDriver; }
    bool HasPressure() const;
    
private:
    TabletManager() = default;
    
    WindowsInkDriver m_inkDriver;
    WinTabDriver m_wintabDriver;
    DriverType m_activeDriver = DriverType::None;
    bool m_initialized = false;
};

} // namespace TabletInput
} // namespace VividPic
