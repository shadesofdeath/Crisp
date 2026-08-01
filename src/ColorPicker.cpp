// ColorPicker.cpp — Panelin ömrü, yerleşimi ve mesajları. Çizim
// ColorPickerPaint.cpp'de.
#include "ColorPickerInternal.h"

#include "ColorPicker.h"
#include "Geometry.h"
#include "Overlay.h"
#include "Theme.h"
#include "Util.h"

#include <shellscalingapi.h>
#include <windowsx.h>

#include <algorithm>
#include <cwctype>

namespace crisp {
namespace picker {
namespace {

constexpr const wchar_t* kWindowClass = L"CrispColorPicker";

[[nodiscard]] double Fraction(int value, int span) noexcept {
    if (span <= 1) {
        return 0.0;
    }
    const double f = static_cast<double>(value) / static_cast<double>(span - 1);
    return f < 0.0 ? 0.0 : (f > 1.0 ? 1.0 : f);
}

void Remember(std::vector<COLORREF>& recent, COLORREF color) {
    recent.erase(std::remove(recent.begin(), recent.end(), color), recent.end());
    recent.insert(recent.begin(), color);
    if (recent.size() > static_cast<size_t>(kColumns)) {
        recent.resize(static_cast<size_t>(kColumns));
    }
}

void UpdateFromSquare(State& state, POINT point) {
    const RECT& square = state.metrics.square;
    state.hsv.saturation =
        Fraction(point.x - square.left, static_cast<int>(geom::Width(square)));
    state.hsv.value =
        1.0 - Fraction(point.y - square.top, static_cast<int>(geom::Height(square)));
    SyncFromHsv(state);
}

void UpdateFromHue(State& state, POINT point) {
    const RECT& strip = state.metrics.hue;
    state.hsv.hue =
        Fraction(point.y - strip.top, static_cast<int>(geom::Height(strip))) * 360.0;
    SyncFromHsv(state);
}

// FARE YAKALAMASI panel açık olduğu sürece bizde durur. Sebep: yakalama
// olmasaydı dışarıya yapılan tıklama önce arkadaki düzenleyiciye giderdi ve
// paneli kapatmak isteyen kullanıcı, tuvale bir ok çizmiş olurdu. Açılır
// menülerin çalışma biçimi de budur.
void GrabMouse(HWND window) {
    if (::GetCapture() != window) {
        ::SetCapture(window);
    }
}

// Ekrandan renk alır. Panel önce GİZLENİR: kaplama masaüstünü dondurarak
// çalışır ve panel açık kalsaydı kullanıcı kendi panelinden renk seçebilirdi.
void RunEyedropper(HWND window, State& state) {
    state.suspended = true;
    if (::GetCapture() == window) {
        ::ReleaseCapture();
    }
    ::ShowWindow(window, SW_HIDE);

    Image frozen;
    const OverlayResult result = RunSelectionOverlay(
        state.instance, state.settings, OverlayMode::ColorPick, false, frozen);

    ::ShowWindow(window, SW_SHOW);
    ::SetForegroundWindow(window);
    GrabMouse(window);
    state.suspended = false;

    if (result.accepted) {
        SetColor(state, RGB((result.pickedColor >> 16) & 0xFFu,
                            (result.pickedColor >> 8) & 0xFFu,
                            result.pickedColor & 0xFFu));
    }
    ::InvalidateRect(window, nullptr, FALSE);
}

void Finish(HWND window, State& state, bool accepted) {
    if (state.suspended) {
        return;   // kapanma zaten başladı; ikinci DestroyWindow anlamsız
    }
    state.suspended = true;
    state.accepted = accepted;
    if (::GetCapture() == window) {
        ::ReleaseCapture();
    }
    ::DestroyWindow(window);
}

void OnKeyDown(HWND window, State& state, WPARAM key) {
    if (key == VK_ESCAPE) {
        Finish(window, state, false);
        return;
    }
    if (key == VK_RETURN) {
        Finish(window, state, true);
        return;
    }
    if (key == VK_BACK && state.hexFocus) {
        if (!state.hexText.empty()) {
            state.hexText.pop_back();
        }
        ::InvalidateRect(window, nullptr, FALSE);
    }
}

void OnChar(HWND window, State& state, wchar_t ch) {
    if (!state.hexFocus) {
        return;
    }
    const bool digit = (ch >= L'0' && ch <= L'9') || (ch >= L'a' && ch <= L'f') ||
                       (ch >= L'A' && ch <= L'F');
    if (!digit || state.hexText.size() >= 6) {
        return;
    }
    state.hexText.push_back(static_cast<wchar_t>(::towupper(ch)));

    // ÜÇ VE ALTI HANEDE UYGULANIR: aradaki uzunluklar geçerli bir renk değil ve
    // her tuşta rengi zorlamak, kullanıcı "1e90ff" yazarken paneli çılgınca
    // titretirdi.
    COLORREF parsed = 0;
    if ((state.hexText.size() == 3 || state.hexText.size() == 6) &&
        ParseHexColor(state.hexText.c_str(), parsed)) {
        const std::wstring typed = state.hexText;
        SetColor(state, parsed);
        state.hexText = typed;
    }
    ::InvalidateRect(window, nullptr, FALSE);
}

void OnLeftDown(HWND window, State& state, POINT point) {
    int index = -1;
    const Hit hit = HitTest(state, point, index);
    state.hexFocus = (hit == Hit::Hex);

    switch (hit) {
        case Hit::Square:
            state.pressed = Hit::Square;
            UpdateFromSquare(state, point);
            break;
        case Hit::Hue:
            state.pressed = Hit::Hue;
            UpdateFromHue(state, point);
            break;
        case Hit::Eyedropper:
            RunEyedropper(window, state);
            return;
        case Hit::Preset:
        case Hit::Recent: {
            // HAZIR RENK TEK TIKLA BİTER: paneli açıp bir örneğe basmak zaten
            // kararın tamamıdır; ardından "Tamam"a bastırmak fazladan bir adım.
            const COLORREF chosen =
                hit == Hit::Preset
                    ? kPresets[static_cast<size_t>(index)]
                    : state.recent[static_cast<size_t>(index)];
            SetColor(state, chosen);
            Finish(window, state, true);
            return;
        }
        case Hit::Accept:
            Finish(window, state, true);
            return;
        case Hit::Cancel:
            Finish(window, state, false);
            return;
        default:
            break;
    }
    ::InvalidateRect(window, nullptr, FALSE);
}

LRESULT CALLBACK PickerProc(HWND window, UINT message, WPARAM wParam,
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

        // DIŞARI TIKLAMAK İPTALDİR. Panel bir açılır menü gibi davranır;
        // ekranda unutulup arkadaki pencereyi kilitlemesi en kötü sonuç olurdu.
        case WM_ACTIVATE:
            if (state != nullptr && LOWORD(wParam) == WA_INACTIVE &&
                !state->suspended) {
                Finish(window, *state, false);
            }
            return 0;

        case WM_LBUTTONDOWN: {
            if (state == nullptr) {
                break;
            }
            const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            RECT client{};
            ::GetClientRect(window, &client);
            if (!::PtInRect(&client, point)) {
                // Yakalama bizde olduğu için bu tıklama panelin DIŞINA yapıldı
                // ve arkadaki pencereye HİÇ ULAŞMAZ; yalnızca paneli kapatır.
                Finish(window, *state, false);
                return 0;
            }
            OnLeftDown(window, *state, point);
            return 0;
        }

        case WM_MOUSEMOVE: {
            if (state == nullptr) {
                break;
            }
            const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            if (state->pressed == Hit::Square) {
                UpdateFromSquare(*state, point);
                ::InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (state->pressed == Hit::Hue) {
                UpdateFromHue(*state, point);
                ::InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            int index = -1;
            const Hit hit = HitTest(*state, point, index);
            if (hit != state->hover || index != state->hoverIndex) {
                state->hover = hit;
                state->hoverIndex = index;
                ::InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        }

        // YAKALAMA BIRAKILMAZ: sürükleme bitti ama panel hâlâ açık ve dışarıya
        // yapılacak tıklamayı yakalamaya devam etmesi gerekiyor.
        case WM_LBUTTONUP:
            if (state != nullptr) {
                state->pressed = Hit::None;
            }
            return 0;

        case WM_CHAR:
            if (state != nullptr) {
                OnChar(window, *state, static_cast<wchar_t>(wParam));
            }
            return 0;

        case WM_KEYDOWN:
            if (state != nullptr) {
                OnKeyDown(window, *state, wParam);
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
    wc.lpfnWndProc = PickerProc;
    wc.hInstance = instance;
    wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClass;
    registered = ::RegisterClassExW(&wc) != 0;
    return registered;
}

}  // namespace

}  // namespace picker

bool PickColor(HINSTANCE instance, HWND owner, const RECT& anchor,
               const Settings& settings, COLORREF& color,
               std::vector<COLORREF>& recent) {
    using namespace picker;

    if (!EnsureWindowClass(instance)) {
        return false;
    }

    State state;
    state.instance = instance;
    state.settings = settings;
    state.original = color;
    state.recent = recent;

    // Düğmenin ALTINA, sol kenarına hizalı; ekranın dışına taşarsa içeri
    // çekilir ve aşağıda yer yoksa düğmenin ÜSTÜNE açılır.
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

    SetColor(state, color);
    BuildMetrics(state);

    int x = anchor.left;
    int y = anchor.bottom + Scale(6, state.dpi);
    if (y + state.metrics.height > work.bottom) {
        y = anchor.top - Scale(6, state.dpi) - state.metrics.height;
    }
    x = (std::min)(x, static_cast<int>(work.right) - state.metrics.width);
    x = (std::max)(x, static_cast<int>(work.left));
    y = (std::max)(y, static_cast<int>(work.top));

    const HWND window = ::CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST, kWindowClass, L"", WS_POPUP, x, y,
        state.metrics.width, state.metrics.height, owner, nullptr, instance,
        &state);
    if (window == nullptr) {
        LogV(L"Renk seçici penceresi oluşturulamadı (hata %lu)", ::GetLastError());
        return false;
    }

    theme::ApplyToWindow(window);
    ::ShowWindow(window, SW_SHOW);
    ::SetForegroundWindow(window);
    ::SetFocus(window);
    GrabMouse(window);

    MSG message{};
    while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }

    if (!state.accepted) {
        return false;
    }
    color = state.color;
    Remember(recent, state.color);
    return true;
}

}  // namespace crisp
