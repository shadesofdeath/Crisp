// AboutWindow.cpp — bkz. AboutWindow.h.
#include "AboutWindow.h"

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
namespace {

constexpr const wchar_t* kWindowClass = L"CrispAboutWindow";
constexpr const wchar_t* kRepositoryUrl = L"https://github.com/shadesofdeath/Crisp";
constexpr const wchar_t* kVersionText = L"0.3.0";

// Tasarım ölçüleri (96 DPI'da mantıksal piksel).
constexpr int kWidth = 460;
constexpr int kHeight = 320;
constexpr int kPad = 24;
constexpr int kIconSide = 72;

struct AboutState {
    HICON icon = nullptr;
    unsigned dpi = 96;
    RECT linkRect{};    // GitHub bağlantısının tıklanabilir alanı
    RECT closeRect{};
    bool linkHot = false;
    bool closeHot = false;
};

// Aynı anda birden fazla hakkında penceresi anlamsız; ikinci çağrı mevcut
// olanı öne getirir.
HWND g_open = nullptr;

[[nodiscard]] int Scale(int value, unsigned dpi) noexcept {
    return ::MulDiv(value, static_cast<int>(dpi), 96);
}

[[nodiscard]] HFONT CreateUiFont(unsigned dpi, int pointSize, int weight) {
    LOGFONTW font{};
    font.lfHeight = -::MulDiv(pointSize, static_cast<int>(dpi), 72);
    font.lfWeight = weight;
    font.lfCharSet = DEFAULT_CHARSET;
    font.lfQuality = CLEARTYPE_QUALITY;
    ::wcscpy_s(font.lfFaceName, L"Segoe UI");
    return ::CreateFontIndirectW(&font);
}

void FillRectColor(HDC dc, const RECT& r, COLORREF color) {
    const HBRUSH brush = ::CreateSolidBrush(color);
    if (brush == nullptr) {
        return;
    }
    ::FillRect(dc, &r, brush);
    ::DeleteObject(brush);
}

void DrawFrame(HDC dc, const RECT& r, int thickness, COLORREF color) {
    FillRectColor(dc, RECT{r.left, r.top, r.right, r.top + thickness}, color);
    FillRectColor(dc, RECT{r.left, r.bottom - thickness, r.right, r.bottom}, color);
    FillRectColor(dc, RECT{r.left, r.top, r.left + thickness, r.bottom}, color);
    FillRectColor(dc, RECT{r.right - thickness, r.top, r.right, r.bottom}, color);
}

// Metni verilen konuma çizer ve kapladığı yüksekliği döndürür; çağıran bir
// sonraki satırı buna göre yerleştirir.
int DrawLine(HDC dc, const std::wstring& text, int x, int y, int width,
             HFONT font, COLORREF color, UINT format = DT_WORDBREAK) {
    if (text.empty()) {
        return 0;
    }
    const HGDIOBJ oldFont = ::SelectObject(dc, font);
    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, color);

    RECT area{x, y, x + width, y + 4000};
    const int height =
        ::DrawTextW(dc, text.c_str(), -1, &area, format | DT_NOPREFIX);
    ::SelectObject(dc, oldFont);
    return height;
}

