// TestGeometry.cpp — Seçim matematiği. Ekran gerektirmez, tamamen belirlenimci.
#include "TestFramework.h"

#include "Geometry.h"

#include <cmath>
#include <cstdlib>

using namespace crisp::geom;

namespace {
// Testlerin çoğunda kullanılan tek monitörlük sınır.
constexpr RECT kScreen{0, 0, 1920, 1080};
// Sol monitörün negatif koordinatta olduğu iki monitörlü kurulum.
constexpr RECT kMultiScreen{-1920, 0, 1920, 1080};
}  // namespace

CRISP_TEST(Geometry, FromCorners_her_yonde_normallestirir) {
    // Sağ-aşağı sürükleme
    CHECK_RECT(FromCorners(POINT{10, 20}, POINT{110, 220}), 10, 20, 110, 220);
    // Sol-yukarı sürükleme aynı dikdörtgeni vermeli
    CHECK_RECT(FromCorners(POINT{110, 220}, POINT{10, 20}), 10, 20, 110, 220);
    // Sağ-yukarı
    CHECK_RECT(FromCorners(POINT{10, 220}, POINT{110, 20}), 10, 20, 110, 220);
    // Sol-aşağı
    CHECK_RECT(FromCorners(POINT{110, 20}, POINT{10, 220}), 10, 20, 110, 220);
}

CRISP_TEST(Geometry, FromCorners_negatif_koordinat) {
    CHECK_RECT(FromCorners(POINT{-500, -300}, POINT{-100, -100}), -500, -300, -100,
               -100);
}

CRISP_TEST(Geometry, Width_Height_dislayici_sag_alt_kenar) {
    // (10,10)-(20,20) ON piksel genişliğindedir, on bir değil. Bu bir off-by-one
    // tuzağı: yakalanan görüntü bir piksel kayarsa gözle fark edilmez.
    const RECT r{10, 10, 20, 20};
    CHECK_EQ(Width(r), 10);
    CHECK_EQ(Height(r), 10);
}

CRISP_TEST(Geometry, IsEmpty_sifir_ve_ters_dikdortgen) {
    CHECK(IsEmpty(RECT{5, 5, 5, 10}));    // sıfır genişlik
    CHECK(IsEmpty(RECT{5, 5, 10, 5}));    // sıfır yükseklik
    CHECK(IsEmpty(RECT{10, 10, 5, 5}));   // ters
    CHECK(!IsEmpty(RECT{0, 0, 1, 1}));    // tek piksel geçerlidir
}

CRISP_TEST(Geometry, ClampTo_disari_tasani_iceri_ceker) {
    CHECK_RECT(ClampTo(RECT{-50, -50, 100, 100}, kScreen), 0, 0, 100, 100);
    CHECK_RECT(ClampTo(RECT{1900, 1000, 2000, 1200}, kScreen), 1900, 1000, 1920,
               1080);
    // Tamamen içerideki dokunulmadan geçer
    CHECK_RECT(ClampTo(RECT{10, 10, 20, 20}, kScreen), 10, 10, 20, 20);
    // Sınırdan büyük olan sınıra oturur
    CHECK_RECT(ClampTo(RECT{-100, -100, 5000, 5000}, kScreen), 0, 0, 1920, 1080);
}

CRISP_TEST(Geometry, ClampTo_negatif_kokenli_sanal_ekran) {
    // Sol monitörün tamamı negatif koordinatta; hapsetme oraya da izin vermeli.
    CHECK_RECT(ClampTo(RECT{-1000, 100, -500, 400}, kMultiScreen), -1000, 100, -500,
               400);
    CHECK_RECT(ClampTo(RECT{-3000, 100, -500, 400}, kMultiScreen), -1920, 100, -500,
               400);
}

CRISP_TEST(Geometry, InflateClamped_buyutur_ve_hapseder) {
    CHECK_RECT(InflateClamped(RECT{100, 100, 200, 200}, 10, kScreen), 90, 90, 210,
               210);
    // Kenarda büyütme sınırı aşmaz
    CHECK_RECT(InflateClamped(RECT{0, 0, 100, 100}, 10, kScreen), 0, 0, 110, 110);
}

CRISP_TEST(Geometry, InflateClamped_kucultme_dikdortgeni_ters_cevirmez) {
    // 10x10'u her yönden 20 küçültmek matematiksel olarak ters dikdörtgen verir;
    // sonuç çökmüş ama GEÇERLİ olmalı, ters değil.
    const RECT result = InflateClamped(RECT{100, 100, 110, 110}, -20, kScreen);
    CHECK(!IsEmpty(result) || (Width(result) == 0 && Height(result) == 0));
    CHECK(result.right >= result.left);
    CHECK(result.bottom >= result.top);
}

