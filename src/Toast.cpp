// Toast.cpp — bkz. Toast.h.
#include "Toast.h"

#include "Geometry.h"
#include "ImageTransform.h"
#include "Theme.h"
#include "Util.h"

#include <dwmapi.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <windowsx.h>

#include <memory>

namespace crisp {
namespace {

constexpr const wchar_t* kWindowClass = L"CrispToastWindow";

// Tasarım ölçüleri (96 DPI mantıksal piksel).
constexpr int kWidth = 320;
constexpr int kHeight = 84;
constexpr int kPad = 12;
constexpr int kThumbSide = 60;
constexpr int kScreenMargin = 16;

constexpr UINT_PTR kTimerId = 1;
constexpr UINT kTickMs = 25;

// Görünme, bekleme ve sönme süreleri (ms). Bekleme, okumaya yetecek kadar
// uzun ama yolu tıkayacak kadar değil.
constexpr int kFadeInMs = 120;
constexpr int kHoldMs = 2600;
constexpr int kFadeOutMs = 320;

struct ToastState {
    Image thumbnail;
    std::wstring title;
    std::wstring detail;
    std::wstring openPath;
    unsigned dpi = 96;
    int elapsed = 0;
    bool hovered = false;
};

// TEK BİLDİRİM: ikinci yakalama birincinin bildirimini devralır. Üst üste
// binen bildirimler ekranın köşesini kaplardı ve hiçbiri okunmazdı.
HWND g_window = nullptr;
std::unique_ptr<ToastState> g_state;

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

// Kareyi dolduracak şekilde ölçekleyip ortadan kırpar.
[[nodiscard]] bool SquareThumbnail(const Image& source, int side, Image& out) {
    if (!source.Valid() || side <= 0) {
        return false;
    }
    const double byWidth = static_cast<double>(side) / source.Width();
    const double byHeight = static_cast<double>(side) / source.Height();
    const double factor = byWidth > byHeight ? byWidth : byHeight;
    if (factor >= 1.0) {
        return CropImage(source, 0, 0, source.Width(), source.Height(), out);
    }

    int width = static_cast<int>(source.Width() * factor + 0.5);
    int height = static_cast<int>(source.Height() * factor + 0.5);
    width = width < side ? side : width;
    height = height < side ? side : height;

    Image covered;
    if (!ScaleImage(source, width, height, covered)) {
        return false;
    }
    return CropImage(covered, (width - side) / 2, (height - side) / 4, side, side,
                     out);
}

void Paint(HWND window, ToastState& state) {
    PAINTSTRUCT paint{};
    const HDC dc = ::BeginPaint(window, &paint);
    if (dc == nullptr) {
        return;
    }

    RECT client{};
    ::GetClientRect(window, &client);
    const int width = static_cast<int>(geom::Width(client));
    const int height = static_cast<int>(geom::Height(client));

    const HDC memory = ::CreateCompatibleDC(dc);
    const HBITMAP buffer = ::CreateCompatibleBitmap(dc, width, height);
    const HGDIOBJ oldBitmap = ::SelectObject(memory, buffer);

    const Palette& colors = theme::Colors();
    const unsigned dpi = state.dpi;
    const int pad = Scale(kPad, dpi);

    FillRectColor(memory, client, colors.surfaceAlt);

    // İnce kenarlık: koyu temada bildirim koyu bir arka planın üstüne
    // düştüğünde sınırı belirsiz kalırdı.
    {
        const HPEN pen = ::CreatePen(PS_SOLID, 1, colors.border);
        const HGDIOBJ oldPen = ::SelectObject(memory, pen);
        const HGDIOBJ oldBrush = ::SelectObject(memory, ::GetStockObject(NULL_BRUSH));
        ::Rectangle(memory, 0, 0, width, height);
        ::SelectObject(memory, oldBrush);
        ::SelectObject(memory, oldPen);
        ::DeleteObject(pen);
    }

    int textLeft = pad;
    if (state.thumbnail.Valid()) {
        const int side = state.thumbnail.Width();
        const int y = (height - state.thumbnail.Height()) / 2;
        FillRectColor(memory,
                      RECT{pad - 1, y - 1, pad + side + 1,
                           y + state.thumbnail.Height() + 1},
                      colors.border);
        const HDC imageDc = ::CreateCompatibleDC(dc);
        const HGDIOBJ oldImage = ::SelectObject(imageDc, state.thumbnail.Handle());
        ::BitBlt(memory, pad, y, side, state.thumbnail.Height(), imageDc, 0, 0,
                 SRCCOPY);
        ::SelectObject(imageDc, oldImage);
        ::DeleteDC(imageDc);
        textLeft = pad + side + Scale(12, dpi);
    }

    const HFONT fontTitle = CreateUiFont(dpi, 10, FW_SEMIBOLD);
    const HFONT fontDetail = CreateUiFont(dpi, 9, FW_NORMAL);

    ::SetBkMode(memory, TRANSPARENT);
    const int textTop = height / 2 - Scale(20, dpi);

    RECT titleArea{textLeft, textTop, width - pad, textTop + Scale(22, dpi)};
    const HGDIOBJ old1 = ::SelectObject(memory, fontTitle);
    ::SetTextColor(memory, colors.text);
    ::DrawTextW(memory, state.title.c_str(), -1, &titleArea,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS |
                    DT_NOPREFIX);
    ::SelectObject(memory, old1);

    RECT detailArea{textLeft, titleArea.bottom, width - pad,
                    titleArea.bottom + Scale(20, dpi)};
    const HGDIOBJ old2 = ::SelectObject(memory, fontDetail);
    ::SetTextColor(memory, state.hovered && !state.openPath.empty() ? colors.accent
                                                                   : colors.textDim);
    ::DrawTextW(memory, state.detail.c_str(), -1, &detailArea,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_PATH_ELLIPSIS |
                    DT_NOPREFIX);
    ::SelectObject(memory, old2);

    ::BitBlt(dc, 0, 0, width, height, memory, 0, 0, SRCCOPY);

    ::DeleteObject(fontTitle);
    ::DeleteObject(fontDetail);
    ::SelectObject(memory, oldBitmap);
    ::DeleteObject(buffer);
    ::DeleteDC(memory);
    ::EndPaint(window, &paint);
}

// Geçen süreye göre 0-255 arası opaklık.
[[nodiscard]] BYTE AlphaForElapsed(int elapsed, bool hovered) noexcept {
    if (hovered) {
        // İmleç üstündeyken sönmez: kullanıcı tıklamak üzereyken bildirimin
        // altından kaçması, en sinir bozucu arayüz hatalarından biridir.
        return 255;
    }
    if (elapsed < kFadeInMs) {
        return static_cast<BYTE>(255 * elapsed / kFadeInMs);
    }
    const int fadeStart = kFadeInMs + kHoldMs;
    if (elapsed < fadeStart) {
        return 255;
    }
    const int into = elapsed - fadeStart;
    if (into >= kFadeOutMs) {
        return 0;
    }
    return static_cast<BYTE>(255 - 255 * into / kFadeOutMs);
}

LRESULT CALLBACK ToastProc(HWND window, UINT message, WPARAM wParam,
                           LPARAM lParam) {
    switch (message) {
        case WM_PAINT:
            if (g_state) {
                Paint(window, *g_state);
            }
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_TIMER: {
            if (!g_state) {
                return 0;
            }
            g_state->elapsed += static_cast<int>(kTickMs);

            POINT cursor{};
            ::GetCursorPos(&cursor);
            RECT bounds{};
            ::GetWindowRect(window, &bounds);
            const bool hovered = ::PtInRect(&bounds, cursor) != FALSE;
            if (hovered != g_state->hovered) {
                g_state->hovered = hovered;
                if (hovered) {
                    // Beklemeyi baştan başlat, yoksa imleç çekilir çekilmez
                    // bildirim anında kaybolurdu.
                    g_state->elapsed = kFadeInMs;
                }
                ::InvalidateRect(window, nullptr, FALSE);
            }

            const BYTE alpha = AlphaForElapsed(g_state->elapsed, hovered);
            if (alpha == 0) {
                ::DestroyWindow(window);
                return 0;
            }
            ::SetLayeredWindowAttributes(window, 0, alpha, LWA_ALPHA);
            return 0;
        }

        case WM_LBUTTONUP:
            if (g_state && !g_state->openPath.empty()) {
                ::ShellExecuteW(nullptr, L"open", g_state->openPath.c_str(),
                                nullptr, nullptr, SW_SHOWNORMAL);
            }
            ::DestroyWindow(window);
            return 0;

        case WM_RBUTTONUP:
            ::DestroyWindow(window);
            return 0;

        case WM_SETCURSOR:
            if (g_state && !g_state->openPath.empty()) {
                ::SetCursor(::LoadCursorW(nullptr, IDC_HAND));
                return TRUE;
            }
            break;

        // Bildirim ODAK ALMAZ: kullanıcı yazarken açılırsa tuş vuruşlarını
        // çalması, yakalama almanın en beklenmedik yan etkisi olurdu.
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;

        case WM_DESTROY:
            ::KillTimer(window, kTimerId);
            g_window = nullptr;
            g_state.reset();
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
    wc.lpfnWndProc = ToastProc;
    wc.hInstance = instance;
    wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClass;
    wc.hbrBackground = nullptr;
    registered = ::RegisterClassExW(&wc) != 0;
    return registered;
}

}  // namespace

void ShowCaptureToast(HINSTANCE instance, const Image& capture,
                      const std::wstring& title, const std::wstring& detail,
                      const std::wstring& openPath) {
    if (!EnsureWindowClass(instance)) {
        return;
    }
    CloseCaptureToast();
    if (g_window != nullptr) {
        // Önceki kapanamadı. İkincisini üstüne koymak durumu kötüleştirir:
        // `g_state` ikisi tarafından paylaşılır ve hangisinin ne çizdiği
        // sıraya kalır.
        return;
    }

    auto state = std::make_unique<ToastState>();
    state->title = title;
    state->detail = detail;
    state->openPath = openPath;

    // Bildirim, YAKALAMANIN ALINDIĞI monitörde açılır: iki ekranlı bir
    // kurulumda diğer ekranın köşesinde belirmesi, hiç belirmemesiyle aynı
    // kapıya çıkardı.
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
    state->dpi = dpiX;

    (void)SquareThumbnail(capture, Scale(kThumbSide, state->dpi),
                          state->thumbnail);

    const int width = Scale(kWidth, state->dpi);
    const int height = Scale(kHeight, state->dpi);
    const int margin = Scale(kScreenMargin, state->dpi);
    const int x = work.right - width - margin;
    const int y = work.bottom - height - margin;

    const HWND window = ::CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        kWindowClass, L"Crisp", WS_POPUP, x, y, width, height, nullptr, nullptr,
        instance, nullptr);
    if (window == nullptr) {
        return;
    }

