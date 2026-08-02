// TestStitch.cpp — Kaydırmalı yakalamanın birleştirme yarısı.
//
// EKRANA HİÇ BAKMIYOR. Bütün girdi burada üretiliyor: bilinen bir desen,
// bilinen bir miktar kaydırılıyor, ve aynı sayı geri isteniyor. Bir hizalama
// hatasını çıplak gözle "biraz kaymış" diye fark etmek saatler alır.
#include "TestFramework.h"

#include "Stitch.h"

#include <vector>

using namespace crisp;

namespace {

// Her satırı, satır numarasından türeyen ayırt edici bir desenle dolduran
// görüntü. `offset` sayfanın ne kadar kaydırıldığı.
//
// DESEN SATIRA GÖRE DEĞİŞMELİ: her satırı aynı olan bir görüntüde "kaç piksel
// kaymış" sorusunun tek bir doğru cevabı yoktur ve sınama, gerçekte olmayan
// bir kesinliği ölçmüş olurdu.
[[nodiscard]] bool MakeFrame(int width, int height, int offset, Image& out) {
    if (!out.Create(width, height)) {
        return false;
    }
    for (int y = 0; y < height; ++y) {
        const int line = y + offset;
        for (int x = 0; x < width; ++x) {
            const uint32_t r = static_cast<uint32_t>((line * 7 + x * 3) & 0xFF);
            const uint32_t g = static_cast<uint32_t>((line * 13) & 0xFF);
            const uint32_t b = static_cast<uint32_t>((x * 5 + line * 2) & 0xFF);
            out.SetPixel(x, y, 0xFF000000u | (r << 16) | (g << 8) | b);
        }
    }
    return true;
}

// Tek renk: hiçbir satır diğerinden ayırt edilemez.
[[nodiscard]] bool MakeFlat(int width, int height, uint32_t colour, Image& out) {
    if (!out.Create(width, height)) {
        return false;
    }
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            out.SetPixel(x, y, colour);
        }
    }
    return true;
}

}  // namespace

CRISP_TEST(Stitch, Bilinen_kaydirmayi_geri_bulur) {
    // Şerit karenin üçte birinden başlıyor ve `minOverlap` satır sürüyor, yani
    // 200 satırlık bir karede en büyük bulunabilir kaydırma 200 - 66 - 40 = 94.
    // Bunun üstü "bulunamadı"dır ve öyle olmalı: o kadar hızlı kaydırılmış iki
    // karenin paylaştığı bir şerit yok.
    for (const int shift : {1, 5, 40, 94}) {
        Image a;
        Image b;
        CHECK(MakeFrame(64, 200, 0, a));
        CHECK(MakeFrame(64, 200, shift, b));
        CHECK_EQ(FindVerticalShift(a, b, 40), shift);
    }
    for (const int shift : {95, 150, 199}) {
        Image a;
        Image b;
        CHECK(MakeFrame(64, 200, 0, a));
        CHECK(MakeFrame(64, 200, shift, b));
        CHECK_EQ(FindVerticalShift(a, b, 40), 0);
    }
}

CRISP_TEST(Stitch, Ustteki_sabit_baslik_eslesmeyi_bozmaz) {
    // GERÇEK BİR HATANIN SINAMASI. Kullanıcının seçtiği alan pencere başlığını
    // ya da sayfaya yapışık bir başlığı içerdiğinde, karenin ÜST kısmı
    // kaydırıldıkça değişmiyor. Şerit en üstten alındığı sürümde bu, hiçbir
    // adayın eşleşmemesine ve "hiçbir şey yakalanamadı" iletisine yol
    // açıyordu — pencere kayıyor olmasına rağmen.
    const int width = 64;
    const int height = 300;
    const int header = 90;
    const int shift = 45;

    Image a;
    Image b;
    CHECK(MakeFrame(width, height, 0, a));
    CHECK(MakeFrame(width, height, shift, b));

    // İki karenin de üst `header` satırını AYNI sabit içerikle ez.
    for (int y = 0; y < header; ++y) {
        for (int x = 0; x < width; ++x) {
            const uint32_t colour =
                0xFF000000u | static_cast<uint32_t>((y * 3 + x) & 0xFF);
            a.SetPixel(x, y, colour);
            b.SetPixel(x, y, colour);
        }
    }

    CHECK_EQ(FindVerticalShift(a, b, 40), shift);
}

CRISP_TEST(Stitch, Ayni_kare_kaydirma_bulmaz) {
    // Hiç kaydırılmamış pencere yeni bir şey göstermiyor; 0 dönmeli ki
    // birleştirme dursun. Aksi hâlde aynı şerit tekrar tekrar eklenirdi.
    Image a;
    Image b;
    CHECK(MakeFrame(64, 200, 0, a));
    CHECK(MakeFrame(64, 200, 0, b));
    CHECK_EQ(FindVerticalShift(a, b, 40), 0);
}

