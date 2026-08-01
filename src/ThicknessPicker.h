// ThicknessPicker.h — Çizgi kalınlığı açılır listesi.
//
// NEDEN AÇILIR LİSTE: üç ayrı düğme araç çubuğunda üç yer kaplıyor, yalnızca
// üç seçenek sunuyor ve her birini birbirinden ayıran şey içindeki noktanın
// çapıydı — 2 ile 4 piksel arasındaki fark 38 piksellik bir düğmede gözle
// ayırt edilemiyordu. Liste, her seçeneği O KALINLIKTA ÇİZİLMİŞ bir çizgiyle
// gösterir: kullanıcı seçmeden önce sonucu görür.
#pragma once

#include <windows.h>

namespace crisp {

// `anchor` EKRAN koordinatında, listenin altına açılacağı düğmedir.
// `ink` önizleme çizgilerinin rengi — seçili çizim rengiyle aynı olmalı ki
// önizleme gerçekten önizleme olsun.
// Seçim yapılırsa true döner ve `thickness` güncellenir.
[[nodiscard]] bool PickThickness(HINSTANCE instance, HWND owner,
                                 const RECT& anchor, COLORREF ink,
                                 int& thickness);

// Listede sunulan kalınlıklar; araç çubuğu düğmesi de geçerli değeri buradan
// doğrular.
inline constexpr int kThicknessChoices[] = {1, 2, 3, 5, 8, 12, 18};

}  // namespace crisp
