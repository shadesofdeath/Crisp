// AboutWindow.cpp — bkz. AboutWindow.h.
#include "AboutWindow.h"

#include "AboutInternal.h"

#include "Geometry.h"
#include "Localization.h"
#include "Ocr.h"
#include "Theme.h"
#include "Util.h"
#include "resource.h"

// WIN32_LEAN_AND_MEAN yüzünden hiçbiri <windows.h> ile gelmez:
//   windowsx.h — GET_X_LPARAM / GET_Y_LPARAM (işaretli koordinat çıkarımı)
//   shellscalingapi.h — GetDpiForMonitor
//   commctrl.h — LoadIconWithScaleDown
#include <commctrl.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <windowsx.h>

#include <string>

namespace crisp {
using namespace about;
namespace {

constexpr const wchar_t* kWindowClass = L"CrispAboutWindow";

// Tasarım ölçüleri (96 DPI'da mantıksal piksel).

// Aynı anda birden fazla hakkında penceresi anlamsız; ikinci çağrı mevcut
// olanı öne getirir.
HWND g_open = nullptr;


LRESULT CALLBACK AboutProc(HWND window, UINT message, WPARAM wParam,
                           LPARAM lParam) {
    auto* state =
        reinterpret_cast<AboutState*>(::GetWindowLongPtrW(window, GWLP_USERDATA));

    switch (message) {
        case WM_NCCREATE: {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            ::SetWindowLongPtrW(window, GWLP_USERDATA,
                                reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return ::DefWindowProcW(window, message, wParam, lParam);
        }

        case WM_PAINT:
            if (state != nullptr) {
                Paint(window, *state);
            }
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_MOUSEMOVE: {
            if (state == nullptr) {
                break;
            }
            const POINT cursor{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            const bool link = ::PtInRect(&state->linkRect, cursor) != FALSE;
            const bool close = ::PtInRect(&state->closeRect, cursor) != FALSE;
            if (link != state->linkHot || close != state->closeHot) {
                state->linkHot = link;
                state->closeHot = close;
                ::InvalidateRect(window, nullptr, FALSE);
            }
            ::SetCursor(::LoadCursorW(nullptr, link ? IDC_HAND : IDC_ARROW));
            return 0;
        }

        case WM_LBUTTONUP: {
            if (state == nullptr) {
                break;
            }
            const POINT cursor{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            if (::PtInRect(&state->linkRect, cursor)) {
                ::ShellExecuteW(nullptr, L"open", kRepositoryUrl, nullptr, nullptr,
                                SW_SHOWNORMAL);
                return 0;
            }
            if (::PtInRect(&state->closeRect, cursor)) {
                ::DestroyWindow(window);
                return 0;
            }
            break;
        }

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE || wParam == VK_RETURN) {
                ::DestroyWindow(window);
                return 0;
            }
            break;

        // DPI değişimi: pencere başka bir monitöre sürüklendiğinde yeniden
        // ölçeklenmezse metin ya bulanık ya yanlış boyutta kalır.
        case WM_DPICHANGED: {
            if (state != nullptr) {
                state->dpi = HIWORD(wParam);
                const auto* suggested = reinterpret_cast<const RECT*>(lParam);
                ::SetWindowPos(window, nullptr, suggested->left, suggested->top,
                               static_cast<int>(geom::Width(*suggested)),
                               static_cast<int>(geom::Height(*suggested)),
                               SWP_NOZORDER | SWP_NOACTIVATE);
                ::InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        }

        case WM_DESTROY:
            g_open = nullptr;
            ::PostQuitMessage(0);
            return 0;

        default:
            break;
    }

    return ::DefWindowProcW(window, message, wParam, lParam);
}

[[nodiscard]] bool EnsureWindowClass(HINSTANCE instance) {
    static bool registered = false;
    if (registered) {
        return true;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = AboutProc;
    wc.hInstance = instance;
    wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClass;
    wc.hIcon = ::LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP));
    // Arka plan fırçası yok: WM_ERASEBKGND yutuluyor ve tüm yüzey
    // WM_PAINT'te tampondan geliyor.
    wc.hbrBackground = nullptr;

    registered = ::RegisterClassExW(&wc) != 0;
    return registered;
}

}  // namespace

namespace about {
int Scale(int value, unsigned dpi) noexcept {
    return ::MulDiv(value, static_cast<int>(dpi), 96);
}
}  // namespace about

void ShowAboutWindow(HINSTANCE instance) {
    if (g_open != nullptr) {
        ::SetForegroundWindow(g_open);
        return;
    }
    if (!EnsureWindowClass(instance)) {
        return;
    }

    AboutState state;

    // Pencere, İMLECİN BULUNDUĞU monitörde ortalanır. Birincil ekranda açmak,
    // üç monitörlü bir kurulumda kullanıcının baktığı yerin dışında bir yerde
    // pencere açmak olurdu.
    POINT cursor{};
    ::GetCursorPos(&cursor);
    const HMONITOR monitor = ::MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);

    MONITORINFO info{};
    info.cbSize = sizeof(info);
    RECT work{0, 0, 1920, 1080};
    if (::GetMonitorInfoW(monitor, &info)) {
        work = info.rcWork;
    }

    UINT dpiX = 96;
    UINT dpiY = 96;
    if (FAILED(::GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
        dpiX = 96;
    }
    state.dpi = dpiX;

    const int width = Scale(kWidth, state.dpi);
    const int height = Scale(kHeight, state.dpi);
    const int x = work.left + (static_cast<int>(geom::Width(work)) - width) / 2;
    const int y = work.top + (static_cast<int>(geom::Height(work)) - height) / 2;

    const int iconSide = Scale(kIconSide, state.dpi);
    HICON icon = nullptr;
    (void)::LoadIconWithScaleDown(instance, MAKEINTRESOURCEW(IDI_APP), iconSide,
                                  iconSide, &icon);
    state.icon = icon;

    const std::wstring title = Loc::Str(IDS_ABOUT_TITLE);

    // WS_EX_DLGMODALFRAME + ince başlık: ekran alıntısı aracının hakkında
    // penceresinin yeniden boyutlandırılabilir olması için bir sebep yok.
    const HWND window = ::CreateWindowExW(
        WS_EX_DLGMODALFRAME, kWindowClass, title.c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU, x, y, width, height, nullptr, nullptr,
        instance, &state);

    if (window == nullptr) {
        if (icon != nullptr) {
            ::DestroyIcon(icon);
        }
        return;
    }

    g_open = window;
    theme::ApplyToWindow(window);
    ::ShowWindow(window, SW_SHOW);
    ::SetForegroundWindow(window);

    MSG message{};
    while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }

    if (icon != nullptr) {
        ::DestroyIcon(icon);
    }
}

}  // namespace crisp