CRISP_TEST(Geometry, IsUsableSelection_kazara_tiklamayi_eler) {
    CHECK(!IsUsableSelection(RECT{10, 10, 10, 10}, 4));   // tek tıklama
    CHECK(!IsUsableSelection(RECT{10, 10, 12, 12}, 4));   // 2x2, eşiğin altı
    CHECK(IsUsableSelection(RECT{10, 10, 14, 14}, 4));    // tam eşik
    CHECK(IsUsableSelection(RECT{10, 10, 200, 100}, 4));
    // Bir eksen yeterliyken diğeri değilse kullanılamaz
    CHECK(!IsUsableSelection(RECT{10, 10, 200, 12}, 4));
}

CRISP_TEST(Geometry, MagnifierSource_merkezler) {
    const RECT source = MagnifierSource(POINT{500, 400}, 20, kScreen);
    CHECK_RECT(source, 490, 390, 510, 410);
    CHECK_EQ(Width(source), 20);
    CHECK_EQ(Height(source), 20);
}

CRISP_TEST(Geometry, MagnifierSource_kenarda_kaydirir_boyutu_korur) {
    // Sol-üst köşe: kare sola/yukarı taşamaz, KAYDIRILIR ve boyutu korunur.
    // Kırpsaydık büyüteç kenarlarda farklı ölçekte çizerdi.
    const RECT topLeft = MagnifierSource(POINT{2, 3}, 20, kScreen);
    CHECK_RECT(topLeft, 0, 0, 20, 20);
    CHECK_EQ(Width(topLeft), 20);

    const RECT bottomRight = MagnifierSource(POINT{1918, 1078}, 20, kScreen);
    CHECK_RECT(bottomRight, 1900, 1060, 1920, 1080);
    CHECK_EQ(Width(bottomRight), 20);
    CHECK_EQ(Height(bottomRight), 20);
}

CRISP_TEST(Geometry, MagnifierSource_kaynak_ekrandan_buyukse_ekrani_verir) {
    const RECT huge = MagnifierSource(POINT{100, 100}, 5000, kScreen);
    CHECK_RECT(huge, 0, 0, 1920, 1080);
}

CRISP_TEST(Geometry, MagnifierSource_sifir_kenar_cokmus_dikdortgen) {
    const RECT degenerate = MagnifierSource(POINT{100, 100}, 0, kScreen);
    CHECK(IsEmpty(degenerate));
}

CRISP_TEST(Geometry, MagnifierPlacement_varsayilan_sag_alt) {
    const POINT p = MagnifierPlacement(POINT{500, 400}, SIZE{120, 120}, 16, kScreen);
    CHECK_POINT(p, 516, 416);
}

CRISP_TEST(Geometry, MagnifierPlacement_kenarda_karsi_tarafa_gecer) {
    // Sağ kenar: panel sola geçmeli
    const POINT right = MagnifierPlacement(POINT{1900, 400}, SIZE{120, 120}, 16,
                                           kScreen);
    CHECK_EQ(right.x, 1900 - 16 - 120);

    // Alt kenar: panel yukarı geçmeli
    const POINT bottom = MagnifierPlacement(POINT{500, 1070}, SIZE{120, 120}, 16,
                                            kScreen);
    CHECK_EQ(bottom.y, 1070 - 16 - 120);

    // Sağ-alt köşe: ikisi birden
    const POINT corner = MagnifierPlacement(POINT{1910, 1070}, SIZE{120, 120}, 16,
                                            kScreen);
    CHECK_EQ(corner.x, 1910 - 16 - 120);
    CHECK_EQ(corner.y, 1070 - 16 - 120);
}

CRISP_TEST(Geometry, MagnifierPlacement_panel_ekrandan_buyukse_iceride_kalir) {
    // Karşı tarafa geçmek de yetmiyorsa sonuç yine sınırların içinde olmalı.
    const RECT tiny{0, 0, 100, 100};
    const POINT p = MagnifierPlacement(POINT{50, 50}, SIZE{200, 200}, 16, tiny);
    CHECK(p.x >= tiny.left);
    CHECK(p.y >= tiny.top);
}

CRISP_TEST(Geometry, SizeLabelPlacement_ustte_yer_varsa_uste_koyar) {
    const POINT p = SizeLabelPlacement(RECT{200, 300, 400, 500}, SIZE{90, 20}, 6,
                                       kScreen);
    CHECK_POINT(p, 200, 300 - 6 - 20);
}