CRISP_TEST(Stitch, Alakasiz_kareler_uydurmaz) {
    // İKİ İLGİSİZ KARE İÇİN DE BİR "EN İYİ" ADAY VARDIR. Eşik olmasaydı o
    // aday kaydırma miktarı sanılır ve birbirine yapıştırılmış iki alakasız
    // şerit üretilirdi — sessizce, hata vermeden.
    Image a;
    Image b;
    CHECK(MakeFrame(64, 200, 0, a));
    CHECK(MakeFlat(64, 200, 0xFF102030u, b));
    CHECK_EQ(FindVerticalShift(a, b, 40), 0);
}

CRISP_TEST(Stitch, Farkli_olculer_kaydirma_bulmaz) {
    Image a;
    Image b;
    CHECK(MakeFrame(64, 200, 0, a));
    CHECK(MakeFrame(80, 200, 20, b));
    CHECK_EQ(FindVerticalShift(a, b, 40), 0);

    Image c;
    CHECK(MakeFrame(64, 150, 20, c));
    CHECK_EQ(FindVerticalShift(a, c, 40), 0);
}

CRISP_TEST(Stitch, Bes_kare_tek_uzun_goruntu_olur) {
    // Kare boyu gerçekçi tutuluyor: şerit karenin üçte birinden başladığı için
    // yüz satırlık bir karede bulunabilir en büyük kaydırma yirmi yediye
    // düşüyor ve sınama, ölçtüğü şeyi değil kendi seçtiği sayıları sınamış
    // olurdu.
    const int width = 48;
    const int height = 200;
    const int shift = 40;

    std::vector<Image> frames;
    for (int i = 0; i < 5; ++i) {
        Image frame;
        CHECK(MakeFrame(width, height, i * shift, frame));
        frames.push_back(std::move(frame));
    }

    Image out;
    size_t used = 0;
    CHECK(StitchVertical(frames, 40, out, &used));
    CHECK_EQ(used, static_cast<size_t>(5));
    CHECK_EQ(out.Width(), width);
    CHECK_EQ(out.Height(), height + shift * 4);

    // BİRLEŞTİRİLEN GÖRÜNTÜ, KESİNTİSİZ SAYFANIN KENDİSİ OLMALI. Her satır,
    // hiç kaydırılmamış tek bir uzun kareden alınmış gibi olmalı; bir piksel
    // kayma bile burada yakalanır.
    Image whole;
    CHECK(MakeFrame(width, out.Height(), 0, whole));
    for (int y = 0; y < out.Height(); ++y) {
        CHECK_EQ(RowDifference(out, y, whole, y), static_cast<uint64_t>(0));
    }
}

CRISP_TEST(Stitch, Eslesmeyen_karede_durur_ve_soyler) {
    const int width = 48;
    const int height = 200;

    std::vector<Image> frames;
    Image first;
    Image second;
    Image stranger;
    CHECK(MakeFrame(width, height, 0, first));
    CHECK(MakeFrame(width, height, 40, second));
    CHECK(MakeFlat(width, height, 0xFF884422u, stranger));
    frames.push_back(std::move(first));
    frames.push_back(std::move(second));
    frames.push_back(std::move(stranger));

    Image out;
    size_t used = 0;
    CHECK(StitchVertical(frames, 40, out, &used));

    // Üçüncü kare eklenmedi ve bu SÖYLENDİ.
    CHECK_EQ(used, static_cast<size_t>(2));
    CHECK_EQ(out.Height(), height + 40);
}

CRISP_TEST(Stitch, Tek_kare_kendisidir) {
    std::vector<Image> frames;
    Image only;
    CHECK(MakeFrame(32, 60, 0, only));
    frames.push_back(std::move(only));

    Image out;
    CHECK(StitchVertical(frames, 20, out, nullptr));
    CHECK_EQ(out.Height(), 60);
    CHECK_EQ(out.Width(), 32);
}

CRISP_TEST(Stitch, Bos_liste_basarisiz) {
    const std::vector<Image> frames;
    Image out;
    CHECK(!StitchVertical(frames, 20, out, nullptr));
}

CRISP_TEST(Stitch, Satir_farki_ayni_satirda_sifir) {
    Image a;
    CHECK(MakeFrame(32, 40, 0, a));
    CHECK_EQ(RowDifference(a, 5, a, 5), static_cast<uint64_t>(0));
    CHECK(RowDifference(a, 5, a, 6) > 0);
    // Sınır dışı okuma sessizce 0 dönmemeli: 0 "birebir aynı" demek.
    CHECK_EQ(RowDifference(a, -1, a, 0), UINT64_MAX);
    CHECK_EQ(RowDifference(a, 0, a, 40), UINT64_MAX);
}
