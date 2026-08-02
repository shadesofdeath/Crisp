// Overlay.cpp — bkz. Overlay.h.
#include "Overlay.h"

#include "OverlayInternal.h"

#include "Geometry.h"
#include "Localization.h"
#include "OverlayActions.h"
#include "OverlayPaint.h"
#include "Util.h"
#include "WindowPick.h"
#include "resource.h"

namespace crisp {

POINT CursorInScreen() noexcept {
    POINT cursor{};
    if (!::GetCursorPos(&cursor)) {
        cursor = POINT{0, 0};
    }
    return cursor;
}

bool PrepareBuffers(HWND window, OverlayState& state) {
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
            } else if (state->grab != geom::Grab::None) {
                UpdateRegionGrab(window, *state, cursor);
            } else if (!state->settled) {
                UpdateHover(*state, window, cursor);
            }
            state->visual.hoverAction = ActionAtCursor(*state, cursor);
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
            const POINT cursor = CursorInScreen();

            // EYLEM ÇUBUĞU SÜRÜKLEMEDEN ÖNCE BAKILIR. Çubuk seçimin dışında
            // duruyor; bu sınama olmasaydı bir düğmeye basmak, seçimi silip
            // yeni bir sürükleme başlatan "dışarı tık" dalına düşerdi.
            if (const int chosen = ActionAtCursor(*state, cursor); chosen >= 0) {
                ActionButton buttons[static_cast<size_t>(OverlayAction::Count)]{};
                RECT bar{};
                const int count = ActionButtons(
                    state->visual.selection, MonitorRectAtPoint(cursor),
                    state->visual.dpi, state->visual.uploadEnabled, buttons, bar);
                if (chosen < count) {
                    state->result.action = buttons[chosen].action;
                    Finish(window, *state, true, state->visual.selection);
                    return 0;
                }
            }

            // Fare kaplamanın dışına çıksa da hareketleri almaya devam et;
            // yakalamayı BeginRegionGrab kuruyor.
            BeginRegionGrab(window, *state, cursor);
            ::InvalidateRect(window, nullptr, FALSE);
            return 0;
        }

        case WM_LBUTTONUP: {
            if (state == nullptr) {
                break;
            }
            if (state->mode == OverlayMode::TextSelect) {
                if (!state->dragging) {
                    break;
                }
                CommitTextSelection(window, *state);
            } else {
                EndRegionGrab(window, *state);
            }
            return 0;
        }

        // Metin seçmede çift tık kelimeyi, aynı kelimeye ikinci çift tık
        // (yani üçlü tık) satırın tamamını seçer — metin düzenleyicilerin
        // evrensel davranışı.
        case WM_LBUTTONDBLCLK: {
            if (state == nullptr) {
                break;
            }
            if (state->mode != OverlayMode::TextSelect) {
                // Yerleşmiş seçimin İÇİNE çift tık: onay. Dışına çift tık ise
                // "baştan başlıyorum" demektir ve ikinci basış yeni bir
                // sürükleme başlatmalı, yoksa hızlı davranan kullanıcı ilk
                // hareketini kaybederdi.
                const POINT cursor = CursorInScreen();
                // Çubuğun üstündeki ikinci basış: ilk basış zaten kaplamayı
                // bitirmiş olmalı, ama bitirmediyse yeni bir sürükleme
                // başlatmamalı.
                if (ActionAtCursor(*state, cursor) >= 0) {
                    return 0;
                }
                if (state->settled &&
                    geom::IsUsableSelection(state->visual.selection,
                                            kMinimumSelectionSide) &&
                    ::PtInRect(&state->visual.selection, cursor) != FALSE) {
                    Finish(window, *state, true, state->visual.selection);
                } else {
                    BeginRegionGrab(window, *state, cursor);
                    ::InvalidateRect(window, nullptr, FALSE);
                }
                return 0;
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
                // YALNIZ YENİ SÜRÜKLEMEDE. UpdateSelection dikdörtgeni
                // `anchor`dan yeniden kuruyor ve taşıma ya da boyutlandırma
                // sırasında `anchor` o sürüklemenin başlangıcı; Shift'e basmak
                // seçimi eski çıpaya geri fırlatırdı.
                if (state->grab == geom::Grab::New) {
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
            if (HandleOverlayShortcut(window, *state, wParam)) {
                return 0;
            }
            break;
        }

        case WM_KEYUP: {
            if (state != nullptr && wParam == VK_SHIFT) {
                state->shiftHeld = false;
                if (state->grab == geom::Grab::New) {
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

        // İMLEÇ BURADA SEÇİLİR, WM_MOUSEMOVE'DA DEĞİL. Fare yakalanmamışken
        // Windows her hareketten ÖNCE WM_SETCURSOR gönderiyor; hareket
        // işleyicisinden verilen bir imleç hemen ardından geri alınırdı.
        // Metin seçmede I-kirişi, yerleşmiş seçimde tutamağa göre boyutlandırma
        // imleci, kalan her yerde artı.
        case WM_SETCURSOR:
            if (state == nullptr || LOWORD(lParam) != HTCLIENT) {
                break;
            }
            ::SetCursor(RegionCursor(*state));
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

}  // namespace crisp
