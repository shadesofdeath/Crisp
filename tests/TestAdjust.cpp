// TestAdjust.cpp — Renk ayarlamaları ve konvolüsyon.
#include "TestFramework.h"

#include "ImageAdjust.h"

using namespace crisp;

namespace {

[[nodiscard]] Image Solid(int width, int height, uint32_t color) {
    Image image;
    (void)image.Create(width, height);
    image.Fill(color);
    return image;
}

[[nodiscard]] int Red(uint32_t pixel) noexcept {
    return static_cast<int>((pixel >> 16) & 0xFFu);
}
[[nodiscard]] int Green(uint32_t pixel) noexcept {
    return static_cast<int>((pixel >> 8) & 0xFFu);
}
[[nodiscard]] int Blue(uint32_t pixel) noexcept {
    return static_cast<int>(pixel & 0xFFu);
}

}  // namespace

CRISP_TEST(Adjust, Grayscale_kanallari_esitler) {
    Image image = Solid(4, 4, 0xFF3060C0u);
    ApplyGrayscale(image);
    const uint32_t pixel = image.Pixel(2, 2);
    CHECK_EQ(Red(pixel), Green(pixel));
    CHECK_EQ(Green(pixel), Blue(pixel));
    // 0.299*48 + 0.587*96 + 0.114*192 = 92.6 → 92
    CHECK_EQ(Red(pixel), 92);
}

CRISP_TEST(Adjust, Invert_iki_kez_kimlik) {
    Image image = Solid(3, 3, 0xFF123456u);
    ApplyInvert(image);
    CHECK_EQ(image.Pixel(1, 1), 0xFFEDCBA9u);
    ApplyInvert(image);
    CHECK_EQ(image.Pixel(1, 1), 0xFF123456u);
}

CRISP_TEST(Adjust, Alfa_hicbir_ayarlamada_degismez) {
    // Parlaklığı artırırken saydamlığı da artırmak, kimsenin istemediği bir
    // yan etki olurdu.
    Image image = Solid(2, 2, 0x80204060u);
    AdjustBrightness(image, 50);
    CHECK_EQ((image.Pixel(0, 0) >> 24), 0x80u);
    AdjustContrast(image, -40);
    CHECK_EQ((image.Pixel(0, 0) >> 24), 0x80u);
    AdjustSaturation(image, 100);
    CHECK_EQ((image.Pixel(0, 0) >> 24), 0x80u);
    AdjustGamma(image, 220);
    CHECK_EQ((image.Pixel(0, 0) >> 24), 0x80u);
    ApplyGrayscale(image);
    CHECK_EQ((image.Pixel(0, 0) >> 24), 0x80u);
}

CRISP_TEST(Adjust, Brightness_yonu_ve_kirpmasi) {
    Image lighter = Solid(2, 2, 0xFF808080u);
    AdjustBrightness(lighter, 20);
    CHECK(Red(lighter.Pixel(0, 0)) > 128);

    Image darker = Solid(2, 2, 0xFF808080u);
    AdjustBrightness(darker, -20);
    CHECK(Red(darker.Pixel(0, 0)) < 128);

    // Uçlarda taşma yok.
    Image white = Solid(2, 2, 0xFFFFFFFFu);
    AdjustBrightness(white, 100);
    CHECK_EQ(Red(white.Pixel(0, 0)), 255);

    Image black = Solid(2, 2, 0xFF000000u);
    AdjustBrightness(black, -100);
    CHECK_EQ(Red(black.Pixel(0, 0)), 0);
}

CRISP_TEST(Adjust, Sifir_miktar_hicbir_sey_yapmaz) {
    Image image = Solid(2, 2, 0xFF3C6496u);
    AdjustBrightness(image, 0);
    AdjustContrast(image, 0);
    AdjustSaturation(image, 0);
    AdjustGamma(image, 100);
    ApplySharpen(image, 0);
    CHECK_EQ(image.Pixel(1, 1), 0xFF3C6496u);
}

CRISP_TEST(Adjust, Saturation_eksi_yuz_griye_indirir) {
    Image image = Solid(3, 3, 0xFFC03060u);
    AdjustSaturation(image, -100);
    const uint32_t pixel = image.Pixel(1, 1);
    CHECK_EQ(Red(pixel), Green(pixel));
    CHECK_EQ(Green(pixel), Blue(pixel));
}

CRISP_TEST(Adjust, Contrast_orta_griyi_yerinde_birakir) {
    // Kontrast eğrisi 128 çevresinde döner; tam orta ton sabit kalmalı.
    Image image = Solid(2, 2, 0xFF808080u);
    AdjustContrast(image, 60);
    CHECK(Red(image.Pixel(0, 0)) >= 127 && Red(image.Pixel(0, 0)) <= 129);
}

CRISP_TEST(Adjust, Gamma_yonu_ve_araligi) {
    Image lighter = Solid(2, 2, 0xFF404040u);
    AdjustGamma(lighter, 50);   // 100'ün altı açar
    CHECK(Red(lighter.Pixel(0, 0)) > 64);

    Image darker = Solid(2, 2, 0xFF404040u);
    AdjustGamma(darker, 200);   // üstü koyulaştırır
    CHECK(Red(darker.Pixel(0, 0)) < 64);

    // Aralık dışı değerler kırpılır, çökmez.
    Image extreme = Solid(2, 2, 0xFF808080u);
    AdjustGamma(extreme, -5);
    AdjustGamma(extreme, 100000);
    CHECK(extreme.Valid());
}

CRISP_TEST(Adjust, Convolve_duz_rengi_bozmaz) {
    // Toplamı bölene eşit bir çekirdek, düz bir alanı olduğu gibi bırakmalı;
    // kenar uzatma çalışmasaydı çerçevede koyu bir kuşak kalırdı.
    Image image = Solid(6, 6, 0xFF646464u);
    const int blur[9] = {1, 1, 1, 1, 1, 1, 1, 1, 1};
    Convolve3x3(image, blur, 9, 0);
    for (int y = 0; y < 6; ++y) {
        for (int x = 0; x < 6; ++x) {
            CHECK_EQ(Red(image.Pixel(x, y)), 100);
        }
    }
}

CRISP_TEST(Adjust, Sharpen_kenar_kontrastini_artirir) {
    // Sol yarısı koyu, sağ yarısı açık. Keskinleştirme kenarın iki yanını
    // birbirinden UZAKLAŞTIRMALI.
    Image image;
    CHECK(image.Create(8, 4));
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 8; ++x) {
            image.SetPixel(x, y, x < 4 ? 0xFF505050u : 0xFFA0A0A0u);
        }
    }

    ApplySharpen(image, 80);
    CHECK(Red(image.Pixel(3, 2)) < 0x50);
    CHECK(Red(image.Pixel(4, 2)) > 0xA0);
}

CRISP_TEST(Adjust, Gecersiz_goruntu_guvenli) {
    Image empty;
    ApplyGrayscale(empty);
    ApplyInvert(empty);
    ApplySepia(empty);
    AdjustBrightness(empty, 40);
    AdjustGamma(empty, 150);
    ApplySharpen(empty, 50);
    const int kernel[9] = {0, 0, 0, 0, 1, 0, 0, 0, 0};
    Convolve3x3(empty, kernel, 1, 0);
    Convolve3x3(empty, nullptr, 1, 0);
    CHECK(!empty.Valid());

    // Sıfır bölen sessizce yok sayılır, sıfıra bölme yok.
    Image image = Solid(2, 2, 0xFF102030u);
    Convolve3x3(image, kernel, 0, 0);
    CHECK_EQ(image.Pixel(0, 0), 0xFF102030u);
}
