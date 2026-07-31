// TestAnnotation.cpp — İşaretleme belgesi ve piksel efektleri.
#include "TestFramework.h"

#include "Annotation.h"
#include "ImageEffects.h"

using namespace crisp;

namespace {

[[nodiscard]] Shape MakeShape(ToolKind kind, int x1, int y1, int x2, int y2) {
    Shape shape;
    shape.kind = kind;
    shape.start = POINT{x1, y1};
    shape.end = POINT{x2, y2};
    return shape;
}

}  // namespace

// ---------------------------------------------------------------------------
// Araç sınıflandırması
// ---------------------------------------------------------------------------

CRISP_TEST(Annotation, ToolUsesDrag_metin_ve_adim_tek_tik) {
    CHECK(ToolUsesDrag(ToolKind::Arrow));
    CHECK(ToolUsesDrag(ToolKind::Rectangle));
    CHECK(ToolUsesDrag(ToolKind::Blur));
    CHECK(!ToolUsesDrag(ToolKind::Text));
    CHECK(!ToolUsesDrag(ToolKind::StepNumber));
    CHECK(!ToolUsesDrag(ToolKind::Select));
}

CRISP_TEST(Annotation, ToolIsFreehand) {
    CHECK(ToolIsFreehand(ToolKind::Pen));
    CHECK(ToolIsFreehand(ToolKind::Highlighter));
    CHECK(!ToolIsFreehand(ToolKind::Arrow));
}

CRISP_TEST(Annotation, ToolIsEffect) {
    CHECK(ToolIsEffect(ToolKind::Blur));
    CHECK(ToolIsEffect(ToolKind::Mosaic));
    CHECK(!ToolIsEffect(ToolKind::Rectangle));
}

// ---------------------------------------------------------------------------
// Şekil sınırları
// ---------------------------------------------------------------------------

CRISP_TEST(Annotation, Bounds_surukleme_yonunden_bagimsiz) {
    CHECK_RECT(MakeShape(ToolKind::Rectangle, 10, 20, 110, 220).Bounds(), 10, 20,
               110, 220);
    // Ters yönde sürükleme aynı dikdörtgeni vermeli
    CHECK_RECT(MakeShape(ToolKind::Rectangle, 110, 220, 10, 20).Bounds(), 10, 20,
               110, 220);
}

CRISP_TEST(Annotation, Bounds_serbest_cizimde_tum_noktalari_kapsar) {
    // KRİTİK: start/end yalnızca ilk ve son noktadır. Arada dışarı taşan bir
    // kavis varsa start/end'e bakan bir sınır onu kırpar ve silme/seçme
    // yanlış alanı hedefler.
    Shape shape;
    shape.kind = ToolKind::Pen;
    shape.start = POINT{100, 100};
    shape.end = POINT{110, 100};
    shape.points = {POINT{100, 100}, POINT{150, 40}, POINT{110, 100}};

    CHECK_RECT(shape.Bounds(), 100, 40, 150, 100);
}

CRISP_TEST(Annotation, Bounds_tek_noktali_serbest_cizim) {
    Shape shape;
    shape.kind = ToolKind::Pen;
    shape.points = {POINT{50, 60}};
    CHECK_RECT(shape.Bounds(), 50, 60, 50, 60);
}

// ---------------------------------------------------------------------------
// Belge ve geçmiş
// ---------------------------------------------------------------------------

CRISP_TEST(Document, Bos_belgede_geri_al_yok) {
    Document doc;
    CHECK(!doc.CanUndo());
    CHECK(!doc.CanRedo());
    CHECK(!doc.Undo());
    CHECK(!doc.Redo());
    CHECK(doc.empty());
}

CRISP_TEST(Document, Ekle_ve_geri_al) {
    Document doc;
    doc.AddShape(MakeShape(ToolKind::Arrow, 0, 0, 10, 10));
    CHECK_EQ(doc.Shapes().size(), 1u);
    CHECK(doc.CanUndo());

    CHECK(doc.Undo());
    CHECK_EQ(doc.Shapes().size(), 0u);
    CHECK(doc.CanRedo());

    CHECK(doc.Redo());
    CHECK_EQ(doc.Shapes().size(), 1u);
}

