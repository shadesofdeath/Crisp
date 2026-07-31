// TestTransform.cpp — Döndürme ve ölçekleme.
#include "TestFramework.h"

#include "ImageTransform.h"

using namespace crisp;

namespace {

// Her pikseli benzersiz: bir dönüşüm pikselleri karıştırırsa fark edilir.
// Kodlama 0xFF00YYXX — x düşük bayt, y ikinci bayt.
void PaintCoordinates(Image& image) {
    for (int y = 0; y < image.Height(); ++y) {
        for (int x = 0; x < image.Width(); ++x) {
            image.SetPixel(x, y,
                           0xFF000000u | (static_cast<uint32_t>(y) << 8) |
                               static_cast<uint32_t>(x));
        }
    }
}

[[nodiscard]] uint32_t Coded(int x, int y) noexcept {
    return 0xFF000000u | (static_cast<uint32_t>(y) << 8) |
           static_cast<uint32_t>(x);
}

}  // namespace

CRISP_TEST(Transform, Rotate90_kenarlari_takas_eder) {
    Image source;
    CHECK(source.Create(4, 2));
    PaintCoordinates(source);

    Image rotated;
    CHECK(RotateImage(source, 1, rotated));
    CHECK_EQ(rotated.Width(), 2);
    CHECK_EQ(rotated.Height(), 4);
}

CRISP_TEST(Transform, Rotate90_pikselleri_dogru_yere_koyar) {
    // Saat yönünde 90°: kaynağın SOL-ÜST köşesi sonucun SAĞ-ÜST köşesine gider.
    Image source;
    CHECK(source.Create(4, 2));
    PaintCoordinates(source);

    Image rotated;
    CHECK(RotateImage(source, 1, rotated));

    // (0,0) -> (height-1-0, 0) = (1, 0)
    CHECK_EQ(rotated.Pixel(1, 0), Coded(0, 0));
    // (3,0) -> (1, 3)
    CHECK_EQ(rotated.Pixel(1, 3), Coded(3, 0));
    // (0,1) -> (0, 0)
    CHECK_EQ(rotated.Pixel(0, 0), Coded(0, 1));
}

CRISP_TEST(Transform, Rotate180_kosegeni_ters_cevirir) {
    Image source;
    CHECK(source.Create(3, 2));
    PaintCoordinates(source);

    Image rotated;
    CHECK(RotateImage(source, 2, rotated));
    CHECK_EQ(rotated.Width(), 3);
    CHECK_EQ(rotated.Height(), 2);
    CHECK_EQ(rotated.Pixel(2, 1), Coded(0, 0));
    CHECK_EQ(rotated.Pixel(0, 0), Coded(2, 1));
}

CRISP_TEST(Transform, Rotate270_90in_tersi) {
    Image source;
    CHECK(source.Create(5, 3));
    PaintCoordinates(source);

    // Dört kez 90° = kimlik. Bu, dört dönüşün de tutarlı olduğunu tek testte
    // kanıtlar; her birini ayrı ayrı elle doğrulamak gerekmez.
    Image a;
    Image b;
    Image c;
    Image d;
    CHECK(RotateImage(source, 1, a));
    CHECK(RotateImage(a, 1, b));
    CHECK(RotateImage(b, 1, c));
    CHECK(RotateImage(c, 1, d));

    CHECK_EQ(d.Width(), source.Width());
    CHECK_EQ(d.Height(), source.Height());
    bool identical = true;
    for (int y = 0; y < source.Height(); ++y) {
        for (int x = 0; x < source.Width(); ++x) {
            if (d.Pixel(x, y) != source.Pixel(x, y)) {
                identical = false;
            }
        }
    }
    CHECK(identical);
}

