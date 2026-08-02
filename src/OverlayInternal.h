// OverlayInternal.h — Seçim kaplamasının İÇ paylaşımı.
//
// AYRI BAŞLIK: kaplamanın durumu hem etkileşim (OverlayInput.cpp) hem pencere
// ve tampon yönetimi (Overlay.cpp) tarafından kullanılıyor. İkisi tek dosyada
// ev kuralının 400 satır sınırını fazlasıyla aşıyordu (docs §9).
#pragma once

#include "AlphaLayer.h"
#include "Capture.h"
#include "Geometry.h"
#include "OcrLayout.h"
#include "Overlay.h"
#include "OverlayPaint.h"
#include "Util.h"

#include <windows.h>

namespace crisp {

// Kazara tıklamayı seçimden ayıran en küçük kenar.
inline constexpr LONG kMinimumSelectionSide = 6;

// Basmayı sürüklemeden ayıran eşik.
//
// BASMAK SEÇİMİ DEĞİŞTİRMEZ, SÜRÜKLEMEK DEĞİŞTİRİR. Eşik olmadan, yerleşmiş bir
// seçimi onaylamak için yapılan çift tıklamanın ilk basışı dikdörtgeni bir
// piksel oynatırdı; ya da dışarı yapılan bir tık, kullanıcı daha ne yapacağına
// karar vermeden seçimi siler.
inline constexpr LONG kGrabThreshold = 4;


struct OverlayState {
    OverlayVisual visual{};
    Image frozen;
    Image dimmed;

    unique_hdc frozenDc;
    unique_hdc dimmedDc;
    unique_hdc backBufferDc;
    unique_hbitmap backBuffer;

    POINT anchor{};
    // TEXTSELECT KİPİNE AİT. Bölge kipi artık `grab`i kullanıyor; iki bayrağı
    // birden tutmak, Shift'in yanlış dalı tetiklemesi gibi hatalara yol
    // açıyordu.
    bool dragging = false;

    // --- Bölge seçimi: yerleşmiş ve ayarlanabilir ---------------------------
    //
    // Fare bırakıldığında yakalama BİTMİYOR: seçim ekranda kalıyor,
    // tutamaklarından boyutlandırılabiliyor ve içinden taşınabiliyor. Onay
    // Enter ya da içine çift tık.
    bool settled = false;
    geom::Grab grab = geom::Grab::None;
    bool grabArmed = false;    // kGrabThreshold aşıldı mı
    RECT grabOrigin{};         // sürükleme başlarkenki seçim
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

[[nodiscard]] POINT CursorInScreen() noexcept;

// Ekran noktasını dondurulmuş görüntünün koordinatına çevirir.
[[nodiscard]] POINT ToImage(POINT screenPoint, const RECT& screen) noexcept;

// Pencere yordamı ve tampon hazırlığı (Overlay.cpp); sınıfı kaydeden
// OverlaySession.cpp'den görünmeleri gerekir.
LRESULT CALLBACK OverlayProc(HWND window, UINT message, WPARAM wParam,
                             LPARAM lParam);
[[nodiscard]] bool PrepareBuffers(HWND window, OverlayState& state);

// --- Etkileşim (OverlayInput.cpp) -------------------------------------------
void UpdateSelection(OverlayState& state, POINT cursor);
void UpdateHover(OverlayState& state, HWND overlay, POINT cursor);
void Finish(HWND window, OverlayState& state, bool accepted, const RECT& selection);
void UpdateTextSelection(OverlayState& state, POINT cursor);
void SetSelectionRange(OverlayState& state, int first, int last);
void ShowTextMenu(HWND window, OverlayState& state);
void CommitTextSelection(HWND window, OverlayState& state);
void PickColor(HWND window, OverlayState& state);
// --- Bölge seçimini ayarlama (OverlayAdjust.cpp) ----------------------------
void BeginRegionGrab(HWND window, OverlayState& state, POINT cursor);
void UpdateRegionGrab(HWND window, OverlayState& state, POINT cursor);
void EndRegionGrab(HWND window, OverlayState& state);
void ClearRegionSelection(HWND window, OverlayState& state);
// İmlecin altındaki eylem düğmesinin dizini; çubuk görünmüyorsa ya da imleç
// düğmelerin dışındaysa -1.
[[nodiscard]] int ActionAtCursor(const OverlayState& state, POINT cursor);
[[nodiscard]] HCURSOR RegionCursor(const OverlayState& state);
[[nodiscard]] bool HandleOverlayShortcut(HWND window, OverlayState& state,
                                         WPARAM key);

}  // namespace crisp
