// OverlayActions.h — Eylem çubuğunun ÇİZİMİ.
//
// Düzeni ActionBar.h'de ve o çekirdekte: kaç düğme olduğu, nerede durdukları
// ve imlecin altındakinin hangisi olduğu pencere açmadan hesaplanıyor.
// Burada kalan tek şey onların nasıl göründüğü.
#pragma once

#include "ActionBar.h"

#include <windows.h>

namespace crisp {

// Çubuğu çizer. `hovered` imlecin altındaki düğmenin dizini ya da -1.
void DrawActionBar(HDC dc, const RECT& screen, unsigned dpi,
                   const ActionButton* buttons, int count, int hovered,
                   HFONT font);

}  // namespace crisp
