#pragma once

#include <windows.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <vector>
#include <chrono>
#include <functional>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

// ============================================================================
// ANIMATION SYSTEM
// ============================================================================

class AnimationManager {
public:
    enum class AnimationType {
        FadeIn,
        FadeOut,
        SlideIn,
        SlideOut,
        Bounce,
        Pulse
    };

    struct AnimationFrame {
        HWND hwnd;
        AnimationType type;
        float duration;  // in milliseconds
        float elapsed;
        std::function<void(float)> callback;
    };

    static AnimationManager& Instance() {
        static AnimationManager instance;
        return instance;
    }

    void StartAnimation(HWND hwnd, AnimationType type, float duration, 
                      std::function<void(float)> callback) {
        AnimationFrame frame = {
            hwnd, type, duration, 0.0f, callback
        };
        animations.push_back(frame);
    }

    void Update(float deltaTime) {
        for (auto it = animations.begin(); it != animations.end(); ++it) {
            it->elapsed += deltaTime;
            float progress = it->elapsed / it->duration;
            
            if (progress >= 1.0f) {
                it->callback(1.0f);
                it = animations.erase(it);
            } else {
                float eased = EaseInOutQuad(progress);
                it->callback(eased);
            }
        }
    }

private:
    std::vector<AnimationFrame> animations;

    // Easing function: Quad In-Out
    float EaseInOutQuad(float t) {
        return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
    }
};

// ============================================================================
// WIDGETS PANEL
// ============================================================================

class WidgetsPanel {
public:
    struct Widget {
        std::string id;
        std::string name;
        RECT bounds;
        HBITMAP icon;
        bool enabled;
    };

    WidgetsPanel() : hwnd(nullptr), isVisible(false) {}

    bool Create(HWND hwndParent) {
        const char* className = "Win11WidgetsPanel";
        WNDCLASSA wc = {};
        
        if (!GetClassInfoA(GetModuleHandle(nullptr), className, &wc)) {
            wc.lpfnWndProc = WidgetsPanelProc;
            wc.lpszClassName = className;
            wc.hbrBackground = (HBRUSH)CreateSolidBrush(0x1E1E1E);
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            
            if (!RegisterClassA(&wc)) {
                return false;
            }
        }

        hwnd = CreateWindowExA(
            WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            className,
            "Widgets",
            WS_POPUP | WS_VISIBLE,
            0, 0, 360, 600,
            hwndParent,
            nullptr,
            GetModuleHandle(nullptr),
            this
        );

        return hwnd != nullptr;
    }

    void Show() {
        if (hwnd) {
            ShowWindow(hwnd, SW_SHOW);
            isVisible = true;
        }
    }

    void Hide() {
        if (hwnd) {
            ShowWindow(hwnd, SW_HIDE);
            isVisible = false;
        }
    }

    void AddWidget(const Widget& widget) {
        widgets.push_back(widget);
        if (hwnd) {
            InvalidateRect(hwnd, nullptr, FALSE);
        }
    }

