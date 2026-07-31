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
#include "OcrLayout.h"
#include "Settings.h"

#include <string>

#include <windows.h>

namespace crisp {

enum class OverlayMode {
    // Sürükleyerek alan, tek tıkla pencere seçilir.
    Region,
    // Sürükleme yok; tek tık imlecin altındaki pikselin rengini alır.
    // Aynı dondurulmuş görüntü ve aynı büyüteç kullanılır — renk seçici için
    // ayrı bir kaplama yazmak, büyüteci ve piksel okumayı ikinci kez
    // uygulamak olurdu.
    ColorPick,
    // Ekrandaki metin taranır, kelimeler kutulanır ve kullanıcı metin seçer
    // gibi sürükleyerek istediği kısmı kopyalar.
    TextSelect,
};

struct OverlayResult {
    bool accepted = false;
    RECT selection{};           // Region kipinde; ekran koordinatı
    uint32_t pickedColor = 0;   // ColorPick kipinde; 0xAARRGGBB
    std::wstring pickedText;    // TextSelect kipinde
};

// Seçim arayüzünü çalıştırır ve KULLANICI KARAR VERENE KADAR DÖNMEZ; kendi
// mesaj döngüsünü işletir. Dönüşte `frozen`, sanal ekranın dondurulmuş
// görüntüsüdür ve seçim ondan kırpılabilir.
//
// preferWindowPick: Region kipinde pencere vurgulamasını ayardan bağımsız açar.
// layout: TextSelect kipinde ZORUNLU — çağıran OCR'ı önceden çalıştırıp
//         kelime kutularını verir. Kaplamanın kendisi OCR çağırmaz; tanıma
//         saniyeler sürebilir ve pencere açıldıktan sonra donmuş görünürdü.
[[nodiscard]] OverlayResult RunSelectionOverlay(HINSTANCE instance,
                                                const Settings& settings,
                                                OverlayMode mode,
                                                bool preferWindowPick,
                                                Image& frozen,
                                                const OcrLayout* layout = nullptr);

}  // namespace crisp
