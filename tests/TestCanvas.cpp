// TestCanvas.cpp — Aynalama, tuval boyutlandırma, otomatik kırpma.
#include "TestFramework.h"

#include "ImageTransform.h"

using namespace crisp;

namespace {

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

CRISP_TEST(Canvas, Flip_yatay_sutunlari_ters_cevirir) {
    Image source;
    CHECK(source.Create(4, 3));
    PaintCoordinates(source);

    Image flipped;
    CHECK(FlipImage(source, FlipAxis::Horizontal, flipped));
    CHECK_EQ(flipped.Width(), 4);
    CHECK_EQ(flipped.Height(), 3);
    CHECK_EQ(flipped.Pixel(0, 1), Coded(3, 1));
    CHECK_EQ(flipped.Pixel(3, 1), Coded(0, 1));
    CHECK_EQ(flipped.Pixel(1, 2), Coded(2, 2));
}

CRISP_TEST(Canvas, Flip_dikey_satirlari_ters_cevirir) {
    Image source;
    CHECK(source.Create(4, 3));
    PaintCoordinates(source);

    Image flipped;
    CHECK(FlipImage(source, FlipAxis::Vertical, flipped));
    CHECK_EQ(flipped.Pixel(1, 0), Coded(1, 2));
    CHECK_EQ(flipped.Pixel(1, 2), Coded(1, 0));
}

CRISP_TEST(Canvas, Flip_iki_kez_kimlik) {
    // Aynalama KAYIPSIZDIR: iki kez uygulamak özgün pikselleri geri vermeli.
    Image source;
    CHECK(source.Create(7, 5));
    PaintCoordinates(source);

    Image once;
    Image twice;
    CHECK(FlipImage(source, FlipAxis::Horizontal, once));
    CHECK(FlipImage(once, FlipAxis::Horizontal, twice));
    bool identical = true;
    for (int y = 0; y < source.Height(); ++y) {
        for (int x = 0; x < source.Width(); ++x) {
            if (twice.Pixel(x, y) != source.Pixel(x, y)) {
                identical = false;
            }
        }
    }
    CHECK(identical);
}

CRISP_TEST(Canvas, Flip_gecersiz_girdi) {
    const Image empty;
    Image out;
    CHECK(!FlipImage(empty, FlipAxis::Horizontal, out));

    Image source;
    CHECK(source.Create(4, 4));
    CHECK(!FlipImage(source, FlipAxis::Vertical, source));
}

CRISP_TEST(Canvas, ResizeCanvas_buyutur_ve_doldurur) {
    Image source;
    CHECK(source.Create(2, 2));
    source.Fill(0xFF00FF00u);

    Image bigger;
    CHECK(ResizeCanvas(source, 6, 6, 2, 2, 0xFF112233u, bigger));
    CHECK_EQ(bigger.Width(), 6);
    CHECK_EQ(bigger.Height(), 6);
    // Kenar boşluğu dolgu renginde.
    CHECK_EQ(bigger.Pixel(0, 0), 0xFF112233u);
    CHECK_EQ(bigger.Pixel(5, 5), 0xFF112233u);
    // Görüntü verilen konumda.
    CHECK_EQ(bigger.Pixel(2, 2), 0xFF00FF00u);
    CHECK_EQ(bigger.Pixel(3, 3), 0xFF00FF00u);
    CHECK_EQ(bigger.Pixel(4, 4), 0xFF112233u);
}

CRISP_TEST(Canvas, ResizeCanvas_negatif_offset_kirpar) {
    // Tuvali küçültmek CropImage'in reddettiği bir istektir; burada geçerlidir.
    Image source;
    CHECK(source.Create(4, 4));
    PaintCoordinates(source);

    Image smaller;
    CHECK(ResizeCanvas(source, 2, 2, -1, -1, 0xFF000000u, smaller));
    CHECK_EQ(smaller.Width(), 2);
    CHECK_EQ(smaller.Pixel(0, 0), Coded(1, 1));
    CHECK_EQ(smaller.Pixel(1, 1), Coded(2, 2));
}

CRISP_TEST(Canvas, ResizeCanvas_tamamen_disarida_bos_tuval) {
    Image source;
    CHECK(source.Create(3, 3));
    source.Fill(0xFFFFFFFFu);

    Image out;
    CHECK(ResizeCanvas(source, 3, 3, 10, 10, 0xFF010203u, out));
    CHECK_EQ(out.Pixel(1, 1), 0xFF010203u);
}

CRISP_TEST(Canvas, ResizeCanvas_gecersiz_girdi) {
    Image source;
    CHECK(source.Create(4, 4));
    Image out;
    CHECK(!ResizeCanvas(source, 0, 4, 0, 0, 0, out));
    CHECK(!ResizeCanvas(source, 4, -1, 0, 0, 0, out));
    CHECK(!ResizeCanvas(source, 4, 4, 0, 0, 0, source));
}

CRISP_TEST(Canvas, AutoCrop_duz_kenarlari_atar) {
    Image source;
    CHECK(source.Create(10, 8));
    source.Fill(0xFFFFFFFFu);
    // Ortada 4x2'lik bir kırmızı blok.
    for (int y = 3; y < 5; ++y) {
        for (int x = 2; x < 6; ++x) {
            source.SetPixel(x, y, 0xFFFF0000u);
        }
    }

    Image cropped;
    CHECK(AutoCropImage(source, 0, cropped));
    CHECK_EQ(cropped.Width(), 4);
    CHECK_EQ(cropped.Height(), 2);
    CHECK_EQ(cropped.Pixel(0, 0), 0xFFFF0000u);
    CHECK_EQ(cropped.Pixel(3, 1), 0xFFFF0000u);
}

CRISP_TEST(Canvas, AutoCrop_tolerans_yakin_tonlari_da_kirpar) {
    Image source;
    CHECK(source.Create(8, 6));
    source.Fill(0xFFFEFEFEu);   // "beyaz" ama tam değil (JPEG'den geçmiş gibi)
    for (int y = 2; y < 4; ++y) {
        for (int x = 3; x < 5; ++x) {
            source.SetPixel(x, y, 0xFF000000u);
        }
    }

    Image strict;
    CHECK(AutoCropImage(source, 0, strict));
    CHECK_EQ(strict.Width(), 2);

    // Kenarda tek bir piksellik sapma bile toleranssız kırpmayı durdurur.
    source.SetPixel(0, 0, 0xFFFAFAFAu);
    Image loose;
    CHECK(AutoCropImage(source, 8, loose));
    CHECK_EQ(loose.Width(), 2);
    CHECK_EQ(loose.Height(), 2);
}

CRISP_TEST(Canvas, AutoCrop_tek_renk_goruntuyu_yok_etmez) {
    // Her şeyi kırpmak 0x0 üretirdi; kullanıcının görüntüsü silinmemeli.
    Image source;
    CHECK(source.Create(5, 5));
    source.Fill(0xFF808080u);

    Image out;
    CHECK(AutoCropImage(source, 0, out));
    CHECK_EQ(out.Width(), 5);
    CHECK_EQ(out.Height(), 5);
}

CRISP_TEST(Canvas, AutoCrop_kose_cogunlugu_referansi_belirler) {
    // Sol-üst köşede yabancı bir piksel var; diğer üç köşe beyaz. Referans
    // çoğunluktan geldiği için kırpma yine de doğru çalışmalı.
    Image source;
    CHECK(source.Create(9, 7));
    source.Fill(0xFFFFFFFFu);
    source.SetPixel(0, 0, 0xFF00FF00u);
    for (int y = 2; y < 5; ++y) {
        for (int x = 3; x < 7; ++x) {
            source.SetPixel(x, y, 0xFF0000FFu);
        }
    }

    Image out;
    CHECK(AutoCropImage(source, 0, out));
    // Sol-üstteki tek piksel de içerik sayılır: kırpma (0,0)'dan başlar.
    CHECK_EQ(out.Width(), 7);
    CHECK_EQ(out.Height(), 5);
}
