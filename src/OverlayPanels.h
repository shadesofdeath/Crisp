// OverlayPanels.h — Kaplamanın üstünde YÜZEN iki kutu: büyüteç ve ipucu.
//
// Kaplamanın geri kalanından ayrılar: ikisi de seçimin geometrisiyle
// ilgilenmez, ikisi de imlecin bulunduğu MONİTÖRE göre yerleşir (sanal ekrana
// değil — iki monitörlü bir masaüstünde sanal ekranın ortası tam olarak iki
// ekranın birleşim yeridir) ve ikisi de bir kenara sığmadığında yer değiştirir.
// Seçim çizimiyle paylaştıkları tek şey OverlayDraw.h'deki ilkeller.
#pragma once

#include "OverlayPaint.h"

#include <windows.h>

namespace crisp {

// 21×21 pikseli 7× büyütür, altına koordinat, onaltılık renk ve bir renk
// kutusu koyar. `frozenDc` dondurulmuş masaüstü, `frozen` aynı görüntünün
// piksel okunabilir hâli.
void DrawMagnifier(HDC dc, const OverlayVisual& visual, HDC frozenDc,
                   const Image& frozen, HFONT font, HFONT fontBold);

// Ekranın üst ortasındaki ipucu kutusu. Metni kipe göre seçer: metin seçme,
// renk alma, bölge, ve yerleşmiş bölge.
void DrawHint(HDC dc, const OverlayVisual& visual, HFONT font);

}  // namespace crisp
