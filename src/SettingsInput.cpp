// SettingsInput.cpp — Ayarlar penceresinin mesajları, okuma/yazma ve ömrü.
// Denetimler, yerleşim ve çizim SettingsWindow.cpp'dedir.
#include "SettingsInternal.h"

#include "Geometry.h"
#include "History.h"
#include "HotkeyEdit.h"
#include "ImageCodec.h"
#include "Localization.h"
#include "MessageWindow.h"
#include "SettingsWindow.h"
#include "Theme.h"
#include "Util.h"
#include "resource.h"

#include <commctrl.h>
#include <shellscalingapi.h>
#include <shlobj.h>

#include <string>

namespace crisp {
namespace settings_ui {
namespace {

constexpr const wchar_t* kWindowClass = L"CrispSettingsWindow";

HWND g_open = nullptr;


LRESULT CALLBACK SettingsProc(HWND window, UINT message, WPARAM wParam,
                              LPARAM lParam) {
    auto* state =
        reinterpret_cast<State*>(::GetWindowLongPtrW(window, GWLP_USERDATA));

    switch (message) {
        case WM_NCCREATE: {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            ::SetWindowLongPtrW(window, GWLP_USERDATA,
                                reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return ::DefWindowProcW(window, message, wParam, lParam);
        }

        case WM_CREATE:
            if (state != nullptr) {
                BuildControls(window, *state);
                LoadIntoControls(window, *state);
            }
            return 0;

        case WM_PAINT:
            if (state != nullptr) {
                Paint(window, *state);
            }
            return 0;

        case WM_ERASEBKGND:
            return 1;

        // Koyu temada denetim metinleri siyah kalırdı; WM_CTLCOLOR* zemin
        // fırçasını ve metin rengini bizim vermemizi sağlar.
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            if (state == nullptr) {
                break;
            }
            const HDC dc = reinterpret_cast<HDC>(wParam);
            ::SetTextColor(dc, theme::Colors().text);
            ::SetBkMode(dc, TRANSPARENT);
            return reinterpret_cast<LRESULT>(state->backgroundBrush);
        }

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            if (state == nullptr || !theme::IsDark()) {
                break;
            }
            const HDC dc = reinterpret_cast<HDC>(wParam);
            ::SetTextColor(dc, theme::Colors().text);
            ::SetBkColor(dc, theme::Colors().surfaceAlt);
            return reinterpret_cast<LRESULT>(state->backgroundBrush);
        }

        case WM_COMMAND: {
            if (state == nullptr) {
                break;
            }
            switch (LOWORD(wParam)) {
                // IDOK / IDCANCEL, IsDialogMessage'ın Enter ve Esc için
                // gönderdiği kimliklerdir.
                //
                // BUNLAR OLMADAN ESC ÇALIŞMIYORDU: odak bir açılır kutudayken
                // WM_KEYDOWN pencereye değil denetime gider ve pencerenin
                // kendi Esc dalı hiç çalışmaz. Duman testinde ayarlar penceresi
                // Esc'e rağmen açık kaldı ve arkasındaki bütün denemeleri
                // engelledi.
                case IDOK:
                case kIdOk:
                    ReadFromControls(window, *state);
                    state->working.Clamp();
                    state->accepted = true;
                    ::DestroyWindow(window);
                    return 0;
                case IDCANCEL:
                case kIdCancel:
                    ::DestroyWindow(window);
                    return 0;
                case kIdReset:
                    ResetToDefaults(window, *state);
                    return 0;
                case kIdHistoryClear:
                    ClearHistory(window, *state);
                    return 0;
                case kIdBrowse: {
                    std::wstring folder = GetText(window, kIdFolder);
                    if (PickFolder(window, folder)) {
                        ::SetDlgItemTextW(window, kIdFolder, folder.c_str());
                    }
                    return 0;
                }
                default:
                    break;
            }
            break;
        }

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                ::DestroyWindow(window);
                return 0;
            }
            break;

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
    wc.lpfnWndProc = SettingsProc;
    wc.hInstance = instance;
    wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClass;
    wc.hIcon = ::LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP));
    wc.hbrBackground = nullptr;
    registered = ::RegisterClassExW(&wc) != 0;
    return registered;
}

}  // namespace


}  // namespace settings_ui

bool ShowSettingsWindow(HINSTANCE instance, Settings& settings) {
    using namespace settings_ui;

    if (g_open != nullptr) {
        ::SetForegroundWindow(g_open);
        return false;
    }
    if (!EnsureWindowClass(instance)) {
        return false;
    }

    State state;
    state.instance = instance;
    state.target = &settings;
    state.working = settings;

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

    LOGFONTW font{};
    font.lfHeight = -::MulDiv(9, static_cast<int>(state.dpi), 72);
    font.lfWeight = FW_NORMAL;
    font.lfCharSet = DEFAULT_CHARSET;
    font.lfQuality = CLEARTYPE_QUALITY;
    ::wcscpy_s(font.lfFaceName, L"Segoe UI");
    state.font = ::CreateFontIndirectW(&font);
    font.lfHeight = -::MulDiv(10, static_cast<int>(state.dpi), 72);
    font.lfWeight = FW_SEMIBOLD;
    state.groupFont = ::CreateFontIndirectW(&font);
    state.backgroundBrush = ::CreateSolidBrush(theme::Colors().surface);

    // İstemci alanı tam olarak tasarım ölçüsünde olmalı; pencere ölçüsü
    // verilseydi kenarlık ve başlık çubuğu içeriden çalardı ve alt düğmeler
    // kırpılırdı.
    RECT desired{0, 0, Scale(kWidth, state.dpi), Scale(kHeight, state.dpi)};
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    ::AdjustWindowRectEx(&desired, style, FALSE, 0);
    const int width = static_cast<int>(geom::Width(desired));
    const int height = static_cast<int>(geom::Height(desired));
    const int x = work.left + (static_cast<int>(geom::Width(work)) - width) / 2;
    const int y = work.top + (static_cast<int>(geom::Height(work)) - height) / 2;

    const HWND window = ::CreateWindowExW(
        WS_EX_DLGMODALFRAME, kWindowClass, Loc::Str(IDS_SETTINGS_TITLE).c_str(),
        style, x, y, width, height, nullptr, nullptr, instance, &state);

    if (window == nullptr) {
        return false;
    }

    g_open = window;
    theme::ApplyToWindow(window);
    ::ShowWindow(window, SW_SHOW);
    ::SetForegroundWindow(window);

    MSG message{};
    while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
        // IsDialogMessage OLMADAN Tab tuşu denetimler arasında dolaşmaz ve
        // pencere klavyeyle kullanılamaz hâle gelir.
        if (!::IsDialogMessageW(window, &message)) {
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        }
    }

    if (state.font != nullptr) {
        ::DeleteObject(state.font);
    }
    if (state.groupFont != nullptr) {
        ::DeleteObject(state.groupFont);
    }
    if (state.backgroundBrush != nullptr) {
        ::DeleteObject(state.backgroundBrush);
    }

    if (state.accepted) {
        settings = state.working;
    }
    return state.accepted;
}

}  // namespace crisp