CRISP_TEST(Document, Geri_al_tek_adim_atar) {
    // "Geri al iki şekli birden siliyor" tipik bir yığın hatasıdır.
    Document doc;
    doc.AddShape(MakeShape(ToolKind::Arrow, 0, 0, 10, 10));
    doc.AddShape(MakeShape(ToolKind::Rectangle, 20, 20, 30, 30));
    doc.AddShape(MakeShape(ToolKind::Ellipse, 40, 40, 50, 50));
    CHECK_EQ(doc.Shapes().size(), 3u);

    CHECK(doc.Undo());
    CHECK_EQ(doc.Shapes().size(), 2u);
    CHECK(doc.Undo());
    CHECK_EQ(doc.Shapes().size(), 1u);
    CHECK(doc.Undo());
    CHECK_EQ(doc.Shapes().size(), 0u);
    CHECK(!doc.Undo());
}

CRISP_TEST(Document, Yeni_duzenleme_yineleme_zincirini_keser) {
    // Kullanıcı geri alıp başka bir şey çizdiyse eski ileri yol artık
    // ulaşılamaz bir daldır; yineleme onu geri getirmemeli.
    Document doc;
    doc.AddShape(MakeShape(ToolKind::Arrow, 0, 0, 10, 10));
    doc.AddShape(MakeShape(ToolKind::Rectangle, 20, 20, 30, 30));
    CHECK(doc.Undo());
    CHECK(doc.CanRedo());

    doc.AddShape(MakeShape(ToolKind::Ellipse, 40, 40, 50, 50));
    CHECK(!doc.CanRedo());

    CHECK_EQ(doc.Shapes().size(), 2u);
    CHECK(doc.Shapes()[1].kind == ToolKind::Ellipse);
}

CRISP_TEST(Document, Adim_numaralari_artar) {
    Document doc;
    CHECK_EQ(doc.NextStepNumber(), 1);

    doc.AddShape(MakeShape(ToolKind::StepNumber, 10, 10, 10, 10));
    doc.AddShape(MakeShape(ToolKind::StepNumber, 20, 20, 20, 20));
    doc.AddShape(MakeShape(ToolKind::StepNumber, 30, 30, 30, 30));

    CHECK_EQ(doc.Shapes()[0].stepNumber, 1);
    CHECK_EQ(doc.Shapes()[1].stepNumber, 2);
    CHECK_EQ(doc.Shapes()[2].stepNumber, 3);
    CHECK_EQ(doc.NextStepNumber(), 4);
}

CRISP_TEST(Document, Adim_numarasi_geri_alinca_geri_doner) {
    // Sayaç durumun parçası olmasaydı, geri alıp yeniden çizmek 1,2,3 yerine
    // 1,2,4 üretirdi.
    Document doc;
    doc.AddShape(MakeShape(ToolKind::StepNumber, 10, 10, 10, 10));
    doc.AddShape(MakeShape(ToolKind::StepNumber, 20, 20, 20, 20));
    CHECK_EQ(doc.NextStepNumber(), 3);

    CHECK(doc.Undo());
    CHECK_EQ(doc.NextStepNumber(), 2);

    doc.AddShape(MakeShape(ToolKind::StepNumber, 30, 30, 30, 30));
    CHECK_EQ(doc.Shapes()[1].stepNumber, 2);
}

CRISP_TEST(Document, Temizle_geri_alinabilir) {
    Document doc;
    doc.AddShape(MakeShape(ToolKind::Arrow, 0, 0, 10, 10));
    doc.AddShape(MakeShape(ToolKind::Arrow, 5, 5, 15, 15));
    doc.Clear();
    CHECK(doc.empty());

    CHECK(doc.Undo());
    CHECK_EQ(doc.Shapes().size(), 2u);
}

CRISP_TEST(Document, Bos_belgeyi_temizlemek_gecmise_adim_eklemez) {
    Document doc;
    doc.Clear();
    CHECK(!doc.CanUndo());
}

CRISP_TEST(Document, Gecmis_sinirli) {
    Document doc;
    // Sınırın iki katı kadar şekil ekle; geri alma sınırı aşmamalı ve
    // uygulama fark edilmeden büyümemeli.
    for (size_t i = 0; i < Document::kMaxHistory * 2; ++i) {
        doc.AddShape(MakeShape(ToolKind::Arrow, 0, 0, 10, 10));
    }
    int steps = 0;
    while (doc.Undo()) {
        ++steps;
    }
    CHECK_EQ(steps, static_cast<int>(Document::kMaxHistory));
}

// ---------------------------------------------------------------------------
// Piksel efektleri
// ---------------------------------------------------------------------------

