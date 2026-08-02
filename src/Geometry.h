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

// Yerleşmiş seçimin eylem çubuğunun sol-üst köşesi. Seçimin ALTINA, sağ
// kenarına hizalı oturur; aşağıda yer yoksa üstüne çıkar, iki tarafta da yer
// yoksa seçimin İÇİNE, alt kenarına iner.
//
// İÇERİ GİRMEK SON ÇARE AMA EKRAN DIŞINA TAŞMAKTAN İYİ. Ekranı kaplayan bir
// seçimde çubuğun sığacağı bir dış kenar yoktur; monitörün dışına konan çubuk
// hiç görünmez, seçimin üstüne binen çubuk ise birkaç yüz pikseli örter — ve
// orası kullanıcının zaten seçtiği, dolayısıyla baktığı yerdir.
//
// Yatayda `bounds` içine sıkıştırılır: seçim ekranın soluna yakınken sağ
// kenarına hizalanan çubuk dışarı taşardı.
[[nodiscard]] POINT ActionBarPlacement(const RECT& selection, SIZE barSize,
                                       LONG gap, const RECT& bounds) noexcept;

// Metinden okunmuş bir seçim.
struct ParsedSelection {
    bool ok = false;
    bool hasPosition = false;   // yalnızca ölçü verilmişse false
    LONG x = 0;
    LONG y = 0;
    LONG width = 0;
    LONG height = 0;
};

// Serbest metinden seçim ölçüsü (ve varsa konumu) okur.
//
// Ctrl+C SEÇİMİN SAYILARINI KOPYALIYORDU, TERSİ YOKTU. "Bu ekran görüntüsü tam
// 1200×630 olmalı" diyen kullanıcının elinde fareyi piksel piksel sürüklemekten
// başka yol yoktu; oysa sayılar zaten bir yerde yazılıydı.
//
// AYIRICI ÖNEMSENMEZ, SAYILAR ÖNEMSENİR. Kabul edilenler `1200x630`,
// `1200 × 630`, `1200, 630`, ve Ctrl+C'nin kendi ürettiği
// `100, 200  1200 × 630`. İki sayı ölçü demek, dört sayı konum ve ölçü.
// Kullanıcının hangi ayırıcıyı yazdığını tahmin etmeye çalışmak yerine
// aradaki her şeyi atlamak, hem daha kısa hem daha affedici.
//
// NEGATİF SAYI YOK: eksi işareti ayırıcı olabilir (`1200-630`) ve bir seçimin
// negatif ölçüsü zaten olamaz. Konumun negatif olabildiği tek yer soldaki
// ikinci monitör; oraya elle sayı yazan kullanıcı yok denecek kadar az ve
// bunun bedeli `1200-630` yazanın hiçbir şey alamaması olurdu.
[[nodiscard]] ParsedSelection ParseSelectionText(const wchar_t* text) noexcept;

// İki nokta arasındaki dikdörtgeni, kenar oranını koruyacak biçimde ayarlar
// (Shift ile kare seçim). anchor sabit kalır, other en yakın kareye çekilir.
[[nodiscard]] POINT SnapToSquare(POINT anchor, POINT other) noexcept;

// Ucu, çıpadan geçen 45°'nin katı olan en yakın ışına kilitler; uzunluk
// KORUNUR (izdüşüm değil, döndürme).
//
// İZDÜŞÜM DEĞİL ÇÜNKÜ İZDÜŞÜM KISALTIR: fareyi 20° tutup Shift'e basan
// kullanıcı, çizginin yönünün düzelmesini bekler, boyunun küçülmesini değil.
[[nodiscard]] POINT SnapToAngle(POINT anchor, POINT other) noexcept;

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

// --- Seçim tutamakları (GeometryGrab.cpp) -----------------------------------
//
// Seçim, fare bırakıldıktan sonra ayarlanabiliyor: kenarlarından
// boyutlandırılıyor, içinden taşınıyor. Aşağıdakiler o işin bütün aritmetiği ve
// hepsi saf — pencere yok, DPI yok, ekran yok — çünkü yanlış gidebilecek şeyler
// burada toplanıyor ve buranın tamamı sınanabilmeli.

// Farenin seçimin neresini tuttuğu.
enum class Grab { None, New, Move, N, S, E, W, NE, NW, SE, SW };

// Tutamak kutuları, seçimle aynı koordinat uzayında.
//
// DÖNEN SAYI 0, 4 YA DA 8. Küçük bir seçimde sekiz tutamak birbirini yer:
// karşılıklı olanlar üst üste biner, kenar ortaları köşelere değer, hangisini
// tuttuğunuz rastlantıya kalır ve taşımaya hiç yer kalmaz. Bu yüzden seçim
// küçüldükçe tutamaklar önce dörde iner, sonra tamamen kalkar ve dikdörtgenin
// tamamı "taşı" olur:
//
//   0 tutamak : kenarlardan biri 3*handleSize'dan kısa
//   4 (köşe)  : ikisi de >= 3*handleSize, biri < 5*handleSize
//   8         : ikisi de >= 5*handleSize
//
// Sıra sabittir ve İSABET SIRASIDIR: NW, NE, SW, SE, N, S, W, E. Köşeler önce,
// çünkü bir köşe kutusu kenar kutusuyla kesiştiğinde kullanıcının kastettiği
// köşedir.
//
// ÇİZEN DE TUTAN DA BURAYI ÇAĞIRIR. İki ayrı hesap, çizilen tutamağın
// tutulamadığı bir sürüme giden en kısa yol olurdu.
//
// DPI ölçeklemesi çağıranda kalır: `MulDiv` bir Win32 çağrısıdır ve bu başlık
// Win32 çağırmaz.
int HandleRects(const RECT& selection, LONG handleSize, RECT outRects[8],
                Grab outGrabs[8]) noexcept;

// İmleç seçimin neresinde. Önce tutamaklar (yukarıdaki sırayla), sonra iç
// bölge, sonra `None`.
[[nodiscard]] Grab HitTestSelection(const RECT& selection, POINT p,
                                    LONG grabSize) noexcept;

// `r`yi kaydırır, sonra BOYUTUNU KORUYARAK `bounds` içine geri iter.
//
// ClampTo BURADA KULLANILAMAZ: her kenarı bağımsız kırptığı için kenara dayanan
// bir seçimi durdurmak yerine daraltırdı. Taşımak boyutu değiştirmez.
//
// `r`, `bounds`tan büyükse sığdırmanın yolu yoktur; orada ClampTo'ya düşer.
[[nodiscard]] RECT OffsetClamped(const RECT& r, LONG dx, LONG dy,
                                 const RECT& bounds) noexcept;

// Tutulan kenarı imlece taşır; ötekiler yerinde kalır.
//
// FromCorners KULLANILMAZ. Bir kenarı karşısının ötesine sürüklemek, tutulan
// tutamağın sürükleme ortasında ad değiştirmesi demek olurdu; hiçbir
// düzenleyicide böyle olmaz. Kenar `minSide`da durur, dönmez.
[[nodiscard]] RECT ResizeByGrab(const RECT& origin, Grab grab, POINT cursor,
                                LONG minSide, const RECT& bounds) noexcept;
}  // namespace geom
}  // namespace crisp