    g_window = window;
    g_state = std::move(state);

    // Windows 11 köşe yuvarlaması. Eski sürümlerde çağrı başarısız olur ve
    // pencere düz köşeli kalır — hata değil, yalnızca daha az süslü.
    const DWORD roundedSmall = 3;   // DWMWCP_ROUNDSMALL
    (void)::DwmSetWindowAttribute(window, 33, &roundedSmall,
                                  sizeof(roundedSmall));

    ::SetLayeredWindowAttributes(window, 0, 0, LWA_ALPHA);
    ::ShowWindow(window, SW_SHOWNOACTIVATE);
    ::SetTimer(window, kTimerId, kTickMs, nullptr);
}

void CloseCaptureToast() noexcept {
    if (g_window == nullptr) {
        return;
    }
    // BAŞKA BİR İŞ PARÇACIĞINDAN YOK EDİLEMEZ. `DestroyWindow` yalnızca
    // pencereyi OLUŞTURAN iş parçacığında çalışır; başka yerden çağrıldığında
    // sessizce FALSE döner. Kendiliğinden yükleme bir süre bildirimini kendi
    // iş parçacığından açtı: birinci bildirim kapanmıyor, ikincisi ise mesaj
    // döngüsü olmayan bir iş parçacığında doğuyor ve saydam kalıyordu —
    // kullanıcının gördüğü, imleç üstüne gelince beliriveren bir bildirimdi.
    if (::DestroyWindow(g_window) == FALSE) {
        LogV(L"Bildirim kapatılamadı: pencere başka bir iş parçacığına ait");
        return;   // g_window DURSUN: pencere hâlâ orada ve sahibi biz değiliz
    }
    g_window = nullptr;
}

}  // namespace crisp
