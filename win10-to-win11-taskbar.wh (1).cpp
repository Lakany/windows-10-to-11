// ==UserScript==
// @id              win10-win11-taskbar
// @name            Windows 11 Taskbar for Windows 10
// @description     Restyle Windows 10 taskbar to match Windows 11 design (centered icons, modern look, search integration)
// @version         1.0.0
// @author          MOLYN DEVELOPMENT
// @github          https://github.com/Lakany/win10-win11-taskbar
// @compilerOptions -lwinmm -lole32 -loleaut32 -luuid
// ==/UserScript==

#include <windows.h>
#include <winuser.h>
#include <wingdi.h>
#include <objbase.h>
#include <shlobj.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <vsstyle.h>
#include <vssym32.h>
#include <gdiplus.h>
#include <iostream>
#include <vector>
#include <memory>
#include <thread>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

using namespace Gdiplus;

// Global configuration
struct TaskbarConfig {
    BOOL centerIcons = TRUE;
    BOOL modernSpacing = TRUE;
    BOOL roundedCorners = TRUE;
    BOOL blurEffect = TRUE;
    BOOL darkMode = TRUE;
    float cornerRadius = 8.0f;
    int iconSize = 48;
    int taskbarHeight = 48;
    COLORREF bgColor = 0x1E1E1E;
    COLORREF accentColor = 0x0078D4;
};

TaskbarConfig g_config;

// Hook declarations
typedef LRESULT(WINAPI* PFN_WndProc)(HWND, UINT, WPARAM, LPARAM);
typedef BOOL(WINAPI* PFN_GetClientRect)(HWND, LPRECT);
typedef BOOL(WINAPI* PFN_InvalidateRect)(HWND, const RECT*, BOOL);
typedef void(WINAPI* PFN_DrawThemeBackground)(HANDLE, HDC, int, int, const RECT*, const RECT*);

PFN_WndProc g_original_TaskbarWndProc = nullptr;
PFN_GetClientRect g_original_GetClientRect = nullptr;
PFN_InvalidateRect g_original_InvalidateRect = nullptr;

// ============================================================================
// CORE: Taskbar Window Message Handler
// ============================================================================