CRISP_TEST(Geometry, SizeLabelPlacement_ust_kenarda_icine_iner) {
    // Seçim ekranın en üstünde: etiket yukarı sığmaz, seçimin içine girmeli.
    const POINT p = SizeLabelPlacement(RECT{200, 0, 400, 200}, SIZE{90, 20}, 6,
                                       kScreen);
    CHECK(p.y >= 0);
    CHECK(p.y < 200);
}

CRISP_TEST(Geometry, SizeLabelPlacement_kisa_secimde_altina_tasar) {
    // Hem yukarıda yer yok hem seçim etiketi barındıramayacak kadar kısa.
    const POINT p = SizeLabelPlacement(RECT{200, 0, 400, 10}, SIZE{90, 20}, 6,
                                       kScreen);
    CHECK(p.y >= 10);
}

CRISP_TEST(Geometry, SnapToSquare_uzun_kenari_alir) {
    // Yatayda 200, dikeyde 50 → kenar 200
    CHECK_POINT(SnapToSquare(POINT{100, 100}, POINT{300, 150}), 300, 300);
    // Dikeyde daha uzun
    CHECK_POINT(SnapToSquare(POINT{100, 100}, POINT{150, 400}), 400, 400);
}

CRISP_TEST(Geometry, SnapToSquare_yonu_korur) {
    // Sola yukarı sürükleme: kare de sola yukarı büyümeli
    CHECK_POINT(SnapToSquare(POINT{500, 500}, POINT{300, 450}), 300, 300);
    // Sağa yukarı
    CHECK_POINT(SnapToSquare(POINT{500, 500}, POINT{700, 450}), 700, 300);
    // Sola aşağı
    CHECK_POINT(SnapToSquare(POINT{500, 500}, POINT{300, 550}), 300, 700);
}

CRISP_TEST(Geometry, SnapToSquare_sifir_surukleme) {
    CHECK_POINT(SnapToSquare(POINT{100, 100}, POINT{100, 100}), 100, 100);
}

CRISP_TEST(Geometry, SnapToAngle_en_yakin_45_dereceye_kilitler) {
    const POINT anchor{100, 100};
    // Neredeyse yatay → tam yatay.
    CHECK_POINT(SnapToAngle(anchor, POINT{200, 105}), 200, 100);
    // Neredeyse dikey → tam dikey.
    CHECK_POINT(SnapToAngle(anchor, POINT{104, 200}), 100, 200);
    // Neredeyse 45° → tam 45° (uzunluk korunur, bileşenler eşitlenir).
    const POINT diagonal = SnapToAngle(anchor, POINT{180, 170});
    CHECK_EQ(diagonal.x - anchor.x, diagonal.y - anchor.y);
}

CRISP_TEST(Geometry, SnapToAngle_uzunlugu_korur) {
    // İZDÜŞÜM DEĞİL DÖNDÜRME: 20° tutup Shift'e basan kullanıcı çizginin
    // yönünün düzelmesini bekler, boyunun kısalmasını değil.
    const POINT anchor{0, 0};
    const POINT result = SnapToAngle(anchor, POINT{100, 30});
    const double length = std::sqrt(static_cast<double>(result.x) * result.x +
                                    static_cast<double>(result.y) * result.y);
    const double original = std::sqrt(100.0 * 100.0 + 30.0 * 30.0);
    CHECK(length > original - 1.5 && length < original + 1.5);
}

CRISP_TEST(Geometry, SnapToAngle_sifir_surukleme_guvenli) {
    // Sıfır uzunlukta açı tanımsızdır; atan2(0,0) sonucu kullanmak imleci
    // rastgele bir yöne fırlatırdı.
    CHECK_POINT(SnapToAngle(POINT{50, 50}, POINT{50, 50}), 50, 50);
}

CRISP_TEST(Zoom, ZoomStep_carpansal_buyur) {
    // Adım SABİT olsaydı %10'da +20 boyutu üçe katlar, %400'de fark etmezdi.
    const int fromSmall = ZoomStep(100, WHEEL_DELTA);
    CHECK(fromSmall > 100);
    CHECK(fromSmall <= 130);

    // Aynı çentik büyük ölçekte ORANSAL olarak benzer bir artış vermeli.
    const int fromLarge = ZoomStep(400, WHEEL_DELTA);
    CHECK(fromLarge > 400);
    CHECK(fromLarge <= 500);
}

CRISP_TEST(Zoom, ZoomStep_sinirlarda_durur) {
    // Üst sınırda takılı kalmalı, taşmamalı.
    int value = kZoomMax;
    for (int i = 0; i < 10; ++i) {
        value = ZoomStep(value, WHEEL_DELTA);
    }
    CHECK_EQ(value, kZoomMax);

    value = kZoomMin;
    for (int i = 0; i < 10; ++i) {
        value = ZoomStep(value, -WHEEL_DELTA);
    }
    CHECK_EQ(value, kZoomMin);
}

