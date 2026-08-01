// OverlayInternal.h — Seçim kaplamasının İÇ paylaşımı.
//
// AYRI BAŞLIK: kaplamanın durumu hem etkileşim (OverlayInput.cpp) hem pencere
// ve tampon yönetimi (Overlay.cpp) tarafından kullanılıyor. İkisi tek dosyada
// ev kuralının 400 satır sınırını fazlasıyla aşıyordu (docs §9).
#pragma once

#include "AlphaLayer.h"
#include "Capture.h"
#include "OcrLayout.h"
#include "Overlay.h"
#include "OverlayPaint.h"
#include "Util.h"

#include <windows.h>

namespace crisp {

// Kazara tıklamayı seçimden ayıran en küçük kenar.
inline constexpr LONG kMinimumSelectionSide = 6;

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
void CommitDrag(HWND window, OverlayState& state);

}  // namespace crisp