    void RemoveWidget(const std::string& id) {
        auto it = std::find_if(widgets.begin(), widgets.end(),
            [&id](const Widget& w) { return w.id == id; });
        if (it != widgets.end()) {
            widgets.erase(it);
            if (hwnd) {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
    }

private:
    HWND hwnd;
    bool isVisible;
    std::vector<Widget> widgets;

    static LRESULT CALLBACK WidgetsPanelProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        WidgetsPanel* pThis = nullptr;

        if (uMsg == WM_CREATE) {
            CREATESTRUCTA* pCreate = reinterpret_cast<CREATESTRUCTA*>(lParam);
            pThis = reinterpret_cast<WidgetsPanel*>(pCreate->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        } else {
            pThis = reinterpret_cast<WidgetsPanel*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        }

        if (pThis) {
            switch (uMsg) {
                case WM_PAINT:
                    pThis->OnPaint(hwnd);
                    return 0;

                case WM_LBUTTONDOWN:
                    pThis->OnMouseClick(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                    return 0;

                case WM_DESTROY:
                    PostQuitMessage(0);
                    return 0;
            }
        }

        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    void OnPaint(HWND hwnd) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rcClient;
        GetClientRect(hwnd, &rcClient);

        // Background
        HBRUSH hBrush = CreateSolidBrush(0x1E1E1E);
        FillRect(hdc, &rcClient, hBrush);
        DeleteObject(hBrush);

        // Draw widgets
        int yOffset = 10;
        for (const auto& widget : widgets) {
            if (widget.enabled) {
                // Widget background
                RECT rcWidget = { 10, yOffset, 350, yOffset + 80 };
                HBRUSH hWidgetBrush = CreateSolidBrush(0x2D2D2D);
                FillRect(hdc, &rcWidget, hWidgetBrush);
                DeleteObject(hWidgetBrush);

                // Widget name
                SetTextColor(hdc, RGB(255, 255, 255));
                SetBkColor(hdc, 0x2D2D2D);
                TextOutA(hdc, 20, yOffset + 10, widget.name.c_str(), widget.name.length());

                yOffset += 90;
            }
        }

        EndPaint(hwnd, &ps);
    }

    void OnMouseClick(HWND hwnd, int x, int y) {
        // Check which widget was clicked
        int yOffset = 10;
        for (auto& widget : widgets) {
            if (widget.enabled) {
                RECT rcWidget = { 10, yOffset, 350, yOffset + 80 };
                if (PtInRect(&rcWidget, { x, y })) {
                    // Widget clicked
                    widget.enabled = !widget.enabled;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    break;
                }
                yOffset += 90;
            }
        }
    }
};

// ============================================================================
// GLASS EFFECT (MICA)
// ============================================================================

class GlassEffect {
public:
    enum class MicaKind {
        Base = 0,
        Alt = 1
    };

    static void ApplyMica(HWND hwnd, MicaKind kind = MicaKind::Base) {
        DWM_BLURBEHIND blur = {};
        blur.dwFlags = DWM_BB_ENABLE;
        blur.fEnable = TRUE;
        blur.hRgnBlur = nullptr;
        DwmEnableBlurBehindWindow(hwnd, &blur);
    }

    static void DisableMica(HWND hwnd) {
        DWM_BLURBEHIND blur = {};
        blur.dwFlags = DWM_BB_ENABLE;
        blur.fEnable = FALSE;
        DwmEnableBlurBehindWindow(hwnd, &blur);
    }

    static void SetBackdrop(HWND hwnd, COLORREF color) {
        // Set window background with DWM
        HWND hDesktop = GetDesktopWindow();
        HDC hdcDesktop = GetDC(hDesktop);
        
        int width = GetDeviceCaps(hdcDesktop, HORZRES);
        int height = GetDeviceCaps(hdcDesktop, VERTRES);

        ReleaseDC(hDesktop, hdcDesktop);
    }
};

// ============================================================================
// ROUNDED CORNERS
// ============================================================================

class RoundedWindowHelper {
public:
    static void SetRoundedCorners(HWND hwnd, int radius) {
        HRGN hrgn = CreateRoundRectRgn(0, 0, 500, 500, radius, radius);
        SetWindowRgn(hwnd, hrgn, TRUE);
    }

    static void RemoveRoundedCorners(HWND hwnd) {
        SetWindowRgn(hwnd, nullptr, TRUE);
    }
};

// ============================================================================
// TASKBAR UTILS
// ============================================================================

class TaskbarUtils {
public:
    static HWND FindTaskbarWindow() {
        return FindWindow("Shell_TrayWnd", nullptr);
    }

    static HWND FindTaskbarButton(HWND hwndTaskbar, const char* windowName) {
        HWND hwnd = nullptr;
        HWND hwndChild = GetWindow(hwndTaskbar, GW_CHILD);

        while (hwndChild) {
            char className[256];
            GetClassNameA(hwndChild, className, sizeof(className));

            if (strcmp(className, "MSTaskSwWClass") == 0) {
                char title[256];
                GetWindowTextA(hwndChild, title, sizeof(title));

                if (strstr(title, windowName)) {
                    hwnd = hwndChild;
                    break;
                }
            }

            hwndChild = GetWindow(hwndChild, GW_HWNDNEXT);
        }

        return hwnd;
    }

    static void CenterTaskbarIcons(HWND hwndTaskbar, int iconSize = 48) {
        RECT rcTaskbar;
        GetClientRect(hwndTaskbar, &rcTaskbar);

        int taskbarWidth = rcTaskbar.right - rcTaskbar.left;
        int centerX = taskbarWidth / 2;

        // Find ReBar
        HWND hwndRebar = FindWindowEx(hwndTaskbar, nullptr, "ReBarWindow32", nullptr);
        if (!hwndRebar) return;

        int buttonWidth = iconSize + 8;
        int buttonIndex = 0;

        HWND hwndChild = GetWindow(hwndRebar, GW_CHILD);
        while (hwndChild) {
            char className[256];
            GetClassNameA(hwndChild, className, sizeof(className));

            if (strcmp(className, "MSTaskSwWClass") == 0) {
                int startX = centerX - (buttonWidth * 10);  // Assume ~10 buttons max
                SetWindowPos(hwndChild, nullptr,
                           startX + (buttonIndex * buttonWidth), 0,
                           iconSize, 48,
                           SWP_NOZORDER | SWP_NOACTIVATE);
                buttonIndex++;
            }

            hwndChild = GetWindow(hwndChild, GW_HWNDNEXT);
        }
    }

    static void SetTaskbarColor(HWND hwndTaskbar, COLORREF color) {
        HDC hdc = GetDC(hwndTaskbar);
        HBRUSH hBrush = CreateSolidBrush(color);
        RECT rcClient;
        GetClientRect(hwndTaskbar, &rcClient);
        FillRect(hdc, &rcClient, hBrush);
        DeleteObject(hBrush);
        ReleaseDC(hwndTaskbar, hdc);
    }
};

// ============================================================================
// HELPER MACROS
// ============================================================================

#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

// ============================================================================
// CALLBACK SYSTEM
// ============================================================================

class EventSystem {
public:
    using EventCallback = std::function<void(HWND, UINT, WPARAM, LPARAM)>;

    static EventSystem& Instance() {
        static EventSystem instance;
        return instance;
    }

    void Subscribe(UINT message, EventCallback callback) {
        callbacks[message].push_back(callback);
    }

    void Trigger(UINT message, HWND hwnd, WPARAM wParam, LPARAM lParam) {
        if (callbacks.find(message) != callbacks.end()) {
            for (auto& cb : callbacks[message]) {
                cb(hwnd, message, wParam, lParam);
            }
        }
    }

private:
    std::map<UINT, std::vector<EventCallback>> callbacks;
};
