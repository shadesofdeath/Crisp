// OverlayPaint.h — Seçim kaplamasının çizimi.
//
// Çizim, pencere ve girdi mantığından AYRI tutulur: kaplamanın davranışı
// (sürükleme, tuşlar, iptal) ile görünüşü birbirinden bağımsız değişir ve
// ikisi tek dosyada olsaydı 400 satırı çoktan aşardı.
//
// TÜM DİKDÖRTGENLER EKRAN KOORDİNATINDADIR. Dönüşümü çizim yapan taraf yapar;
// çağıranın istemci koordinatına çevirmesi gerekmez. Sanal ekranın sol-üst
// köşesi negatif olabildiği için bu dönüşümü tek bir yerde tutmak, her
// kullanım yerinde bir işaret hatası riskinden iyidir.
#pragma once

#include "Capture.h"

#include <windows.h>

namespace crisp {

// Kaplamanın o anki görsel durumu.
struct OverlayVisual {
    RECT screen{};        // sanal ekran (ekran koordinatı) — istemcinin kökeni
    RECT selection{};     // seçili alan; boşsa seçim yok
    RECT hover{};         // imlecin altındaki pencere; boşsa vurgulama yok
    POINT cursor{};
    bool dragging = false;
    bool showMagnifier = true;
    bool showHint = true;
    bool colorPick = false;   // ipucu metnini ve seçim çizimini değiştirir
    unsigned dpi = 96;
};

// Kaplamayı hedef DC'ye çizer.
//   frozenDc  — dondurulmuş ekran görüntüsü seçili bellek DC'si (parlak)
//   dimmedDc  — aynı görüntünün karartılmış kopyası
// İkisi de çağıran tarafından hazırlanır ve kaplama ömrü boyunca yaşar;
// her boyamada yeniden üretmek 4K çok monitörlü bir masaüstünde onlarca
// milisaniye sürerdi.
void PaintOverlay(HDC target, const OverlayVisual& visual, HDC frozenDc,
                  HDC dimmedDc, const Image& frozen);

// Karartılmış kopyayı üretir. Bir kez çağrılır.
[[nodiscard]] bool BuildDimmedCopy(const Image& source, Image& out);

}  // namespace crisp
