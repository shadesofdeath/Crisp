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

    // TextSelect kipi
    OcrLayout layout;
    AlphaLayer layer;
    int selectionAnchor = -1;   // sürüklemenin başladığı kelime
    // Sağ tık menüsü açıkken pencere etkinliğini kaybeder; kaplamanın kendini
    // iptal etmemesi için bu bayrak gerekiyor.
    bool menuOpen = false;

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

// Ekran koordinatını dondurulmuş görüntünün koordinatına çevirir; kelime
// kutuları görüntü koordinatındadır.
[[nodiscard]] POINT ToImage(POINT screenPoint, const RECT& screen) noexcept {
    return POINT{screenPoint.x - screen.left, screenPoint.y - screen.top};
}

// TextSelect: sürükleme boyunca seçili kelime aralığını günceller.
void UpdateTextSelection(OverlayState& state, POINT cursor) {
    if (state.selectionAnchor < 0) {
        return;
    }
    const int current = ocrsel::NearestWord(
        state.layout, ToImage(cursor, state.visual.screen));
    if (current < 0) {
        return;
    }
    ocrsel::NormalizeRange(state.selectionAnchor, current,
                           state.visual.selectionFirst,
                           state.visual.selectionLast);
}

// Seçimi doğrudan bir kelime aralığına kurar (çift/üçlü tıklama).
void SetSelectionRange(OverlayState& state, int first, int last) {
    if (first < 0 || last < 0) {
        return;
    }
    ocrsel::NormalizeRange(first, last, state.visual.selectionFirst,
                           state.visual.selectionLast);
    state.visual.showHint = false;
}

[[nodiscard]] std::wstring CurrentSelectionText(const OverlayState& state) {
    if (state.visual.selectionFirst < 0) {
        return std::wstring();
    }
    return ocrsel::TextForRange(state.layout, state.visual.selectionFirst,
                                state.visual.selectionLast);
}

// TextSelect kipinde sağ tık menüsü. Snipping Tool'daki gibi: seçim varsa
// kopyala, her hâlükârda tümünü kopyala.
void ShowTextMenu(HWND window, OverlayState& state) {
    const HMENU menu = ::CreatePopupMenu();
    if (menu == nullptr) {
        return;
    }

    enum : int { kCopy = 1, kCopyAll, kSelectAll, kCancel };

    const bool hasSelection = state.visual.selectionFirst >= 0;
    ::AppendMenuW(menu, MF_STRING | (hasSelection ? 0u : MF_GRAYED), kCopy,
                  L"Kopyala\tCtrl+C");
    ::AppendMenuW(menu, MF_STRING, kCopyAll, L"Tümünü kopyala");
    ::AppendMenuW(menu, MF_STRING, kSelectAll, L"Tümünü seç\tCtrl+A");
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu, MF_STRING, kCancel, L"İptal\tEsc");

    POINT cursor{};
    ::GetCursorPos(&cursor);

    state.menuOpen = true;
    ::SetForegroundWindow(window);
    const int command = ::TrackPopupMenuEx(
        menu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, cursor.x, cursor.y,
        window, nullptr);
    ::DestroyMenu(menu);
    state.menuOpen = false;

    switch (command) {
        case kCopy:
            state.result.pickedText = CurrentSelectionText(state);
            Finish(window, state, !state.result.pickedText.empty(), RECT{});
            break;
        case kCopyAll:
            state.result.pickedText = ocrsel::AllText(state.layout);
            Finish(window, state, !state.result.pickedText.empty(), RECT{});
            break;
        case kSelectAll:
            if (!state.layout.empty()) {
                SetSelectionRange(state, 0, state.layout.count() - 1);
                ::InvalidateRect(window, nullptr, FALSE);
            }
            break;
        case kCancel:
            Finish(window, state, false, RECT{});
            break;
        default:
            // Menü kapatıldı, seçim korunur.
            ::InvalidateRect(window, nullptr, FALSE);
            break;
    }
}