CRISP_TEST(Zoom, ZoomStep_kucultme_daima_ilerler) {
    // +1/-1 sabitleri olmasaydı küçük değerlerde tam sayı bölmesi 0 verir ve
    // yakınlaştırma bir yerde SIKIŞIRDI.
    int value = 100;
    for (int i = 0; i < 40; ++i) {
        const int next = ZoomStep(value, -WHEEL_DELTA);
        CHECK(next < value || next == kZoomMin);
        value = next;
    }
    CHECK_EQ(value, kZoomMin);
}

CRISP_TEST(Zoom, ZoomStep_buyutme_daima_ilerler) {
    int value = kZoomMin;
    for (int i = 0; i < 60; ++i) {
        const int next = ZoomStep(value, WHEEL_DELTA);
        CHECK(next > value || next == kZoomMax);
        value = next;
    }
    CHECK_EQ(value, kZoomMax);
}

CRISP_TEST(Zoom, ZoomStep_sifir_delta_degistirmez) {
    CHECK_EQ(ZoomStep(137, 0), 137);
}

CRISP_TEST(Zoom, ZoomStep_kucuk_delta_en_az_bir_adim) {
    // Yüksek çözünürlüklü tekerlekler 120'den küçük değerler gönderir;
    // tam sayı bölmesi 0 verirse hiç yakınlaştırma olmazdı.
    CHECK(ZoomStep(100, 30) > 100);
    CHECK(ZoomStep(100, -30) < 100);
}

CRISP_TEST(Zoom, ZoomStep_araligin_disindaki_girdiyi_duzeltir) {
    CHECK(ZoomStep(-50, 0) >= kZoomMin);
    CHECK(ZoomStep(99999, 0) <= kZoomMax);
}

CRISP_TEST(Zoom, ScaledSize_orani_korur) {
    const SIZE half = ScaledSize(SIZE{400, 300}, 50);
    CHECK_EQ(half.cx, 200);
    CHECK_EQ(half.cy, 150);

    const SIZE same = ScaledSize(SIZE{400, 300}, 100);
    CHECK_EQ(same.cx, 400);
    CHECK_EQ(same.cy, 300);

    const SIZE twice = ScaledSize(SIZE{400, 300}, 200);
    CHECK_EQ(twice.cx, 800);
    CHECK_EQ(twice.cy, 600);
}

CRISP_TEST(Zoom, ScaledSize_asla_sifir_donmez) {
    // Sıfır boyutlu pencere oluşturulamaz; 1x1'e yuvarlanmalı.
    const SIZE tiny = ScaledSize(SIZE{5, 3}, kZoomMin);
    CHECK(tiny.cx >= 1);
    CHECK(tiny.cy >= 1);
}

CRISP_TEST(Zoom, ZoomAnchoredOrigin_capayi_sabit_tutar) {
    // Pencere (100,100)'de 200x200. Çapa tam ortada: (200,200).
    // İki katına çıkınca çapa yine (200,200) ekran noktasında kalmalı.
    const POINT origin = ZoomAnchoredOrigin(POINT{100, 100}, POINT{200, 200},
                                            SIZE{200, 200}, SIZE{400, 400});
    // Çapa pencerenin ortasındaydı (%50); yeni boyutta da ortada olmalı:
    // origin = 200 - 400*0.5 = 0
    CHECK_POINT(origin, 0, 0);
}

CRISP_TEST(Zoom, ZoomAnchoredOrigin_sol_ust_kosede_sabit) {
    // Çapa pencerenin sol-üst köşesindeyse köşe hiç oynamamalı.
    const POINT origin = ZoomAnchoredOrigin(POINT{300, 200}, POINT{300, 200},
                                            SIZE{100, 100}, SIZE{500, 500});
    CHECK_POINT(origin, 300, 200);
}

CRISP_TEST(Zoom, ZoomAnchoredOrigin_bozuk_eski_boyut_guvenli) {
    const POINT origin = ZoomAnchoredOrigin(POINT{10, 20}, POINT{50, 60},
                                            SIZE{0, 0}, SIZE{100, 100});
    CHECK_POINT(origin, 10, 20);
}

// --- Yakınlaştırma ve kaydırma -----------------------------------------------

