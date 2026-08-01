// ThicknessPicker.cpp — bkz. ThicknessPicker.h.
#include "ThicknessPicker.h"

#include "AlphaLayer.h"
#include "ColorSpace.h"
#include "Geometry.h"
#include "Theme.h"
#include "Util.h"

#include <shellscalingapi.h>
#include <windowsx.h>

#include <algorithm>
#include <iterator>

namespace crisp {
namespace {

constexpr const wchar_t* kWindowClass = L"CrispThicknessPicker";

// Tasarım ölçüleri (96 DPI mantıksal piksel).
constexpr int kPad = 6;
constexpr int kRowHeight = 34;
constexpr int kPanelWidth = 188;
constexpr int kLabelWidth = 34;

struct State {
    unsigned dpi = 96;
    COLORREF ink = RGB(255, 255, 255);
    int thickness = 3;
    int hover = -1;
    bool accepted = false;
    bool closing = false;
    int width = 0;
    int height = 0;
    AlphaLayer chrome;
};

[[nodiscard]] int Scale(int value, unsigned dpi) noexcept {
    return ::MulDiv(value, static_cast<int>(dpi), 96);
}

[[nodiscard]] RECT RowBounds(const State& state, int index) noexcept {
    const int pad = Scale(kPad, state.dpi);
    const int height = Scale(kRowHeight, state.dpi);
    return RECT{pad, pad + index * height, state.width - pad,
                pad + (index + 1) * height};
}

[[nodiscard]] int RowAt(const State& state, POINT point) noexcept {
    for (int i = 0; i < static_cast<int>(std::size(kThicknessChoices)); ++i) {
        const RECT bounds = RowBounds(state, i);
        if (::PtInRect(&bounds, point)) {
            return i;
        }
    }
    return -1;
}

void Finish(HWND window, State& state, bool accepted) {
    if (state.closing) {
        return;
    }
    state.closing = true;
    state.accepted = accepted;
    if (::GetCapture() == window) {
        ::ReleaseCapture();
    }
    ::DestroyWindow(window);
}

void Paint(HWND window, State& state) {
    PAINTSTRUCT paint{};
    const HDC dc = ::BeginPaint(window, &paint);
    if (dc == nullptr) {
        return;
    }

    RECT client{};
    ::GetClientRect(window, &client);
    const Palette& colors = theme::Colors();

    const HDC memory = ::CreateCompatibleDC(dc);
    const HBITMAP buffer =
        ::CreateCompatibleBitmap(dc, geom::Width(client), geom::Height(client));
    const HGDIOBJ oldBitmap = ::SelectObject(memory, buffer);

    const HBRUSH surface = ::CreateSolidBrush(colors.surface);
    if (surface != nullptr) {
        ::FillRect(memory, &client, surface);
        ::DeleteObject(surface);
    }

    const int count = static_cast<int>(std::size(kThicknessChoices));

    // 1. AŞAMA — satır zeminleri tek alfa katmanına.
    if (state.chrome.Prepare(memory, POINT{0, 0},
                             static_cast<int>(geom::Width(client)),
                             static_cast<int>(geom::Height(client)))) {
        const int radius = Scale(6, state.dpi);
        for (int i = 0; i < count; ++i) {
            const RECT row = RowBounds(state, i);
            if (kThicknessChoices[i] == state.thickness) {
                state.chrome.FillRoundRect(row, colors.accent, 255, radius);
            } else if (state.hover == i) {
                state.chrome.FillRoundRect(row, colors.text, 26, radius);
            }
        }
        state.chrome.BlendTo(memory);
    }

    // 2. AŞAMA — önizleme çizgileri ve etiketler.
    LOGFONTW font{};
    font.lfHeight = -::MulDiv(9, static_cast<int>(state.dpi), 72);
    font.lfWeight = FW_NORMAL;
    font.lfCharSet = DEFAULT_CHARSET;
    font.lfQuality = CLEARTYPE_QUALITY;
    ::wcscpy_s(font.lfFaceName, L"Segoe UI");
    const HFONT created = ::CreateFontIndirectW(&font);
    const HGDIOBJ oldFont =
        created != nullptr ? ::SelectObject(memory, created) : nullptr;
    ::SetBkMode(memory, TRANSPARENT);

    for (int i = 0; i < count; ++i) {
        const RECT row = RowBounds(state, i);
        const bool selected = kThicknessChoices[i] == state.thickness;
        const COLORREF label = selected ? RGB(255, 255, 255) : colors.textDim;

        // ÇİZGİ SEÇİLİ ÇİZİM RENGİNDE: önizleme kalınlığı gösterdiği kadar
        // rengi de göstermeli. AMA zeminden ayırt edilemiyorsa hiç görünmez —
        // koyu temada siyah kalem tam olarak bunu yapardı — ve o durumda
        // karşıt bir renge düşülür.
        const COLORREF background = selected ? colors.accent : colors.surface;
        const COLORREF line =
            HasContrast(state.ink, background)
                ? state.ink
                : (PrefersDarkInk(background) ? RGB(24, 24, 27)
                                              : RGB(255, 255, 255));

        const int stroke = (std::max)(1, Scale(kThicknessChoices[i], state.dpi));
        const HPEN pen = ::CreatePen(PS_SOLID, stroke, line);
        if (pen != nullptr) {
            const HGDIOBJ oldPen = ::SelectObject(memory, pen);
            const int y = (row.top + row.bottom) / 2;
            const int left = row.left + Scale(14, state.dpi);
            const int right = row.right - Scale(kLabelWidth, state.dpi);
            ::MoveToEx(memory, left, y, nullptr);
            ::LineTo(memory, right, y);
            ::SelectObject(memory, oldPen);
            ::DeleteObject(pen);
        }

        wchar_t text[8];
        ::swprintf_s(text, L"%d", kThicknessChoices[i]);
        RECT area{row.right - Scale(kLabelWidth, state.dpi), row.top,
                  row.right - Scale(8, state.dpi), row.bottom};
        ::SetTextColor(memory, label);
        ::DrawTextW(memory, text, -1, &area,
                    DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    if (oldFont != nullptr) {
        ::SelectObject(memory, oldFont);
    }
    if (created != nullptr) {
        ::DeleteObject(created);
    }

    const HBRUSH edge = ::CreateSolidBrush(colors.border);
    if (edge != nullptr) {
        ::FrameRect(memory, &client, edge);
        ::DeleteObject(edge);
    }

    ::BitBlt(dc, 0, 0, geom::Width(client), geom::Height(client), memory, 0, 0,
             SRCCOPY);

    ::SelectObject(memory, oldBitmap);
    ::DeleteObject(buffer);
    ::DeleteDC(memory);
    ::EndPaint(window, &paint);
}

LRESULT CALLBACK ThicknessProc(HWND window, UINT message, WPARAM wParam,
                               LPARAM lParam) {
    auto* state = reinterpret_cast<State*>(::GetWindowLongPtrW(window, GWLP_USERDATA));

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

        case WM_ACTIVATE:
            if (state != nullptr && LOWORD(wParam) == WA_INACTIVE) {
                Finish(window, *state, false);
            }
            return 0;

        case WM_MOUSEMOVE: {
            if (state == nullptr) {
                break;
            }
            const int row =
                RowAt(*state, POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
            if (row != state->hover) {
                state->hover = row;
                ::InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            if (state == nullptr) {
                break;
            }
            const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            RECT client{};
            ::GetClientRect(window, &client);
            if (!::PtInRect(&client, point)) {
                Finish(window, *state, false);
                return 0;
            }
            const int row = RowAt(*state, point);
            if (row >= 0) {
                state->thickness = kThicknessChoices[row];
                Finish(window, *state, true);
            }
            return 0;
        }

        case WM_KEYDOWN:
            if (state == nullptr) {
                break;
            }
            if (wParam == VK_ESCAPE) {
                Finish(window, *state, false);
            }
            return 0;

        case WM_DESTROY:
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
    wc.lpfnWndProc = ThicknessProc;
    wc.hInstance = instance;
    wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClass;
    registered = ::RegisterClassExW(&wc) != 0;
    return registered;
}

}  // namespace

bool PickThickness(HINSTANCE instance, HWND owner, const RECT& anchor,
                   COLORREF ink, int& thickness) {
    if (!EnsureWindowClass(instance)) {
        return false;
    }

    State state;
    state.ink = ink;
    state.thickness = thickness;

    const HMONITOR monitor = ::MonitorFromRect(&anchor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    RECT work{0, 0, 1920, 1080};
    if (::GetMonitorInfoW(monitor, &info)) {
        work = info.rcWork;
    }
    UINT dpiX = 96;
    UINT dpiY = 96;
    if (SUCCEEDED(::GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
        state.dpi = dpiX;
    }

    const int count = static_cast<int>(std::size(kThicknessChoices));
    state.width = Scale(kPanelWidth, state.dpi);
    state.height = Scale(kPad, state.dpi) * 2 + count * Scale(kRowHeight, state.dpi);

    int x = anchor.left;
    int y = anchor.bottom + Scale(6, state.dpi);
    if (y + state.height > work.bottom) {
        y = anchor.top - Scale(6, state.dpi) - state.height;
    }
    x = (std::min)(x, static_cast<int>(work.right) - state.width);
    x = (std::max)(x, static_cast<int>(work.left));
    y = (std::max)(y, static_cast<int>(work.top));

    const HWND window = ::CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST, kWindowClass, L"", WS_POPUP, x, y,
        state.width, state.height, owner, nullptr, instance, &state);
    if (window == nullptr) {
        LogV(L"Kalınlık listesi oluşturulamadı (hata %lu)", ::GetLastError());
        return false;
    }

    theme::ApplyToWindow(window);
    ::ShowWindow(window, SW_SHOW);
    ::SetForegroundWindow(window);
    ::SetFocus(window);
    ::SetCapture(window);

    MSG message{};
    while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }

    if (!state.accepted) {
        return false;
    }
    thickness = state.thickness;
    return true;
}

}  // namespace crisp
