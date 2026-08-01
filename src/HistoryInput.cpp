// HistoryInput.cpp — Geçmiş penceresinin mesajları, bağlam menüsü ve ömrü.
// Yerleşim ve çizim HistoryWindow.cpp'dedir.
#include "HistoryInternal.h"

#include "ClipboardImage.h"
#include "Geometry.h"
#include "ImageCodec.h"
#include "Localization.h"
#include "MessageWindow.h"
#include "Theme.h"
#include "Util.h"
#include "resource.h"

#include <shellapi.h>
#include <shellscalingapi.h>
#include <windowsx.h>

#include <string>

namespace crisp {
namespace history {
namespace {

constexpr const wchar_t* kWindowClass = L"CrispHistoryWindow";
constexpr int kWidth = 780;
constexpr int kHeight = 540;

// Bağlam menüsü komutları. Kaynak kimlikleriyle çakışmamaları için yerel ve
// küçük tutulur; menü TPM_RETURNCMD ile okunduğu için dışarı sızmazlar.
enum MenuId { kMenuEdit = 1, kMenuCopy, kMenuReveal, kMenuDelete, kMenuClear };

// Aynı anda tek geçmiş penceresi; ikinci çağrı var olanı öne getirir.
HWND g_open = nullptr;

void OnScroll(HWND window, State& state, int newPosition) {
    if (newPosition == state.scroll) {
        return;
    }
    state.scroll = newPosition;
    Layout(window, state);
    UpdateScrollBar(window, state);
    Layout(window, state);
    ::InvalidateRect(window, nullptr, FALSE);
}

LRESULT HandleScrollMessage(HWND window, State& state, WPARAM wParam) {
    SCROLLINFO info{};
    info.cbSize = sizeof(info);
    info.fMask = SIF_ALL;
    ::GetScrollInfo(window, SB_VERT, &info);

    const int line = Scale(kTileHeight / 3, state.dpi);
    int position = state.scroll;
    switch (LOWORD(wParam)) {
        case SB_LINEUP:   position -= line; break;
        case SB_LINEDOWN: position += line; break;
        case SB_PAGEUP:   position -= static_cast<int>(info.nPage); break;
        case SB_PAGEDOWN: position += static_cast<int>(info.nPage); break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION:
            position = info.nTrackPos;
            break;
        case SB_TOP:      position = 0; break;
        case SB_BOTTOM:   position = info.nMax; break;
        default: return 0;
    }
    OnScroll(window, state, position);
    return 0;
}

LRESULT CALLBACK HistoryProc(HWND window, UINT message, WPARAM wParam,
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

        case WM_PAINT:
            if (state != nullptr) {
                Paint(window, *state);
            }
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_SIZE:
            if (state != nullptr) {
                Layout(window, *state);
                UpdateScrollBar(window, *state);
                Layout(window, *state);
                ::InvalidateRect(window, nullptr, FALSE);
            }
            return 0;

        case WM_VSCROLL:
            if (state != nullptr) {
                return HandleScrollMessage(window, *state, wParam);
            }
            return 0;

        case WM_MOUSEWHEEL: {
            if (state == nullptr) {
                break;
            }
            const int notches = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
            OnScroll(window, *state,
                     state->scroll - notches * Scale(kTileHeight / 2, state->dpi));
            return 0;
        }

        case WM_MOUSEMOVE: {
            if (state == nullptr) {
                break;
            }
            const POINT cursor{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            const int hit = TileAt(*state, cursor);
            if (hit != state->hovered) {
                state->hovered = hit;
                ::InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        }

        case WM_MOUSELEAVE:
            if (state != nullptr && state->hovered != -1) {
                state->hovered = -1;
                ::InvalidateRect(window, nullptr, FALSE);
            }
            return 0;

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN: {
            if (state == nullptr) {
                break;
            }
            const POINT cursor{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            const int hit = TileAt(*state, cursor);
            // Boşluğa sağ tıklamak seçimi BOZMAZ: menüdeki "Sil" o an seçili
            // olana işler ve seçimi sıfırlamak, kullanıcının niyetiyle
            // menünün hedefini ayırırdı.
            if (hit >= 0 && hit != state->selected) {
                state->selected = hit;
                ::InvalidateRect(window, nullptr, FALSE);
            }
            if (message == WM_RBUTTONDOWN) {
                POINT screen = cursor;
                ::ClientToScreen(window, &screen);
                ShowContextMenu(window, *state, screen);
            }
            return 0;
        }

        case WM_LBUTTONDBLCLK:
            if (state != nullptr) {
                const POINT cursor{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                if (TileAt(*state, cursor) >= 0) {
                    EditSelected(window, *state);
                }
            }
            return 0;

        case WM_KEYDOWN: {
            if (state == nullptr) {
                break;
            }
            const bool control = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
            switch (wParam) {
                case VK_ESCAPE: ::DestroyWindow(window); return 0;
                case VK_RETURN: EditSelected(window, *state); return 0;
                case VK_DELETE: DeleteSelected(window, *state); return 0;
                case VK_LEFT:   MoveSelection(window, *state, -1); return 0;
                case VK_RIGHT:  MoveSelection(window, *state, 1); return 0;
                case VK_UP:     MoveSelection(window, *state, -state->columns); return 0;
                case VK_DOWN:   MoveSelection(window, *state, state->columns); return 0;
                case VK_HOME:   MoveSelection(window, *state, -9999); return 0;
                case VK_END:    MoveSelection(window, *state, 9999); return 0;
                case 'C':
                    if (control) {
                        CopySelected(window, *state);
                        return 0;
                    }
                    break;
                default: break;
            }
            break;
        }

        case WM_DPICHANGED: {
            if (state != nullptr) {
                state->dpi = HIWORD(wParam);
                const auto* suggested = reinterpret_cast<const RECT*>(lParam);
                ::SetWindowPos(window, nullptr, suggested->left, suggested->top,
                               static_cast<int>(geom::Width(*suggested)),
                               static_cast<int>(geom::Height(*suggested)),
                               SWP_NOZORDER | SWP_NOACTIVATE);
                // Küçük resimler DPI'ya göre üretilir; yeniden ölçeklenmezse
                // yüksek DPI'lı monitörde bulanık kalırlar.
                ReloadTiles(window, *state);
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
    wc.lpfnWndProc = HistoryProc;
    wc.hInstance = instance;
    wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClass;
    wc.hIcon = ::LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP));
    wc.style = CS_DBLCLKS;   // WM_LBUTTONDBLCLK bunsuz hiç gelmez
    wc.hbrBackground = nullptr;

    registered = ::RegisterClassExW(&wc) != 0;
    return registered;
}

}  // namespace
}  // namespace history

HistoryResult ShowHistoryWindow(HINSTANCE instance, HistoryStore& store) {
    if (history::g_open != nullptr) {
        ::SetForegroundWindow(history::g_open);
        return HistoryResult{};
    }
    if (!history::EnsureWindowClass(instance)) {
        return HistoryResult{};
    }

    history::State state;
    state.instance = instance;
    state.store = &store;

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

    const int width = history::Scale(history::kWidth, state.dpi);
    const int height = history::Scale(history::kHeight, state.dpi);
    const int x = work.left + (static_cast<int>(geom::Width(work)) - width) / 2;
    const int y = work.top + (static_cast<int>(geom::Height(work)) - height) / 2;

    const std::wstring title = Loc::Str(IDS_HISTORY_TITLE);
    const HWND window = ::CreateWindowExW(
        0, history::kWindowClass, title.c_str(),
        WS_OVERLAPPEDWINDOW | WS_VSCROLL, x, y, width, height, nullptr, nullptr,
        instance, &state);

    if (window == nullptr) {
        return HistoryResult{};
    }

    history::g_open = window;
    theme::ApplyToWindow(window);
    history::ReloadTiles(window, state);
    ::ShowWindow(window, SW_SHOW);
    ::SetForegroundWindow(window);

    MSG message{};
    while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }

    return std::move(state.result);
}

}  // namespace crisp