CRISP_TEST(Geometry, FitScale_kucultur_ama_buyutmez) {
    // 1000x500 görüntü 500x500 alana: yatayda 0,5 sığar, dikeyde 1,0.
    CHECK(FitScale(1000, 500, 500, 500) > 0.49);
    CHECK(FitScale(1000, 500, 500, 500) < 0.51);
    // Alandan küçük görüntü BÜYÜTÜLMEZ.
    CHECK_EQ(static_cast<int>(FitScale(100, 50, 800, 600) * 100), 100);
    // Bozuk ölçüler çökertmemeli.
    CHECK_EQ(static_cast<int>(FitScale(0, 50, 800, 600) * 100), 100);
    CHECK_EQ(static_cast<int>(FitScale(100, 50, 0, 600) * 100), 100);
}

CRISP_TEST(Geometry, ClampZoom_araliga_ceker) {
    CHECK(ClampZoom(0.01, 0.1, 8.0) > 0.099);
    CHECK(ClampZoom(99.0, 0.1, 8.0) < 8.001);
    CHECK(ClampZoom(2.0, 0.1, 8.0) > 1.999);
}

CRISP_TEST(Geometry, ClampPan_kucuk_goruntuyu_ortalar) {
    // Görüntü görünür alandan küçükse kaydırma yoktur.
    CHECK_POINT(ClampPan(POINT{300, -200}, 100, 100, 800, 600), 0, 0);
}

CRISP_TEST(Geometry, ClampPan_buyuk_goruntude_sinir) {
    // 1200 genişlik, 800 görünür: taşma 400, sınır ±200.
    CHECK_POINT(ClampPan(POINT{999, 0}, 1200, 600, 800, 600), 200, 0);
    CHECK_POINT(ClampPan(POINT{-999, 0}, 1200, 600, 800, 600), -200, 0);
    CHECK_POINT(ClampPan(POINT{50, 0}, 1200, 600, 800, 600), 50, 0);
}

CRISP_TEST(Geometry, CanvasRect_ortalar_ve_pan_ekler) {
    const RECT viewport{0, 0, 800, 600};
    const RECT canvas = CanvasRect(viewport, 400, 300, 1.0, POINT{0, 0});
    CHECK_RECT(canvas, 200, 150, 600, 450);

    const RECT shifted = CanvasRect(viewport, 400, 300, 1.0, POINT{20, -10});
    CHECK_RECT(shifted, 220, 140, 620, 440);
}

CRISP_TEST(Geometry, ViewToImage_ve_geri_donusur) {
    const POINT origin{100, 50};
    const POINT image = ViewToImage(POINT{300, 250}, origin, 2.0);
    CHECK_POINT(image, 100, 100);
    CHECK_POINT(ImageToView(image, origin, 2.0), 300, 250);

    // Sıfır ölçek bölme hatası vermemeli.
    CHECK_POINT(ViewToImage(POINT{300, 250}, origin, 0.0), 0, 0);
}

CRISP_TEST(Geometry, PanForZoomAnchor_cipayi_yerinde_tutar) {
    // GERÇEK DAVRANIŞ SINAMASI: yakınlaştırdıktan sonra çıpanın altındaki
    // GÖRÜNTÜ NOKTASI aynı kalmalı. Elle denemek yerine ölçülebilen tek şey bu.
    const RECT viewport{0, 0, 800, 600};
    const POINT anchor{600, 200};

    const POINT before = ViewToImage(
        anchor,
        POINT{CanvasRect(viewport, 1000, 800, 1.0, POINT{0, 0}).left,
              CanvasRect(viewport, 1000, 800, 1.0, POINT{0, 0}).top},
        1.0);

    const POINT pan = PanForZoomAnchor(anchor, viewport, 1000, 800, 1.0,
                                             POINT{0, 0}, 2.0);
    const RECT after = CanvasRect(viewport, 1000, 800, 2.0, pan);
    const POINT nowAt =
        ViewToImage(anchor, POINT{after.left, after.top}, 2.0);

    // Tamsayı yuvarlaması yüzünden bir piksel sapma kabul edilebilir; iki
    // piksel, ölçek değiştikçe biriken bir kayma demektir.
    CHECK(std::abs(nowAt.x - before.x) <= 1);
    CHECK(std::abs(nowAt.y - before.y) <= 1);
}

CRISP_TEST(Geometry, PanForZoomAnchor_sinirlarda_kalir) {
    const RECT viewport{0, 0, 800, 600};
    // Uzaklaştırırken görüntü görünür alandan küçülürse kaydırma sıfırlanmalı.
    const POINT pan = PanForZoomAnchor(POINT{790, 590}, viewport, 1000, 800,
                                             1.0, POINT{100, 100}, 0.25);
    CHECK_POINT(pan, 0, 0);
}
