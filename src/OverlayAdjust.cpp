// OverlayAdjust.cpp — Bırakılmış bir seçimin ayarlanması.
//
// AYRI DOSYA: OverlayInput.cpp 255 satırdı ve bu durum makinesi onu ev
// kuralının 400 satır sınırına dayardı (docs §9). Ayrım işlevsel de: burası
// seçim VAR OLDUKTAN SONRA olanları anlatır, OverlayInput.cpp ise onun nasıl
// çizildiğini.
//
// ESKİDEN BÖYLE DEĞİLDİ. Fare bırakıldığı anda `CommitDrag` yakalamayı
// bitiriyordu; seçimin çevresine çizilen sekiz tutamak ise hiçbir şey
// yapmıyordu, çünkü seçimin var olup da hâlâ değiştirilebildiği bir an yoktu.
// Arayüz tutmadığı bir söz veriyordu.
#include "OverlayInternal.h"

#include "OverlayActions.h"

#include "Geometry.h"
#include "Util.h"

#include <cstdlib>

namespace crisp {
namespace {

[[nodiscard]] LONG GrabSize(const OverlayState& state) noexcept {
    return ::MulDiv(kHandleGrabSide, static_cast<int>(state.visual.dpi), 96);
}

[[nodiscard]] HCURSOR CursorFor(geom::Grab grab) noexcept {
    switch (grab) {
        case geom::Grab::NW:
        case geom::Grab::SE:
            return ::LoadCursorW(nullptr, IDC_SIZENWSE);
        case geom::Grab::NE:
        case geom::Grab::SW:
            return ::LoadCursorW(nullptr, IDC_SIZENESW);
        case geom::Grab::N:
        case geom::Grab::S:
            return ::LoadCursorW(nullptr, IDC_SIZENS);
        case geom::Grab::E:
        case geom::Grab::W:
            return ::LoadCursorW(nullptr, IDC_SIZEWE);
        case geom::Grab::Move:
            return ::LoadCursorW(nullptr, IDC_SIZEALL);
        default:
            return nullptr;
    }
}

}  // namespace

void BeginRegionGrab(HWND window, OverlayState& state, POINT cursor) {
    state.anchor = cursor;
    state.grabOrigin = state.visual.selection;
    state.grabArmed = false;

    // SEÇİME BURADA DOKUNULMAZ. Basmak bir niyet kaydıdır; dikdörtgeni
    // değiştiren şey eşiği aşan ilk hareket. Aksi hâlde onay için yapılan çift
    // tıklamanın ilk basışı seçimi bozardı.
    state.grab = geom::Grab::New;
    if (state.settled) {
        const geom::Grab hit =
            geom::HitTestSelection(state.visual.selection, cursor, GrabSize(state));
        if (hit != geom::Grab::None) {
            state.grab = hit;
        }
    }

    state.visual.dragging = true;
    state.visual.showHint = false;

    // İMLEÇ BİR KEZ BURADA KURULUR. Fare yakalanmışken Windows WM_SETCURSOR
    // göndermiyor, dolayısıyla sürükleme boyunca geçerli olacak imleç
    // sürüklemenin başında verilmeli.
    if (const HCURSOR cursorShape = CursorFor(state.grab); cursorShape != nullptr) {
        ::SetCursor(cursorShape);
    }
    ::SetCapture(window);
}

void UpdateRegionGrab(HWND window, OverlayState& state, POINT cursor) {
    if (state.grab == geom::Grab::None) {
        return;
    }

    if (!state.grabArmed) {
        const LONG dx = std::labs(cursor.x - state.anchor.x);
        const LONG dy = std::labs(cursor.y - state.anchor.y);
        if (dx < kGrabThreshold && dy < kGrabThreshold) {
            return;
        }
        state.grabArmed = true;

        // Yeni bir sürükleme eşiği aştığı ANDA eskisi geçersiz olur — daha
        // önce değil, çünkü basıp hiç kıpırdamadan bırakmak seçimi silmemeli.
        if (state.grab == geom::Grab::New) {
            state.settled = false;
            state.visual.settled = false;
            state.visual.selection = RECT{};
        }
    }

    switch (state.grab) {
        case geom::Grab::New:
            UpdateSelection(state, cursor);
            break;
        case geom::Grab::Move:
            state.visual.selection =
                geom::OffsetClamped(state.grabOrigin, cursor.x - state.anchor.x,
                                    cursor.y - state.anchor.y, state.visual.screen);
            break;
        default:
            state.visual.selection =
                geom::ResizeByGrab(state.grabOrigin, state.grab, cursor,
                                   kMinimumSelectionSide, state.visual.screen);
            break;
    }
    ::InvalidateRect(window, nullptr, FALSE);
}

void EndRegionGrab(HWND window, OverlayState& state) {
    const geom::Grab grab = state.grab;
    const bool armed = state.grabArmed;

    state.grab = geom::Grab::None;
    state.grabArmed = false;
    state.visual.dragging = false;

    // YAKALAMA HER YOLDA BIRAKILIR. Eskiden bunu tek çıkışı olan `CommitDrag`
    // yapıyordu; şimdi dört ayrı sonuç var ve birini unutmak, düğme basılı
    // değilken fareyi tutmaya devam eden bir pencere demek.
    ::ReleaseCapture();

    // Çift tıklamanın ikinci basışı WM_LBUTTONDBLCLK olarak geliyor (sınıf
    // CS_DBLCLKS taşıyor), yani onun WM_LBUTTONUP'ı basış görmeden buraya
    // düşer. Yapılacak bir şey yok.
    if (grab == geom::Grab::None) {
        ::InvalidateRect(window, nullptr, FALSE);
        return;
    }

    if (grab != geom::Grab::New) {
        return;   // taşıma ya da boyutlandırma bitti; seçim yerinde kalır
    }

    if (armed && geom::IsUsableSelection(state.visual.selection, kMinimumSelectionSide)) {
        state.settled = true;
        state.visual.settled = true;

        // VURGULANAN PENCERE TEMİZLENİR. `visual.hover` yalnızca `UpdateHover`
        // tarafından yazılıyor ve yerleşmişken onu çağırmıyoruz; temizlenmezse
        // son bilinen pencere hem ekranda vurgulu kalır hem de aşağıdaki
        // "tıkla = pencereyi yakala" dalına yem olurdu.
        state.visual.hover = RECT{};
        state.visual.showHint = true;
        ::InvalidateRect(window, nullptr, FALSE);
        return;
    }

    // TIKLA = PENCEREYİ YAKALA, YALNIZCA YERLEŞMEMİŞKEN. Yerleşmiş bir seçim
    // varken dışarı yapılan tık, kullanıcının baştan başlamak istemesidir;
    // eskisinin yanındaki bir pencereyi yakalamak istemesi değil.
    if (!state.settled && !armed && !geom::IsEmpty(state.visual.hover)) {
        Finish(window, state, true, state.visual.hover);
        return;
    }

    ClearRegionSelection(window, state);
}

void ClearRegionSelection(HWND window, OverlayState& state) {
    state.settled = false;
    state.visual.settled = false;
    state.visual.selection = RECT{};
    state.visual.showHint = true;

    // Vurgu HEMEN geri gelsin: kullanıcı seçimi sildikten sonra fareyi
    // kıpırdatmadan bir pencereye tıklayabilmeli.
    UpdateHover(state, window, state.visual.cursor);
    ::InvalidateRect(window, nullptr, FALSE);
}

int ActionAtCursor(const OverlayState& state, POINT cursor) {
    if (!state.settled || !state.visual.showActionBar ||
        state.grab != geom::Grab::None) {
        return -1;
    }
    ActionButton buttons[static_cast<size_t>(OverlayAction::Count)]{};
    RECT bar{};
    const int count = ActionButtons(state.visual.selection,
                                    MonitorRectAtPoint(cursor), state.visual.dpi,
                                    state.visual.uploadEnabled, buttons, bar);
    return ActionButtonAt(buttons, count, cursor);
}

HCURSOR RegionCursor(const OverlayState& state) {
    if (state.mode == OverlayMode::TextSelect) {
        return ::LoadCursorW(nullptr, IDC_IBEAM);
    }

    // ÇUBUĞUN ÜSTÜNDE OK. Artı imleç "buraya nişan al" der; düğmenin üstünde
    // nişan alınacak bir şey yok, tıklanacak bir şey var. Tutamak isabet
    // testinden ÖNCE bakılıyor çünkü çubuk seçimin alt kenarına değebiliyor.
    if (ActionAtCursor(state, CursorInScreen()) >= 0) {
        return ::LoadCursorW(nullptr, IDC_ARROW);
    }

    if (state.grab != geom::Grab::None) {
        if (const HCURSOR shape = CursorFor(state.grab); shape != nullptr) {
            return shape;
        }
    } else if (state.settled) {
        // İMLEÇ KONUMU BURADA SORULUR, `visual.cursor` OKUNMAZ: WM_SETCURSOR
        // WM_MOUSEMOVE'dan ÖNCE geliyor, dolayısıyla saklanan konum bir hareket
        // eskidir ve imleç bir kare geriden gelirdi.
        const geom::Grab hit = geom::HitTestSelection(state.visual.selection,
                                                      CursorInScreen(), GrabSize(state));
        if (const HCURSOR shape = CursorFor(hit); shape != nullptr) {
            return shape;
        }
    }
    return ::LoadCursorW(nullptr, IDC_CROSS);
}

}  // namespace crisp