namespace {

// Yarısı siyah yarısı beyaz keskin bir kenar: bulanıklık kenarı yumuşatmalı.
void PaintHalves(Image& image) {
    for (int y = 0; y < image.Height(); ++y) {
        for (int x = 0; x < image.Width(); ++x) {
            image.SetPixel(x, y,
                           x < image.Width() / 2 ? 0xFF000000u : 0xFFFFFFFFu);
        }
    }
}

}  // namespace

CRISP_TEST(Effects, Blur_keskin_kenari_yumusatir) {
    Image image;
    CHECK(image.Create(40, 20));
    PaintHalves(image);

    const uint32_t before = image.Pixel(19, 10);   // kenarın hemen solu: siyah
    CHECK_EQ(before & 0xFFu, 0u);

    BlurRegion(image, RECT{0, 0, 40, 20}, 6);

    // Kenardaki piksel artık ne saf siyah ne saf beyaz olmalı.
    const uint32_t after = image.Pixel(19, 10) & 0xFFu;
    CHECK(after > 0u);
    CHECK(after < 255u);
}

CRISP_TEST(Effects, Blur_alfayi_korur) {
    Image image;
    CHECK(image.Create(16, 16));
    image.Fill(0x80FF0000u);   // yarı saydam kırmızı
    BlurRegion(image, RECT{0, 0, 16, 16}, 3);
    CHECK_EQ((image.Pixel(8, 8) >> 24), 0x80u);
}

CRISP_TEST(Effects, Blur_alan_disina_dokunmaz) {
    Image image;
    CHECK(image.Create(40, 20));
    PaintHalves(image);

    // Yalnızca sol yarıyı bulanıklaştır; sağ kenar bozulmamalı.
    BlurRegion(image, RECT{0, 0, 10, 20}, 4);
    CHECK_EQ(image.Pixel(39, 10), 0xFFFFFFFFu);
}

CRISP_TEST(Effects, Blur_gecersiz_girdi_guvenli) {
    Image image;
    CHECK(image.Create(8, 8));
    image.Fill(0xFF123456u);

    BlurRegion(image, RECT{0, 0, 8, 8}, 0);      // yarıçap yok
    CHECK_EQ(image.Pixel(4, 4), 0xFF123456u);

    BlurRegion(image, RECT{100, 100, 200, 200}, 3);   // tamamen dışarıda
    CHECK_EQ(image.Pixel(4, 4), 0xFF123456u);

    Image empty;
    BlurRegion(empty, RECT{0, 0, 4, 4}, 3);   // çökmemeli
}

CRISP_TEST(Effects, Mosaic_karoyu_tek_renge_indirir) {
    Image image;
    CHECK(image.Create(16, 16));
    PaintHalves(image);

    MosaicRegion(image, RECT{0, 0, 16, 16}, 8);

    // İlk 8x8 karonun tamamı aynı renk olmalı.
    const uint32_t corner = image.Pixel(0, 0);
    bool uniform = true;
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            if (image.Pixel(x, y) != corner) {
                uniform = false;
            }
        }
    }
    CHECK(uniform);
}

CRISP_TEST(Effects, Mosaic_karo_rengi_kendi_ortalamasi) {
    // Karonun yarısı siyah yarısı beyazsa ortalama ortada olmalı; tek bir
    // pikseli örnekleyip yaymak uçlardan birini verirdi.
    Image image;
    CHECK(image.Create(8, 8));
    PaintHalves(image);   // sol 4 sütun siyah, sağ 4 beyaz

    MosaicRegion(image, RECT{0, 0, 8, 8}, 8);
    const uint32_t blue = image.Pixel(0, 0) & 0xFFu;
    CHECK(blue > 100u);
    CHECK(blue < 155u);
}

CRISP_TEST(Effects, Mosaic_tasan_karo_kirpilir) {
    // 10 genişlik, 4'lük karo: son karo 2 piksel. Taşma okuması olmamalı.
    Image image;
    CHECK(image.Create(10, 10));
    image.Fill(0xFF808080u);
    MosaicRegion(image, RECT{0, 0, 10, 10}, 4);
    CHECK_EQ(image.Pixel(9, 9), 0xFF808080u);
}

CRISP_TEST(Effects, Mosaic_gecersiz_girdi_guvenli) {
    Image image;
    CHECK(image.Create(8, 8));
    image.Fill(0xFF123456u);

    MosaicRegion(image, RECT{0, 0, 8, 8}, 1);    // karo çok küçük
    CHECK_EQ(image.Pixel(4, 4), 0xFF123456u);

    MosaicRegion(image, RECT{50, 50, 60, 60}, 4);   // dışarıda
    CHECK_EQ(image.Pixel(4, 4), 0xFF123456u);
}