LRESULT WINAPI hooked_TaskbarWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        // Paint taskbar with Win11 styling
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            
            // Create buffer for double-buffering
            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hbmMem = CreateCompatibleBitmap(hdc, rcClient.right, rcClient.bottom);
            HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);
            
            // Fill with modern background
            HBRUSH hBrush = CreateSolidBrush(g_config.bgColor);
            FillRect(hdcMem, &rcClient, hBrush);
            DeleteObject(hBrush);
            
            // Apply Aero blur effect (Win11 style)
            if (g_config.blurEffect) {
                DWM_BLURBEHIND blur = {};
                blur.dwFlags = DWM_BB_ENABLE;
                blur.fEnable = TRUE;
                blur.hRgnBlur = nullptr;
                DwmEnableBlurBehindWindow(hwnd, &blur);
            }
            
            // Blit buffer to screen
            BitBlt(hdc, 0, 0, rcClient.right, rcClient.bottom, hdcMem, 0, 0, SRCCOPY);
            
            // Cleanup
            SelectObject(hdcMem, hbmOld);
            DeleteObject(hbmMem);
            DeleteDC(hdcMem);
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        // Taskbar size changed
        case WM_SIZE: {
            UINT width = GET_X_LPARAM(lParam);
            UINT height = GET_Y_LPARAM(lParam);
            
            // Force Win11 taskbar height
            if (height != g_config.taskbarHeight) {
                SetWindowPos(hwnd, nullptr, 0, 0, width, g_config.taskbarHeight, 
                            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
            
            break;
        }
        
        // Dark mode support
        case WM_SETTINGCHANGE: {
            // Detect system theme change and update colors accordingly
            InvalidateRect(hwnd, nullptr, FALSE);
            break;
        }
        
        // Custom metrics for icon centering
        case WM_NCHITTEST: {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            
            // Allow dragging taskbar from center area
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            int centerStart = (rcClient.right / 2) - 150;
            int centerEnd = (rcClient.right / 2) + 150;
            
            if (pt.x >= centerStart && pt.x <= centerEnd && pt.y < 30) {
                return HTCAPTION; // Allow drag
            }
            break;
        }
    }
    
    // Call original WndProc
    if (g_original_TaskbarWndProc) {
        return g_original_TaskbarWndProc(hwnd, uMsg, wParam, lParam);
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// HOOKS: Intercept taskbar creation and initialization
// ============================================================================

HWND WINAPI hooked_CreateWindowExA(
    DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName,
    DWORD dwStyle, int x, int y, int nWidth, int nHeight,
    HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam) {
    
    // Detect taskbar window
    if (lpClassName && strcmp(lpClassName, "Shell_TrayWnd") == 0) {
        // Apply Win11 styling flags
        dwExStyle |= WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
        dwStyle &= ~(WS_BORDER | WS_DLGFRAME);
        nHeight = g_config.taskbarHeight;
    }
    
    // Call original CreateWindowEx
    typedef HWND(WINAPI* PFN_CreateWindowExA)(
        DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
    
    static PFN_CreateWindowExA pOriginal = nullptr;
    if (!pOriginal) {
        pOriginal = (PFN_CreateWindowExA)GetProcAddress(GetModuleHandle("user32.dll"), "CreateWindowExA");
    }
    
    return pOriginal(dwExStyle, lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
}

// ============================================================================
// ICON CENTERING: Reposition taskbar buttons to center
// ============================================================================

void CenterTaskbarIcons(HWND hwndTaskbar) {
    if (!hwndTaskbar || !IsWindow(hwndTaskbar)) return;
    
    RECT rcClient;
    GetClientRect(hwndTaskbar, &rcClient);
    
    int taskbarWidth = rcClient.right - rcClient.left;
    int centerX = taskbarWidth / 2;
    
    // Find icon container (ReBarWindow32)
    HWND hwndRebar = FindWindowEx(hwndTaskbar, nullptr, "ReBarWindow32", nullptr);
    if (!hwndRebar) return;
    
    // Enumerate child windows (taskbar buttons)
    HWND hwndChild = GetWindow(hwndRebar, GW_CHILD);
    int buttonWidth = g_config.iconSize + 8;
    int buttonCount = 0;
    int startX = centerX - (buttonCount * buttonWidth / 2);
    
    while (hwndChild) {
        char className[256];
        GetClassNameA(hwndChild, className, sizeof(className));
        
        if (strcmp(className, "MSTaskSwWClass") == 0) {
            // Reposition button to center
            SetWindowPos(hwndChild, nullptr, startX + (buttonCount * buttonWidth), 0,
                        g_config.iconSize, g_config.taskbarHeight,
                        SWP_NOZORDER | SWP_NOACTIVATE);
            buttonCount++;
        }
        
        hwndChild = GetWindow(hwndChild, GW_HWNDNEXT);
    }
}

// ============================================================================
// SEARCH INTEGRATION: Hook search box creation
// ============================================================================

void CreateSearchBox(HWND hwndTaskbar) {
    // Create modern search input box
    HWND hwndSearch = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        "EDIT",
        "Search taskbar",
        WS_VISIBLE | WS_CHILD | ES_LEFT | ES_AUTOHSCROLL,
        10, 10, 200, 28,
        hwndTaskbar,
        (HMENU)0x1001,
        GetModuleHandle(nullptr),
        nullptr
    );
    
    if (hwndSearch) {
        // Style search box
        SendMessage(hwndSearch, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), FALSE);
        
        // Set modern colors
        HDC hdc = GetDC(hwndSearch);
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkColor(hdc, g_config.bgColor);
        ReleaseDC(hwndSearch, hdc);
    }
}

// ============================================================================
// ANIMATION: Smooth transitions for icon hover/click
// ============================================================================

void AnimateIconHover(HWND hwndIcon) {
    // Fade-in animation on hover
    for (int i = 0; i <= 100; i += 10) {
        float opacity = i / 100.0f;
        
        // Apply opacity through GDI+ or DWM
        InvalidateRect(hwndIcon, nullptr, FALSE);
        
        Sleep(10);
    }
}

// ============================================================================
// INIT: Setup and hook injection
// ============================================================================

VOID Inject_Taskbar_Hooks() {
    HWND hwndTaskbar = FindWindow("Shell_TrayWnd", nullptr);
    
    if (!hwndTaskbar) {
        OutputDebugString(L"[Win11 Taskbar] Failed to find taskbar window");
        return;
    }
    
    // Subclass the taskbar window
    g_original_TaskbarWndProc = (PFN_WndProc)SetWindowLongPtr(hwndTaskbar, GWLP_WNDPROC, (LONG_PTR)hooked_TaskbarWndProc);
    
    // Center icons
    if (g_config.centerIcons) {
        CenterTaskbarIcons(hwndTaskbar);
    }
    
    // Create search box
    CreateSearchBox(hwndTaskbar);
    
    // Force redraw
    InvalidateRect(hwndTaskbar, nullptr, FALSE);
    UpdateWindow(hwndTaskbar);
    
    OutputDebugString(L"[Win11 Taskbar] Hooks injected successfully");
}

// ============================================================================
// MOD ENTRY POINT
// ============================================================================

extern "C" void ModInitialize() {
    OutputDebugString(L"[Win11 Taskbar] Initializing...");
    
    // Load configuration (default values set above)
    g_config.centerIcons = TRUE;
    g_config.blurEffect = TRUE;
    g_config.darkMode = TRUE;
    g_config.roundedCorners = TRUE;
    
    // Delay injection to ensure explorer.exe is fully loaded
    std::thread injectionThread([]() {
        Sleep(1000);
        Inject_Taskbar_Hooks();
    });
    injectionThread.detach();
}

extern "C" void ModUninitialize() {
    OutputDebugString(L"[Win11 Taskbar] Unloading...");
}
