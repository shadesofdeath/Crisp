// TestGeometry.cpp — Seçim matematiği. Ekran gerektirmez, tamamen belirlenimci.
#include "TestFramework.h"

#include "Geometry.h"

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
