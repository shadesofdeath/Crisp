// Geometry.h — Kaplamanın kullandığı saf dikdörtgen matematiği.
//
// BU BAŞLIKTAKİ HİÇBİR FONKSİYON WIN32 ÇAĞIRMAZ. Yalnızca RECT/POINT/SIZE
// tiplerini kullanır, ekran durumunu okumaz, pencere oluşturmaz. Bunun sebebi
// tamamen test edilebilirlik: seçim mantığındaki bir hata, kaplamayı elle açıp
// sürüklemeden, doğrudan girdi-çıktı karşılaştırmasıyla yakalanabilmeli.
#pragma once

#include <windows.h>

namespace crisp {
namespace geom {

[[nodiscard]] constexpr LONG Width(const RECT& r) noexcept { return r.right - r.left; }
[[nodiscard]] constexpr LONG Height(const RECT& r) noexcept { return r.bottom - r.top; }

[[nodiscard]] constexpr bool IsEmpty(const RECT& r) noexcept {
    return Width(r) <= 0 || Height(r) <= 0;
}

// İki köşeden normalleştirilmiş dikdörtgen: kullanıcı hangi yöne sürüklerse
// sürüklesin left<right ve top<bottom garanti edilir.
//
// SAĞ/ALT KENAR DIŞLAYICIDIR (Windows RECT geleneği): (10,10)-(20,20) seçimi
// 10 piksel genişliğindedir, 11 değil. Buradaki bir kapsayıcı/dışlayıcı karışımı
// yakalanan görüntüyü bir piksel kaydırır ve gözle fark edilmez.
[[nodiscard]] RECT FromCorners(POINT a, POINT b) noexcept;

// r'yi bounds içine hapseder. r, bounds'tan büyükse bounds döner.
[[nodiscard]] RECT ClampTo(const RECT& r, const RECT& bounds) noexcept;

// Dikdörtgeni her yönde d kadar büyütür (negatif d küçültür), sonra bounds'a
// hapseder.
[[nodiscard]] RECT InflateClamped(const RECT& r, LONG d, const RECT& bounds) noexcept;

// Seçim kullanılabilir mi? Tek tıklama (0x0) ve kaza eseri bir-iki piksellik
// sürüklemeler yakalama başlatmamalı.
[[nodiscard]] bool IsUsableSelection(const RECT& r, LONG minSide) noexcept;

// Büyütecin okuyacağı kaynak dikdörtgen: imlecin çevresinde sourceSide x
// sourceSide'lık kare, ekran sınırlarına hapsedilmiş.
//
// KENARDA KAYDIRILIR, KIRPILMAZ: imleç sol kenardayken kare sola taşarsa
// pencere sağa kaydırılır ve boyutu KORUNUR. Kırpsaydık büyüteç kenarlarda
// farklı bir ölçekte çizerdi ve piksel sayımı yanlış olurdu.
[[nodiscard]] RECT MagnifierSource(POINT cursor, LONG sourceSide,
                                   const RECT& bounds) noexcept;

// Büyüteç panelinin sol-üst köşesi. Varsayılan olarak imlecin sağ-altına
// yerleşir; oraya sığmıyorsa o eksende karşı tarafa geçer.
[[nodiscard]] POINT MagnifierPlacement(POINT cursor, SIZE panelSize, LONG gap,
                                       const RECT& bounds) noexcept;

// Seçim ölçüsünü gösteren etiketin sol-üst köşesi. Seçimin üstüne oturur;
// yukarıda yer yoksa içine, o da olmuyorsa altına iner.
[[nodiscard]] POINT SizeLabelPlacement(const RECT& selection, SIZE labelSize,
                                       LONG gap, const RECT& bounds) noexcept;

// İki nokta arasındaki dikdörtgeni, kenar oranını koruyacak biçimde ayarlar
// (Shift ile kare seçim). anchor sabit kalır, other en yakın kareye çekilir.
[[nodiscard]] POINT SnapToSquare(POINT anchor, POINT other) noexcept;

// --- İğnelenmiş pencere yakınlaştırması --------------------------------------

inline constexpr int kZoomMin = 10;
inline constexpr int kZoomMax = 800;

// Tekerlek adımından yeni yakınlaştırma yüzdesi.
//
// ADIM ÇARPANSAL, SABİT DEĞİL: %10 iken +10 eklemek boyutu ikiye katlar,
// %400 iken aynı ekleme fark ettirmez. Her adım yaklaşık %20 büyütür/küçültür,
// böylece kullanıcı her ölçekte aynı hızda yaklaşmış hisseder.
[[nodiscard]] int ZoomStep(int currentPercent, int wheelDelta) noexcept;

// Yakınlaştırılmış boyut. En az 1x1 döner: sıfır boyutlu pencere oluşturulamaz.
[[nodiscard]] SIZE ScaledSize(SIZE original, int percent) noexcept;

// Pencereyi, verilen ekran noktası görüntüde AYNI yerde kalacak biçimde
// yeniden konumlandırır. İmleç altındaki nokta sabit kalarak yakınlaştırma
// hissi verir; onsuz pencere sol-üst köşeden büyür ve kullanıcı baktığı yeri
// kaybeder.
[[nodiscard]] POINT ZoomAnchoredOrigin(POINT windowTopLeft, POINT anchorScreen,
                                       SIZE oldSize, SIZE newSize) noexcept;


// ---------------------------------------------------------------------------
// Yakınlaştırma ve kaydırma
// ---------------------------------------------------------------------------
// BURADA OLMASININ SEBEBİ: bu dört fonksiyon düzenleyicinin tuval matematiğinin
// tamamı ve hiçbiri pencereye dokunmuyor. Uygulama katmanında dururken projenin
// tek sınanamayan saf geometrisiydi — "yakınlaştırınca imlecin altındaki piksel
// kayıyor mu" sorusunun cevabı ancak elle deneyerek bulunabiliyordu.

// Görüntüyü görünür alana sığdıran ölçek. BÜYÜTMEZ: küçük bir yakalamayı
// pencereye yaymak pikselleri bulanıklaştırır ve kullanıcı çizdiği şeyin
// gerçek boyutunu yanlış tahmin eder. Geçersiz ölçülerde 1.0 döner.
[[nodiscard]] double FitScale(int imageWidth, int imageHeight, int viewWidth,
                              int viewHeight) noexcept;

// Ölçeği [minimum, maximum] aralığına çeker.
[[nodiscard]] double ClampZoom(double zoom, double minimum,
                               double maximum) noexcept;

// Görüntüyü görünür alandan tamamen çıkaracak kaydırmaları engeller.
// Görüntü görünür alandan KÜÇÜKSE kaydırma sıfırlanır: serbest bırakmak,
// kullanıcının resmi köşeye itip "kayboldu" sanması demek olurdu.
[[nodiscard]] POINT ClampPan(POINT pan, int scaledWidth, int scaledHeight,
                             int viewWidth, int viewHeight) noexcept;

// Ölçeklenmiş görüntünün görünür alandaki yeri: ortalanır, sonra pan eklenir.
[[nodiscard]] RECT CanvasRect(const RECT& viewport, int imageWidth,
                              int imageHeight, double scale, POINT pan) noexcept;

// İstemci ⇄ görüntü koordinatı. scale sıfır ya da negatifse başnokta döner.
[[nodiscard]] POINT ViewToImage(POINT client, POINT canvasOrigin,
                                double scale) noexcept;
[[nodiscard]] POINT ImageToView(POINT image, POINT canvasOrigin,
                                double scale) noexcept;

// Yakınlaştırmadan sonra ÇIPANIN ALTINDAKİ PİKSELİ yerinde tutan yeni pan.
//
// Sıra önemli: çıpanın altındaki görüntü noktası ESKİ ölçekle bulunur, o nokta
// YENİ ölçekle nereye düşeceği hesaplanır, fark kaydırmaya eklenir. Ekranın
// ortasına yakınlaştırmak, kullanıcının baktığı yeri her adımda kaybetmesi
// olurdu.
[[nodiscard]] POINT PanForZoomAnchor(POINT anchor, const RECT& viewport,
                                     int imageWidth, int imageHeight,
                                     double oldScale, POINT oldPan,
                                     double newScale) noexcept;
}  // namespace geom
}  // namespace crisp