void Paint(HWND window, AboutState& state) {
    PAINTSTRUCT paint{};
    const HDC dc = ::BeginPaint(window, &paint);
    if (dc == nullptr) {
        return;
    }

    RECT client{};
    ::GetClientRect(window, &client);

    // Çift tamponlama: metin ve simge tek tek çizilirken arka planın kısa
    // süreliğine görünmesi titreme olarak fark edilir.
    const HDC memory = ::CreateCompatibleDC(dc);
    const HBITMAP buffer = ::CreateCompatibleBitmap(dc, geom::Width(client),
                                                    geom::Height(client));
    const HGDIOBJ oldBitmap = ::SelectObject(memory, buffer);

    const Palette& colors = theme::Colors();
    const unsigned dpi = state.dpi;

    FillRectColor(memory, client, colors.surface);

    const int pad = Scale(kPad, dpi);
    const int iconSide = Scale(kIconSide, dpi);

    if (state.icon != nullptr) {
        ::DrawIconEx(memory, pad, pad, state.icon, iconSide, iconSide, 0, nullptr,
                     DI_NORMAL);
    }

    const int textLeft = pad + iconSide + Scale(20, dpi);
    const int textWidth = geom::Width(client) - textLeft - pad;
    int y = pad;

    const HFONT fontTitle = CreateUiFont(dpi, 20, FW_SEMIBOLD);
    const HFONT fontBody = CreateUiFont(dpi, 10, FW_NORMAL);
    const HFONT fontSmall = CreateUiFont(dpi, 9, FW_NORMAL);

    y += DrawLine(memory, L"Crisp", textLeft, y, textWidth, fontTitle,
                  colors.text, DT_SINGLELINE);
    y += DrawLine(memory, std::wstring(L"v") + kVersionText, textLeft, y,
                  textWidth, fontSmall, colors.textDim, DT_SINGLELINE);
    y += Scale(10, dpi);
    y += DrawLine(memory, Loc::Str(IDS_ABOUT_TAGLINE), textLeft, y, textWidth,
                  fontBody, colors.text);

    // Açıklama, simgenin altından tam genişlikte devam eder: iki sütun
    // hizasını korumak için sol kenar simgeyle aynı yerden başlar.
    y = pad + iconSide + Scale(18, dpi);
    if (y < pad + iconSide) {
        y = pad + iconSide + Scale(18, dpi);
    }
    const int fullWidth = geom::Width(client) - pad * 2;
    y += DrawLine(memory, Loc::Str(IDS_ABOUT_BODY), pad, y, fullWidth, fontBody,
                  colors.textDim);
    y += Scale(12, dpi);

    // Ayraç
    FillRectColor(memory, RECT{pad, y, geom::Width(client) - pad, y + 1},
                  colors.border);
    y += Scale(12, dpi);

    y += DrawLine(memory,
                  Loc::Str(IsOcrAvailable() ? IDS_ABOUT_OCR_YES : IDS_ABOUT_OCR_NO),
                  pad, y, fullWidth, fontSmall, colors.textDim, DT_SINGLELINE);
    y += Scale(4, dpi);
    y += DrawLine(memory, L"MIT · © 2026 ShadesOfDeath", pad, y, fullWidth,
                  fontSmall, colors.textDim, DT_SINGLELINE);

    // GitHub bağlantısı
    {
        const HGDIOBJ oldFont = ::SelectObject(memory, fontSmall);
        RECT measure{0, 0, 0, 0};
        ::DrawTextW(memory, kRepositoryUrl, -1, &measure,
                    DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
        ::SelectObject(memory, oldFont);

        state.linkRect = RECT{pad, y + Scale(6, dpi), pad + geom::Width(measure),
                              y + Scale(6, dpi) + geom::Height(measure)};
        DrawLine(memory, kRepositoryUrl, state.linkRect.left, state.linkRect.top,
                 fullWidth, fontSmall, colors.accent, DT_SINGLELINE);
        if (state.linkHot) {
            // Altı çizili: bağlantı olduğu ancak imleç üstündeyken belli olur.
            FillRectColor(memory,
                          RECT{state.linkRect.left, state.linkRect.bottom - 1,
                               state.linkRect.right, state.linkRect.bottom},
                          colors.accent);
        }
    }

    // Kapat düğmesi — sağ altta.
    {
        const int buttonWidth = Scale(96, dpi);
        const int buttonHeight = Scale(32, dpi);
        state.closeRect = RECT{geom::Width(client) - pad - buttonWidth,
                               geom::Height(client) - pad - buttonHeight,
                               geom::Width(client) - pad,
                               geom::Height(client) - pad};

        FillRectColor(memory, state.closeRect,
                      state.closeHot ? colors.accent : colors.surfaceAlt);
        DrawFrame(memory, state.closeRect, 1,
                  state.closeHot ? colors.accent : colors.border);

        const HGDIOBJ oldFont = ::SelectObject(memory, fontBody);
        ::SetBkMode(memory, TRANSPARENT);
        ::SetTextColor(memory, state.closeHot ? RGB(255, 255, 255) : colors.text);
        RECT textArea = state.closeRect;
        const std::wstring closeText = Loc::Str(IDS_PIN_CLOSE);
        ::DrawTextW(memory, closeText.c_str(), -1, &textArea,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        ::SelectObject(memory, oldFont);
    }

    ::BitBlt(dc, 0, 0, geom::Width(client), geom::Height(client), memory, 0, 0,
             SRCCOPY);

    ::DeleteObject(fontTitle);
    ::DeleteObject(fontBody);
    ::DeleteObject(fontSmall);
    ::SelectObject(memory, oldBitmap);
    ::DeleteObject(buffer);
    ::DeleteDC(memory);
    ::EndPaint(window, &paint);
}

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