// Fare bırakıldığında seçim KORUNUR, kaplama kapanmaz.
//
// Bölge yakalamada bırakma "tamam" demektir; metin seçmede değil. Kullanıcı
// seçtikten sonra ne yapacağına karar verir: sağ tık menüsü, Ctrl+C, ya da
// seçimi düzeltmek için yeniden sürüklemek. Bırakır bırakmaz kapatmak, seçimi
// gözden geçirme imkânını elinden alırdı.
void CommitTextSelection(HWND window, OverlayState& state) {
    state.dragging = false;
    state.visual.dragging = false;
    ::ReleaseCapture();
    ::InvalidateRect(window, nullptr, FALSE);
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

    // Metin katmanı, kelimelerin kapsadığı alan kadar. Tüm ekran kadar bir
    // yüzey ayırmak 4K'da 30 MB'ı her karede temizlemek olurdu; metin genelde
    // ekranın küçük bir bölümünde.
    if (state.mode == OverlayMode::TextSelect && !state.layout.empty()) {
        RECT bounds = state.layout.words.front().bounds;
        for (const OcrWord& word : state.layout.words) {
            bounds.left = word.bounds.left < bounds.left ? word.bounds.left : bounds.left;
            bounds.top = word.bounds.top < bounds.top ? word.bounds.top : bounds.top;
            bounds.right =
                word.bounds.right > bounds.right ? word.bounds.right : bounds.right;
            bounds.bottom =
                word.bounds.bottom > bounds.bottom ? word.bounds.bottom : bounds.bottom;
        }
        // Kutu payı ve yuvarlatma taşabilir; cömert bir marj bırakılır.
        ::InflateRect(&bounds, 24, 24);
        bounds = geom::ClampTo(
            bounds, RECT{0, 0, state.frozen.Width(), state.frozen.Height()});

        if (state.layer.Prepare(screenDc.get(), POINT{bounds.left, bounds.top},
                                static_cast<int>(geom::Width(bounds)),
                                static_cast<int>(geom::Height(bounds)))) {
            state.visual.layer = &state.layer;
        }
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
            if (state->mode == OverlayMode::TextSelect) {
                const POINT image = ToImage(cursor, state->visual.screen);
                state->visual.hoverWord = ocrsel::WordAt(state->layout, image);
                state->visual.hoverLine = ocrsel::LineAt(state->layout, image);
                if (state->dragging) {
                    UpdateTextSelection(*state, cursor);
                }
            } else if (state->dragging) {
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
            if (state->mode == OverlayMode::TextSelect) {
                const POINT cursor = CursorInScreen();
                state->selectionAnchor = ocrsel::NearestWord(
                    state->layout, ToImage(cursor, state->visual.screen));
                state->dragging = true;
                state->visual.dragging = true;
                state->visual.showHint = false;
                UpdateTextSelection(*state, cursor);
                ::SetCapture(window);
                ::InvalidateRect(window, nullptr, FALSE);
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
            if (state->mode == OverlayMode::TextSelect) {
                CommitTextSelection(window, *state);
            } else {
                CommitDrag(window, *state);
            }
            return 0;
        }

        // Metin seçmede çift tık kelimeyi, aynı kelimeye ikinci çift tık
        // (yani üçlü tık) satırın tamamını seçer — metin düzenleyicilerin
        // evrensel davranışı.
        case WM_LBUTTONDBLCLK: {
            if (state == nullptr || state->mode != OverlayMode::TextSelect) {
                break;
            }
            const POINT image = ToImage(CursorInScreen(), state->visual.screen);
            const int word = ocrsel::WordAt(state->layout, image);

            const bool wordAlreadySelected =
                word >= 0 && state->visual.selectionFirst == word &&
                state->visual.selectionLast == word;

            if (wordAlreadySelected || word < 0) {
                const int line = ocrsel::LineAt(state->layout, image);
                if (line >= 0) {
                    int first = -1;
                    int last = -1;
                    // Satırın herhangi bir kelimesinden aralığı bul.
                    for (int i = 0; i < state->layout.count(); ++i) {
                        if (state->layout.words[static_cast<size_t>(i)].line == line) {
                            ocrsel::LineRange(state->layout, i, first, last);
                            break;
                        }
                    }
                    SetSelectionRange(*state, first, last);
                }
            } else {
                SetSelectionRange(*state, word, word);
            }
            ::InvalidateRect(window, nullptr, FALSE);
            return 0;
        }

        case WM_RBUTTONDOWN: {
            if (state == nullptr) {
                break;
            }
            if (state->mode == OverlayMode::TextSelect) {
                ShowTextMenu(window, *state);
                return 0;
            }
            Finish(window, *state, false, RECT{});
            return 0;
        }

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
            if (state->mode == OverlayMode::TextSelect) {
                const bool control = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
                if (control && wParam == 'A' && !state->layout.empty()) {
                    state->visual.selectionFirst = 0;
                    state->visual.selectionLast = state->layout.count() - 1;
                    state->visual.showHint = false;
                    ::InvalidateRect(window, nullptr, FALSE);
                    return 0;
                }
                if ((wParam == VK_RETURN || (control && wParam == 'C')) &&
                    state->visual.selectionFirst >= 0) {
                    state->result.pickedText = ocrsel::TextForRange(
                        state->layout, state->visual.selectionFirst,
                        state->visual.selectionLast);
                    Finish(window, *state, !state->result.pickedText.empty(),
                           RECT{});
                    return 0;
                }
                break;
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
            // Metin seçmede I-kirişi: kullanıcıya bunun bir alan değil METİN
            // seçimi olduğunu imleç söyler.
            ::SetCursor(::LoadCursorW(
                nullptr, (state != nullptr && state->mode == OverlayMode::TextSelect)
                             ? IDC_IBEAM
                             : IDC_CROSS));
            return TRUE;

        // Kaplama odağı kaybederse (Alt+Tab, Win tuşu) iptal edilir: görünmez
        // bir tam ekran pencerenin arkada asılı kalması kullanıcıyı kilitler.
        case WM_ACTIVATE: {
            // Sağ tık menüsü açıkken pencere etkinliğini kaybeder; o an iptal
            // etmek menüyü kullanılamaz hâle getirirdi.
            if (state != nullptr && LOWORD(wParam) == WA_INACTIVE &&
                !state->menuOpen) {
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
    // CS_DBLCLKS olmadan WM_LBUTTONDBLCLK hiç gelmez ve kelime/satır seçimi
    // çalışmaz.
    wc.style = CS_DBLCLKS;
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
                                  Image& frozen, const OcrLayout* layout) {
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
    state.visual.textSelect = (mode == OverlayMode::TextSelect);

    if (mode == OverlayMode::TextSelect && layout != nullptr) {
        state.layout = *layout;
        state.visual.layout = &state.layout;
    }

    // Renk seçmenin tek yolu büyüteç: ayar kapalı olsa bile açılır, yoksa
    // kullanıcı hangi pikseli aldığını göremez.
    // Metin seçmede ise büyüteç KAPALIDIR: kelime kutularının üstünü örter ve
    // seçilen şey piksel değil metin olduğu için bir işe yaramaz.
    state.visual.showMagnifier =
        (mode != OverlayMode::TextSelect) &&
        (settings.showMagnifier || mode == OverlayMode::ColorPick);

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
