// Overlay.cpp — bkz. Overlay.h.
#include "Overlay.h"

#include "Geometry.h"
#include "OverlayPaint.h"
#include "Util.h"
#include "WindowPick.h"

namespace crisp {
namespace {

constexpr const wchar_t* kWindowClass = L"CrispSelectionOverlay";

// Bu boyutun altındaki sürüklemeler seçim değil, kazara fare kaymasıdır ve
// pencere yakalama tıklaması olarak yorumlanır.
constexpr LONG kMinimumSelectionSide = 6;

// Kaplamanın tüm durumu. Pencere verisi olarak saklanır; global değişken yok.
struct OverlayState {
    OverlayVisual visual{};
    Image frozen;
    Image dimmed;

    unique_hdc frozenDc;
    unique_hdc dimmedDc;
    unique_hdc backBufferDc;
    unique_hbitmap backBuffer;

    POINT anchor{};
    bool dragging = false;
    bool shiftHeld = false;
    bool decided = false;
    bool allowHover = true;
    OverlayMode mode = OverlayMode::Region;
    OverlayResult result{};
};

[[nodiscard]] POINT CursorInScreen() noexcept {
    POINT cursor{};
    if (!::GetCursorPos(&cursor)) {
        cursor = POINT{0, 0};
    }
    return cursor;
}

// Sürükleme sırasında seçimi günceller. Shift basılıysa kareye kilitlenir.
void UpdateSelection(OverlayState& state, POINT cursor) {
    POINT other = cursor;
    if (state.shiftHeld) {
        other = geom::SnapToSquare(state.anchor, cursor);
    }
    state.visual.selection =
        geom::ClampTo(geom::FromCorners(state.anchor, other), state.visual.screen);
}

// Sürükleme yokken imlecin altındaki pencereyi bulur.
void UpdateHover(OverlayState& state, HWND overlay, POINT cursor) {
    if (!state.allowHover) {
        state.visual.hover = RECT{};
        return;
    }
    const HWND window = WindowUnderPoint(cursor, overlay);
    RECT bounds{};
    if (window != nullptr && WindowFrameBounds(window, bounds)) {
        state.visual.hover = geom::ClampTo(bounds, state.visual.screen);
    } else {
        state.visual.hover = RECT{};
    }
}

void Finish(HWND window, OverlayState& state, bool accepted, const RECT& selection) {
    if (state.decided) {
        return;
    }
    state.decided = true;
    state.result.accepted = accepted;
    state.result.selection = selection;
    ::DestroyWindow(window);
}

// ColorPick kipinde tıklama: imlecin altındaki pikselin rengini alıp biter.
void PickColor(HWND window, OverlayState& state) {
    const int x = static_cast<int>(state.visual.cursor.x - state.visual.screen.left);
    const int y = static_cast<int>(state.visual.cursor.y - state.visual.screen.top);

    state.result.pickedColor = state.frozen.Pixel(x, y);
    Finish(window, state, true, RECT{});
}

// Sürükleme bittiğinde: yeterince büyük bir alan varsa onu, yoksa imlecin
// altındaki pencereyi al. "Tıkla = pencere" davranışı bu daldan gelir.
void CommitDrag(HWND window, OverlayState& state) {
    state.dragging = false;
    state.visual.dragging = false;
    ::ReleaseCapture();

    if (geom::IsUsableSelection(state.visual.selection, kMinimumSelectionSide)) {
        Finish(window, state, true, state.visual.selection);
        return;
    }

    if (!geom::IsEmpty(state.visual.hover)) {
        Finish(window, state, true, state.visual.hover);
        return;
    }

    // Ne alan ne pencere: seçimi temizle ve kullanıcıyı kaplamada bırak.
    state.visual.selection = RECT{};
    ::InvalidateRect(window, nullptr, FALSE);
}

[[nodiscard]] bool PrepareBuffers(HWND window, OverlayState& state) {
    const window_dc screenDc{nullptr};
    if (!screenDc.valid()) {
        return false;
    }

    state.frozenDc.reset(::CreateCompatibleDC(screenDc.get()));
    state.dimmedDc.reset(::CreateCompatibleDC(screenDc.get()));
    state.backBufferDc.reset(::CreateCompatibleDC(screenDc.get()));
    if (!state.frozenDc || !state.dimmedDc || !state.backBufferDc) {
        return false;
    }

    ::SelectObject(state.frozenDc.get(), state.frozen.Handle());
    ::SelectObject(state.dimmedDc.get(), state.dimmed.Handle());

    // Arka tampon: titremesiz çizim için. Doğrudan ekrana çizmek, büyüteç her
    // fare hareketinde yeniden çizildiğinden gözle görülür titreme yaratırdı.
    state.backBuffer.reset(::CreateCompatibleBitmap(
        screenDc.get(), state.frozen.Width(), state.frozen.Height()));
    if (!state.backBuffer) {
        return false;
    }
    ::SelectObject(state.backBufferDc.get(), state.backBuffer.get());

    state.visual.dpi = ::GetDpiForWindow(window);
    if (state.visual.dpi == 0) {
        state.visual.dpi = 96;
    }
    return true;
}

LRESULT CALLBACK OverlayProc(HWND window, UINT message, WPARAM wParam,
                             LPARAM lParam) {
    auto* state = reinterpret_cast<OverlayState*>(
        ::GetWindowLongPtrW(window, GWLP_USERDATA));

    switch (message) {
        case WM_NCCREATE: {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            ::SetWindowLongPtrW(window, GWLP_USERDATA,
                                reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return TRUE;
        }

        case WM_CREATE: {
            if (state == nullptr || !PrepareBuffers(window, *state)) {
                return -1;
            }
            const POINT cursor = CursorInScreen();
            state->visual.cursor = cursor;
            UpdateHover(*state, window, cursor);
            return 0;
        }

        case WM_MOUSEMOVE: {
            if (state == nullptr) {
                break;
            }
            const POINT cursor = CursorInScreen();
            state->visual.cursor = cursor;
            if (state->dragging) {
                UpdateSelection(*state, cursor);
            } else {
                UpdateHover(*state, window, cursor);
            }
            ::InvalidateRect(window, nullptr, FALSE);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            if (state == nullptr) {
                break;
            }
            if (state->mode == OverlayMode::ColorPick) {
                PickColor(window, *state);
                return 0;
            }
            state->anchor = CursorInScreen();
            state->dragging = true;
            state->visual.dragging = true;
            state->visual.showHint = false;
            state->visual.selection = RECT{};
            // Fare kaplamanın dışına çıksa da hareketleri almaya devam et.
            ::SetCapture(window);
            ::InvalidateRect(window, nullptr, FALSE);
            return 0;
        }

        case WM_LBUTTONUP: {
            if (state == nullptr || !state->dragging) {
                break;
            }
            CommitDrag(window, *state);
            return 0;
        }

        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN: {
            if (state != nullptr) {
                Finish(window, *state, false, RECT{});
            }
            return 0;
        }

        case WM_KEYDOWN: {
            if (state == nullptr) {
                break;
            }
            if (wParam == VK_ESCAPE) {
                Finish(window, *state, false, RECT{});
                return 0;
            }
            if (wParam == VK_SHIFT && !state->shiftHeld) {
                state->shiftHeld = true;
                if (state->dragging) {
                    UpdateSelection(*state, state->visual.cursor);
                    ::InvalidateRect(window, nullptr, FALSE);
                }
                return 0;
            }
            if (wParam == VK_RETURN &&
                geom::IsUsableSelection(state->visual.selection,
                                        kMinimumSelectionSide)) {
                Finish(window, *state, true, state->visual.selection);
                return 0;
            }
            break;
        }

        case WM_KEYUP: {
            if (state != nullptr && wParam == VK_SHIFT) {
                state->shiftHeld = false;
                if (state->dragging) {
                    UpdateSelection(*state, state->visual.cursor);
                    ::InvalidateRect(window, nullptr, FALSE);
                }
                return 0;
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT paint{};
            const HDC dc = ::BeginPaint(window, &paint);
            if (dc != nullptr && state != nullptr) {
                PaintOverlay(state->backBufferDc.get(), state->visual,
                             state->frozenDc.get(), state->dimmedDc.get(),
                             state->frozen);
                ::BitBlt(dc, 0, 0, state->frozen.Width(), state->frozen.Height(),
                         state->backBufferDc.get(), 0, 0, SRCCOPY);
            }
            ::EndPaint(window, &paint);
            return 0;
        }

        // Arka planı silme: her şey WM_PAINT'te tampondan geliyor, silme
        // yalnızca bir kare boyunca beyaz parlama üretirdi.
        case WM_ERASEBKGND:
            return 1;

        case WM_SETCURSOR:
            ::SetCursor(::LoadCursorW(nullptr, IDC_CROSS));
            return TRUE;

        // Kaplama odağı kaybederse (Alt+Tab, Win tuşu) iptal edilir: görünmez
        // bir tam ekran pencerenin arkada asılı kalması kullanıcıyı kilitler.
        case WM_ACTIVATE: {
            if (state != nullptr && LOWORD(wParam) == WA_INACTIVE) {
                Finish(window, *state, false, RECT{});
            }
            return 0;
        }

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
    wc.lpfnWndProc = OverlayProc;
    wc.hInstance = instance;
    wc.hCursor = ::LoadCursorW(nullptr, IDC_CROSS);
    wc.lpszClassName = kWindowClass;
    // Arka plan fırçası YOK: WM_ERASEBKGND'i zaten yutuyoruz.
    wc.hbrBackground = nullptr;

    registered = ::RegisterClassExW(&wc) != 0;
    return registered;
}

}  // namespace

OverlayResult RunSelectionOverlay(HINSTANCE instance, const Settings& settings,
                                  OverlayMode mode, bool preferWindowPick,
                                  Image& frozen) {
    OverlayResult result{};

    if (!EnsureWindowClass(instance)) {
        LogV(L"Kaplama pencere sınıfı kaydedilemedi");
        return result;
    }

    const RECT screen = VirtualScreenRect();
    if (!CaptureRect(screen, frozen)) {
        LogV(L"Ekran dondurulamadı");
        return result;
    }

    OverlayState state;
    state.mode = mode;
    state.visual.screen = screen;
    state.visual.showHint = true;
    state.visual.colorPick = (mode == OverlayMode::ColorPick);

    // Renk seçmenin tek yolu büyüteç: ayar kapalı olsa bile açılır, yoksa
    // kullanıcı hangi pikseli aldığını göremez.
    state.visual.showMagnifier =
        settings.showMagnifier || mode == OverlayMode::ColorPick;

    // Pencere vurgulaması yalnızca bölge kipinde anlamlı.
    state.allowHover = (mode == OverlayMode::Region) &&
                       (preferWindowPick || settings.showWindowHighlight);

    if (!BuildDimmedCopy(frozen, state.dimmed)) {
        return result;
    }
    // Dondurulmuş görüntünün sahipliği kaplamaya taşınır ve dönüşte geri
    // verilir. Kopyalamak 4K çok monitörlü bir masaüstünde onlarca megabayt
    // gereksiz tahsis olurdu.
    state.frozen = std::move(frozen);

    // WS_EX_TOOLWINDOW: Alt+Tab listesinde görünmesin.
    // WS_EX_TOPMOST: her şeyin üstünde; yakaladığı masaüstünün üstünde durmalı.
    const DWORD exStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW;

    const HWND window = ::CreateWindowExW(
        exStyle, kWindowClass, L"Crisp", WS_POPUP, screen.left, screen.top,
        static_cast<int>(geom::Width(screen)),
        static_cast<int>(geom::Height(screen)), nullptr, nullptr, instance,
        &state);

    if (window == nullptr) {
        LogV(L"Kaplama penceresi oluşturulamadı (hata %lu)", ::GetLastError());
        frozen = std::move(state.frozen);
        return result;
    }

    ::ShowWindow(window, SW_SHOW);

    // Gösterimden SONRA en üste yeniden yerleştir. WS_EX_TOPMOST oluşturma
    // anında verilir ama görev çubuğu da en-üst bir penceredir ve iki en-üst
    // pencere arasında sıra en son yerleştirilene göre belirlenir. Bu çağrı
    // olmadan kaplama görev çubuğunun ALTINDA kalabilir; o hâlde kullanıcı
    // ekranın alt şeridinde karartılmamış, canlı bir görev çubuğu görür ve
    // oradan seçim yaptığında gördüğüyle yakaladığı uyuşmaz.
    ::SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);

    // SetForegroundWindow, süreç ön plan hakkına sahip değilse SESSİZCE
    // başarısız olur. Gerçek kullanımda hakkı global kısayol verir; başka bir
    // süreç tetiklerse (betik, otomasyon) pencere görünür ama klavye girdisi
    // almayabilir, bu yüzden odak ayrıca zorlanır.
    ::SetForegroundWindow(window);
    ::SetFocus(window);
    ::SetActiveWindow(window);

    MSG message{};
    while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }

    // Dondurulmuş görüntü çağırana geri verilir: seçim ondan kırpılacak.
    frozen = std::move(state.frozen);
    return state.result;
}

}  // namespace crisp
