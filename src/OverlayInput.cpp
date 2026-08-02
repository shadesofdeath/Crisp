// OverlayInput.cpp — Kaplamadaki seçim, metin seçme ve renk seçme etkileşimi.
//
// AYRI DOSYA: pencere ve tampon yönetimiyle birlikte Overlay.cpp 663 satırdı,
// ev kuralının sınırının çok üstünde (docs §9). Ayrım işlevsel: burası
// KULLANICININ NE YAPTIĞINI, Overlay.cpp pencerenin nasıl kurulduğunu anlatır.
#include "OverlayInternal.h"

#include "ClipboardImage.h"
#include "Geometry.h"
#include "Localization.h"
#include "Util.h"
#include "WindowPick.h"
#include "resource.h"

#include <cstdio>
#include <string>
#include <vector>

namespace crisp {


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

// Bir monitör tuşu ne yapmalı: yerleşmiş bir seçim varken onu O monitöre
// getirmek, yoksa doğrudan yakalamak.
//
// ESKİDEN HEP YAKALIYORDU ve bu, ayarlanabilir seçimle çelişiyor: kullanıcı
// dikdörtgeni bırakmış, ayarlıyor, sonra "2" ye basıyor — beklediği şey seçimin
// ikinci monitöre geçmesi, yakalamanın bitmesi değil. Enter hâlâ tek onay.
void SetOrFinish(HWND window, OverlayState& state, const RECT& rect) {
    if (state.settled) {
        state.visual.selection = rect;
        ::InvalidateRect(window, nullptr, FALSE);
        return;
    }
    Finish(window, state, true, rect);
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
POINT ToImage(POINT screenPoint, const RECT& screen) noexcept {
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

    const std::wstring copyText = Loc::MenuText(IDS_TEXTMENU_COPY, IDS_ACCEL_COPY);
    const std::wstring copyAllText = Loc::Str(IDS_TEXTMENU_COPY_ALL);
    const std::wstring selectAllText =
        Loc::MenuText(IDS_TEXTMENU_SELECT_ALL, IDS_ACCEL_SELECT_ALL);
    const std::wstring cancelText =
        Loc::MenuText(IDS_TEXTMENU_CANCEL, IDS_ACCEL_ESC);

    const bool hasSelection = state.visual.selectionFirst >= 0;
    ::AppendMenuW(menu, MF_STRING | (hasSelection ? 0u : MF_GRAYED), kCopy,
                  copyText.c_str());
    ::AppendMenuW(menu, MF_STRING, kCopyAll, copyAllText.c_str());
    ::AppendMenuW(menu, MF_STRING, kSelectAll, selectAllText.c_str());
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu, MF_STRING, kCancel, cancelText.c_str());

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

// Bölge kipindeki ek tuşlar. İşlendiyse true döner.
//
// ÜÇÜ DE ZATEN ELDE OLAN VERİYİ KULLANIR: ekran dondurulmuş hâlde duruyor,
// monitör sınırları sistemden geliyor ve seçim dikdörtgeni zaten hesaplanmış.
// Hiçbiri yeni bir yakalama ya da yeni bir pencere gerektirmiyor.
bool HandleOverlayShortcut(HWND window, OverlayState& state, WPARAM key) {
    const bool control = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;

    // Ctrl+C: seçimin konumunu ve ölçüsünü METİN olarak kopyalar. Bir hata
    // raporuna "sağ üstteki düğme" yazmak yerine sayı yazmak gerektiğinde,
    // şimdiye kadar bu sayıları okuyup elle yazmaktan başka yol yoktu.
    if (control && key == 'C') {
        const RECT& selection = state.visual.selection;
        wchar_t text[96];
        if (geom::IsEmpty(selection)) {
            ::swprintf_s(text, L"%ld, %ld", state.visual.cursor.x,
                         state.visual.cursor.y);
        } else {
            ::swprintf_s(text, L"%ld, %ld  %ld × %ld", selection.left,
                         selection.top, geom::Width(selection),
                         geom::Height(selection));
        }
        if (!CopyTextToClipboard(text, window)) {
            LogV(L"Seçim koordinatları panoya kopyalanamadı");
        }
        return true;
    }

    // Ctrl+V: PANODAKİ ÖLÇÜYÜ SEÇİME UYGULAR — Ctrl+C'nin tersi.
    //
    // "Bu görüntü tam 1200×630 olmalı" diyen kullanıcının elinde, fareyi
    // piksel piksel sürükleyip büyüteçten sayıyı okumaktan başka yol yoktu.
    // Oysa sayı çoğu zaman zaten bir yerde yazılı: bir tasarım belgesinde, bir
    // biçim tarifinde, ya da bir önceki seçimin Ctrl+C çıktısında.
    //
    // KONUM VERİLMEDİYSE SEÇİM YERİNDE KALIR. Yalnızca ölçü yapıştıran
    // kullanıcı dikdörtgenin taşınmasını değil, boyutlanmasını bekliyor;
    // seçim henüz yoksa imlecin çevresine ortalanır.
    if (control && key == 'V') {
        std::wstring text;
        if (!ReadTextFromClipboard(text, window)) {
            return true;
        }
        const geom::ParsedSelection parsed = geom::ParseSelectionText(text.c_str());
        if (!parsed.ok) {
            return true;
        }

        POINT origin{parsed.x, parsed.y};
        if (!parsed.hasPosition) {
            if (geom::IsEmpty(state.visual.selection)) {
                origin = POINT{state.visual.cursor.x - parsed.width / 2,
                               state.visual.cursor.y - parsed.height / 2};
            } else {
                origin = POINT{state.visual.selection.left,
                               state.visual.selection.top};
            }
        }

        const RECT wanted{origin.x, origin.y, origin.x + parsed.width,
                          origin.y + parsed.height};
        state.visual.selection = geom::ClampTo(wanted, state.visual.screen);
        state.settled = true;
        state.visual.settled = true;
        state.visual.hover = RECT{};
        state.visual.showHint = true;
        ::InvalidateRect(window, nullptr, FALSE);
        return true;
    }

    // Boşluk: imlecin bulunduğu monitörün tamamını seçer ve bitirir.
    if (key == VK_SPACE) {
        const RECT monitor = MonitorRectAtCursor();
        SetOrFinish(window, state, geom::ClampTo(monitor, state.visual.screen));
        return true;
    }

    // 1..9: numaralı monitörü seçer. Sıra soldan sağa; MonitorRects onu
    // kararlı hâle getiriyor.
    if (key >= '1' && key <= '9') {
        const std::vector<RECT> monitors = MonitorRects();
        const size_t index = static_cast<size_t>(key - '1');
        if (index < monitors.size()) {
            SetOrFinish(window, state, geom::ClampTo(monitors[index], state.visual.screen));
            return true;
        }
        return false;
    }

    // 0: sanal masaüstünün tamamı.
    if (key == '0') {
        SetOrFinish(window, state, state.visual.screen);
        return true;
    }
    return false;
}

// Sürükleme bittiğinde: yeterince büyük bir alan varsa onu, yoksa imlecin
// altındaki pencereyi al. "Tıkla = pencere" davranışı bu daldan gelir.
}  // namespace crisp
