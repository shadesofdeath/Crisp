// Overlay.h — Tam ekran seçim arayüzü.
//
// EKRAN ÖNCE DONDURULUR. Kaplama canlı masaüstünün üstüne saydam bir pencere
// koymaz; masaüstünü bir kez yakalayıp o görüntüyü çizer. Üç sebeple:
//   1. Büyüteç, altındaki pikselleri okuyabilmelidir — canlı ekranda kaplamanın
//      kendi çizimini okurdu.
//   2. Seçim sırasında arkadaki animasyonlar (video, imleç yanıp sönmesi)
//      görüntüyü değiştirmez; kullanıcı ne gördüyse onu yakalar.
//   3. Yeniden çizim tek bir BitBlt'tir; saydam pencere her karede altındaki
//      her şeyi yeniden çizdirirdi.
#pragma once

#include "Capture.h"
#include "Settings.h"

#include <windows.h>

namespace crisp {

struct OverlayResult {
    bool accepted = false;
    RECT selection{};   // ekran koordinatı
};

// Seçim arayüzünü çalıştırır ve KULLANICI KARAR VERENE KADAR DÖNMEZ; kendi
// mesaj döngüsünü işletir. Dönüşte `frozen`, sanal ekranın dondurulmuş
// görüntüsüdür ve seçim ondan kırpılabilir.
//
// preferWindowPick: pencere yakalama kipinde açılır — kullanıcı sürüklemeden
// tek tıkla pencere seçebilir (aynı davranış bölge kipinde de vardır, bu
// bayrak yalnızca başlangıçta vurgulamayı açar).
[[nodiscard]] OverlayResult RunSelectionOverlay(HINSTANCE instance,
                                                const Settings& settings,
                                                bool preferWindowPick,
                                                Image& frozen);

}  // namespace crisp
