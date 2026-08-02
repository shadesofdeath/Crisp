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

#include "AlphaLayer.h"
#include "Capture.h"
#include "OcrLayout.h"

#include <windows.h>

namespace crisp {

// Tutamağın ÇİZİLEN ve TUTULAN boyutu, 96 dpi mantıksal piksel.
//
// TUTMA ALANI ÇİZİLENDEN BÜYÜK: 7 piksellik bir kare fare hedefi değil. Çizim
// küçük ve zarif, hedef geniş; ikisi ayrı sayı olduğu için ikisi de kendi işine
// göre seçilebiliyor.
//
// BOYAMA BAŞLIĞINDA, ÇÜNKÜ İKİ TARAF DA OKUYOR: çizen OverlayPaint.cpp, tutan
// OverlayAdjust.cpp. Çizilen tutamağın tutulamadığı bir sürüme giden en kısa
// yol, bu sayıların iki yerde ayrı ayrı durmasıydı.
inline constexpr int kHandleDrawSide = 7;
inline constexpr int kHandleGrabSide = 13;

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
    // ARTI İMLEÇ: ekran boyunca uzanan iki ince çizgi. Büyüteç imlecin
    // ALTINDAKİ pikseli gösterir ama seçimin kenarını uzaktaki bir
    // pencereyle hizalamaya yaramaz; bunun için ekranı kesen bir çizgi
    // gerekiyor.
    bool crosshair = true;

    // Seçim bırakıldı ve artık ayarlanabilir: tutamaklarından boyutlandırılıyor,
    // içinden taşınıyor. `OverlayState::settled`den yansıtılır, tıpkı
    // `dragging` gibi.
    //
    // Boyamayı üç yerde değiştiriyor: artı imleç kalkar (nişan alma bitti),
    // pencere vurgusu kalkar (pencere seçme bitti), ipucu farklı satırı gösterir.
    bool settled = false;

    // --- Metin seçme kipi ----------------------------------------------------
    bool textSelect = false;
    const OcrLayout* layout = nullptr;   // sahibi kaplama durumudur
    AlphaLayer* layer = nullptr;         // yeniden kullanılan çizim yüzeyi
    int hoverWord = -1;
    int hoverLine = -1;
    int selectionFirst = -1;   // -1: seçim yok
    int selectionLast = -1;

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
// `dimPercent` 0..80: seçim dışının ne kadar karartılacağı.
//
// AYAR OLDU ÇÜNKÜ TEK BİR DEĞER HERKESE UYMUYOR: koyu bir masaüstünde %40
// karartma seçimi zar zor ayırt ettiriyor, açık bir masaüstünde ise fazla
// geliyor.
[[nodiscard]] bool BuildDimmedCopy(const Image& source, unsigned dimPercent,
                                   Image& out);

}  // namespace crisp
