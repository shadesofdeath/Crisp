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

#include <string>

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
}  // namespace crisp