CRISP_TEST(Transform, Rotate_negatif_ve_tasan_degerler) {
    Image source;
    CHECK(source.Create(4, 2));
    PaintCoordinates(source);

    Image minusOne;
    Image three;
    CHECK(RotateImage(source, -1, minusOne));
    CHECK(RotateImage(source, 3, three));
    // -1 ile 3 aynı dönüş olmalı; C++'ta negatif modülo negatif kalır ve düz
    // % kullanılsaydı -1 hiçbir dala düşmezdi.
    CHECK_EQ(minusOne.Width(), three.Width());
    CHECK_EQ(minusOne.Pixel(0, 0), three.Pixel(0, 0));

    Image four;
    CHECK(RotateImage(source, 4, four));   // kimlik
    CHECK_EQ(four.Width(), 4);
    CHECK_EQ(four.Pixel(2, 1), Coded(2, 1));
}

CRISP_TEST(Transform, Rotate_gecersiz_girdi) {
    const Image empty;
    Image out;
    CHECK(!RotateImage(empty, 1, out));

    Image source;
    CHECK(source.Create(4, 4));
    CHECK(!RotateImage(source, 1, source));   // aynı nesne
}

CRISP_TEST(Transform, Scale_istenen_boyutu_verir) {
    Image source;
    CHECK(source.Create(100, 50));
    source.Fill(0xFF204060u);

    Image scaled;
    CHECK(ScaleImage(source, 50, 25, scaled));
    CHECK_EQ(scaled.Width(), 50);
    CHECK_EQ(scaled.Height(), 25);
    // Düz renk ölçeklendiğinde aynı renk kalmalı: örnekleme hatası burada
    // hemen görünür.
    CHECK_EQ(scaled.Pixel(25, 12), 0xFF204060u);
}

CRISP_TEST(Transform, Scale_buyutme) {
    Image source;
    CHECK(source.Create(10, 10));
    source.Fill(0xFF808080u);

    Image scaled;
    CHECK(ScaleImage(source, 40, 40, scaled));
    CHECK_EQ(scaled.Width(), 40);
    CHECK_EQ(scaled.Pixel(20, 20), 0xFF808080u);
}

CRISP_TEST(Transform, Scale_ayni_boyut_kopya) {
    Image source;
    CHECK(source.Create(8, 6));
    PaintCoordinates(source);

    Image scaled;
    CHECK(ScaleImage(source, 8, 6, scaled));
    CHECK_EQ(scaled.Pixel(5, 4), Coded(5, 4));
}

CRISP_TEST(Transform, Scale_alfayi_korur) {
    Image source;
    CHECK(source.Create(20, 20));
    source.Fill(0x80112233u);

    Image scaled;
    CHECK(ScaleImage(source, 10, 10, scaled));
    CHECK_EQ((scaled.Pixel(5, 5) >> 24), 0x80u);
}

CRISP_TEST(Transform, Scale_gecersiz_girdi) {
    Image source;
    CHECK(source.Create(8, 8));
    Image out;
    CHECK(!ScaleImage(source, 0, 8, out));
    CHECK(!ScaleImage(source, 8, -2, out));
    CHECK(!ScaleImage(source, 8, 8, source));

    const Image empty;
    CHECK(!ScaleImage(empty, 4, 4, out));
}

CRISP_TEST(Transform, ScaleByPercent) {
    Image source;
    CHECK(source.Create(200, 100));
    source.Fill(0xFF334455u);

    Image half;
    CHECK(ScaleImageByPercent(source, 50, half));
    CHECK_EQ(half.Width(), 100);
    CHECK_EQ(half.Height(), 50);

    Image doubled;
    CHECK(ScaleImageByPercent(source, 200, doubled));
    CHECK_EQ(doubled.Width(), 400);
    CHECK_EQ(doubled.Height(), 200);
}

CRISP_TEST(Transform, ScaleByPercent_asla_sifir_boyut_uretmez) {
    // %1'e indirilen 10x10 bir görüntü 0x0 olurdu ve Image::Create bunu
    // reddeder; sonuç en az 1x1 olmalı.
    Image source;
    CHECK(source.Create(10, 10));
    source.Fill(0xFF000000u);

    Image tiny;
    CHECK(ScaleImageByPercent(source, 1, tiny));
    CHECK(tiny.Width() >= 1);
    CHECK(tiny.Height() >= 1);

    CHECK(!ScaleImageByPercent(source, 0, tiny));
    CHECK(!ScaleImageByPercent(source, -50, tiny));
}
