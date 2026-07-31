// TestOcrLayout.cpp — Metin seçme mantığı. OCR motoru çalıştırılmaz; yerleşim
// elle kurulur, böylece testler motorun doğruluğuna değil bizim mantığımıza
// bakar.
#include "TestFramework.h"

#include "OcrLayout.h"

using namespace crisp;
using namespace crisp::ocrsel;

namespace {

// İki satırlık örnek yerleşim:
//   satır 0:  [Fatura](10,10-70,30)  [No](80,10-110,30)
//   satır 1:  [Toplam](10,40-80,60)  [14](90,40,115,60)  [TL](125,40-150,60)
[[nodiscard]] OcrLayout SampleLayout() {
    OcrLayout layout;
    layout.words.push_back(OcrWord{L"Fatura", RECT{10, 10, 70, 30}, 0});
    layout.words.push_back(OcrWord{L"No", RECT{80, 10, 110, 30}, 0});
    layout.words.push_back(OcrWord{L"Toplam", RECT{10, 40, 80, 60}, 1});
    layout.words.push_back(OcrWord{L"14", RECT{90, 40, 115, 60}, 1});
    layout.words.push_back(OcrWord{L"TL", RECT{125, 40, 150, 60}, 1});
    return layout;
}

}  // namespace

CRISP_TEST(OcrLayout, WordAt_kelimenin_uzerinde) {
    const OcrLayout layout = SampleLayout();
    CHECK_EQ(WordAt(layout, POINT{20, 20}), 0);
    CHECK_EQ(WordAt(layout, POINT{95, 20}), 1);
    CHECK_EQ(WordAt(layout, POINT{140, 50}), 4);
}

CRISP_TEST(OcrLayout, WordAt_bosluklarda_eksi_bir) {
    const OcrLayout layout = SampleLayout();
    CHECK_EQ(WordAt(layout, POINT{75, 20}), -1);    // iki kelime arası
    CHECK_EQ(WordAt(layout, POINT{500, 500}), -1);  // çok uzak
    CHECK_EQ(WordAt(layout, POINT{20, 35}), -1);    // satırlar arası
}

CRISP_TEST(OcrLayout, WordAt_bos_yerlesim) {
    const OcrLayout empty;
    CHECK_EQ(WordAt(empty, POINT{0, 0}), -1);
    CHECK_EQ(NearestWord(empty, POINT{0, 0}), -1);
}

CRISP_TEST(OcrLayout, NearestWord_tam_isabette_ayni_sonucu_verir) {
    const OcrLayout layout = SampleLayout();
    CHECK_EQ(NearestWord(layout, POINT{20, 20}), 0);
    CHECK_EQ(NearestWord(layout, POINT{95, 20}), 1);
}

CRISP_TEST(OcrLayout, NearestWord_satirin_sag_boslugunda_ayni_satirda_kalir) {
    // KRİTİK: kullanıcı bir satırın sağ boşluğuna sürüklediğinde o satırın SON
    // kelimesini bekler. Ağırlıksız uzaklıkta alt satırın ilk kelimesi
    // kazanabilir ve seçim bir satır aşağı atlar.
    const OcrLayout layout = SampleLayout();
    // Satır 0'ın sağında, ama dikeyde satır 1'e de yakın bir nokta:
    CHECK_EQ(NearestWord(layout, POINT{200, 25}), 1);   // "No" — satır 0'ın sonu
}

CRISP_TEST(OcrLayout, NearestWord_sol_boslukta_satirin_ilk_kelimesi) {
    const OcrLayout layout = SampleLayout();
    CHECK_EQ(NearestWord(layout, POINT{0, 50}), 2);   // "Toplam"
}

CRISP_TEST(OcrLayout, NearestWord_metnin_ustunde_ilk_kelime) {
    const OcrLayout layout = SampleLayout();
    CHECK_EQ(NearestWord(layout, POINT{20, 0}), 0);
}

CRISP_TEST(OcrLayout, NormalizeRange_yonu_duzeltir) {
    int lo = 0;
    int hi = 0;
    NormalizeRange(1, 4, lo, hi);
    CHECK_EQ(lo, 1);
    CHECK_EQ(hi, 4);

    NormalizeRange(4, 1, lo, hi);
    CHECK_EQ(lo, 1);
    CHECK_EQ(hi, 4);

    NormalizeRange(3, 3, lo, hi);
    CHECK_EQ(lo, 3);
    CHECK_EQ(hi, 3);
}

CRISP_TEST(OcrLayout, TextForRange_ayni_satir_bosluklu) {
    const OcrLayout layout = SampleLayout();
    CHECK_STR(TextForRange(layout, 0, 1), L"Fatura No");
    CHECK_STR(TextForRange(layout, 2, 4), L"Toplam 14 TL");
}

CRISP_TEST(OcrLayout, TextForRange_satir_degisiminde_CRLF) {
    // Düz LF, Windows uygulamalarının çoğunda tek satır sayılır; pano metni
    // CRLF olmalı.
    const OcrLayout layout = SampleLayout();
    CHECK_STR(TextForRange(layout, 1, 2), L"No\r\nToplam");
}

CRISP_TEST(OcrLayout, TextForRange_tek_kelime) {
    const OcrLayout layout = SampleLayout();
    CHECK_STR(TextForRange(layout, 3, 3), L"14");
}

CRISP_TEST(OcrLayout, TextForRange_tasan_indeksler_kirpilir) {
    const OcrLayout layout = SampleLayout();
    CHECK_STR(TextForRange(layout, -5, 100), L"Fatura No\r\nToplam 14 TL");
    // Ters aralık boş döner, çökmez
    CHECK_STR(TextForRange(layout, 4, 1), L"");
}

CRISP_TEST(OcrLayout, TextForRange_bos_yerlesim) {
    const OcrLayout empty;
    CHECK_STR(TextForRange(empty, 0, 0), L"");
    CHECK_STR(AllText(empty), L"");
}

CRISP_TEST(OcrLayout, AllText_tumunu_verir) {
    const OcrLayout layout = SampleLayout();
    CHECK_STR(AllText(layout), L"Fatura No\r\nToplam 14 TL");
}

CRISP_TEST(OcrLayout, NormalizeReadingOrder_satirlari_ustten_alta_sirlar) {
    // Motorun verdiği sıra ekrandaki sırayla ters: alt satır önce gelmiş.
    OcrLayout layout;
    layout.words.push_back(OcrWord{L"alt", RECT{10, 100, 40, 120}, 0});
    layout.words.push_back(OcrWord{L"ust", RECT{10, 10, 40, 30}, 1});

    NormalizeReadingOrder(layout);

    CHECK_STR(layout.words[0].text, L"ust");
    CHECK_STR(layout.words[1].text, L"alt");
    CHECK_EQ(layout.words[0].line, 0);
    CHECK_EQ(layout.words[1].line, 1);
    CHECK_STR(AllText(layout), L"ust\r\nalt");
}

CRISP_TEST(OcrLayout, NormalizeReadingOrder_satir_icinde_soldan_saga) {
    OcrLayout layout;
    layout.words.push_back(OcrWord{L"uc", RECT{200, 10, 230, 30}, 0});
    layout.words.push_back(OcrWord{L"bir", RECT{10, 10, 40, 30}, 0});
    layout.words.push_back(OcrWord{L"iki", RECT{100, 10, 130, 30}, 0});

    NormalizeReadingOrder(layout);

    CHECK_STR(AllText(layout), L"bir iki uc");
}

CRISP_TEST(OcrLayout, NormalizeReadingOrder_ayni_satirdakileri_birlikte_tutar) {
    // İki satır, motorun sırası karışık. Düzeltmeden sonra her satırın
    // kelimeleri ARDIŞIK olmalı; aksi hâlde aralık seçimi satır atlar.
    OcrLayout layout;
    layout.words.push_back(OcrWord{L"b1", RECT{10, 100, 40, 120}, 5});
    layout.words.push_back(OcrWord{L"a1", RECT{10, 10, 40, 30}, 2});
    layout.words.push_back(OcrWord{L"b2", RECT{50, 100, 80, 120}, 5});
    layout.words.push_back(OcrWord{L"a2", RECT{50, 10, 80, 30}, 2});

    NormalizeReadingOrder(layout);

    CHECK_STR(AllText(layout), L"a1 a2\r\nb1 b2");
    CHECK_EQ(layout.words[0].line, 0);
    CHECK_EQ(layout.words[1].line, 0);
    CHECK_EQ(layout.words[2].line, 1);
    CHECK_EQ(layout.words[3].line, 1);
}

CRISP_TEST(OcrLayout, NormalizeReadingOrder_bos_yerlesim_guvenli) {
    OcrLayout empty;
    NormalizeReadingOrder(empty);
    CHECK(empty.empty());
}

CRISP_TEST(OcrLayout, LineCount_satir_sayisi) {
    CHECK_EQ(LineCount(SampleLayout()), 2);
    const OcrLayout empty;
    CHECK_EQ(LineCount(empty), 0);
}

CRISP_TEST(OcrLayout, LineRange_satirin_ucunu_bulur) {
    const OcrLayout layout = SampleLayout();
    int first = 0;
    int last = 0;

    // Satır 0: kelime 0..1
    LineRange(layout, 0, first, last);
    CHECK_EQ(first, 0);
    CHECK_EQ(last, 1);
    LineRange(layout, 1, first, last);
    CHECK_EQ(first, 0);
    CHECK_EQ(last, 1);

    // Satır 1: kelime 2..4 — ORTADAKİ kelimeden de aynı aralık çıkmalı
    LineRange(layout, 3, first, last);
    CHECK_EQ(first, 2);
    CHECK_EQ(last, 4);
}

CRISP_TEST(OcrLayout, LineRange_gecersiz_indeks) {
    const OcrLayout layout = SampleLayout();
    int first = 99;
    int last = 99;
    LineRange(layout, -1, first, last);
    CHECK_EQ(first, -1);
    CHECK_EQ(last, -1);
    LineRange(layout, 100, first, last);
    CHECK_EQ(first, -1);
}

CRISP_TEST(OcrLayout, LineBounds_kelimelerin_birlesimi) {
    const OcrLayout layout = SampleLayout();
    // Satır 0: (10,10)-(70,30) ∪ (80,10)-(110,30)
    CHECK_RECT(LineBounds(layout, 0), 10, 10, 110, 30);
    // Satır 1: (10,40)-(80,60) ∪ (90,40)-(115,60) ∪ (125,40)-(150,60)
    CHECK_RECT(LineBounds(layout, 1), 10, 40, 150, 60);
}

CRISP_TEST(OcrLayout, LineBounds_olmayan_satir_bos) {
    const OcrLayout layout = SampleLayout();
    const RECT bounds = LineBounds(layout, 7);
    CHECK(bounds.left == 0 && bounds.right == 0);
}

CRISP_TEST(OcrLayout, LineAt_kelimeler_arasi_bosluk_da_satira_dahil) {
    // Kullanıcı iki kelime ARASINA tıkladığında satırı seçebilmeli; kelime
    // kutularına bakan bir arama burada -1 döner ve satır seçimi çalışmaz.
    const OcrLayout layout = SampleLayout();
    CHECK_EQ(WordAt(layout, POINT{75, 20}), -1);   // kelimeler arası
    CHECK_EQ(LineAt(layout, POINT{75, 20}), 0);    // ama satır 0
}

CRISP_TEST(OcrLayout, LineAt_satir_disi) {
    const OcrLayout layout = SampleLayout();
    CHECK_EQ(LineAt(layout, POINT{20, 35}), -1);   // satırlar arası boşluk
    CHECK_EQ(LineAt(layout, POINT{500, 500}), -1);
}

CRISP_TEST(OcrLayout, Satir_atlamasi_birden_fazla_satirda) {
    OcrLayout layout;
    layout.words.push_back(OcrWord{L"bir", RECT{0, 0, 30, 20}, 0});
    layout.words.push_back(OcrWord{L"iki", RECT{0, 30, 30, 50}, 1});
    layout.words.push_back(OcrWord{L"uc", RECT{0, 60, 30, 80}, 2});
    CHECK_STR(AllText(layout), L"bir\r\niki\r\nuc");
}

// --- Motorun satır kimliği yanlış olduğunda ----------------------------------

CRISP_TEST(OcrLayout, Ayni_satirdaki_parcalar_birlesir) {
    // GERÇEK BİR HATADAN: Windows OCR tek bir kod satırını parçalara bölüp her
    // birine ayrı satır kimliği veriyor ve üst kenarları birkaç piksel
    // oynuyor. Kimliklere güvenmek, kopyalanan metni karıştırıyordu.
    OcrLayout layout;
    layout.words.push_back(OcrWord{L"wParam", RECT{300, 101, 380, 119}, 7});
    layout.words.push_back(OcrWord{L"if", RECT{100, 100, 120, 118}, 3});
    layout.words.push_back(OcrWord{L"(control", RECT{140, 102, 260, 120}, 5});

    ocrsel::NormalizeReadingOrder(layout);

    CHECK_EQ(layout.count(), 3);
    CHECK_STR(layout.words[0].text, L"if");
    CHECK_STR(layout.words[1].text, L"(control");
    CHECK_STR(layout.words[2].text, L"wParam");
    // Üçü de TEK satır sayılmalı; ayrı satırlar CRLF sokup metni bölerdi.
    CHECK_EQ(layout.words[0].line, 0);
    CHECK_EQ(layout.words[2].line, 0);
    CHECK_EQ(ocrsel::LineCount(layout), 1);
}

CRISP_TEST(OcrLayout, Ayri_satirlar_birlestirilmez) {
    OcrLayout layout;
    layout.words.push_back(OcrWord{L"alt", RECT{100, 140, 160, 158}, 0});
    layout.words.push_back(OcrWord{L"ust", RECT{100, 100, 160, 118}, 0});

    ocrsel::NormalizeReadingOrder(layout);

    CHECK_STR(layout.words[0].text, L"ust");
    CHECK_STR(layout.words[1].text, L"alt");
    CHECK_EQ(layout.words[0].line, 0);
    CHECK_EQ(layout.words[1].line, 1);
    CHECK_EQ(ocrsel::LineCount(layout), 2);
}

CRISP_TEST(OcrLayout, Parcali_satirin_metni_tek_satir_olur) {
    OcrLayout layout;
    layout.words.push_back(OcrWord{L"dunya", RECT{200, 101, 300, 119}, 9});
    layout.words.push_back(OcrWord{L"merhaba", RECT{100, 100, 190, 118}, 4});

    ocrsel::NormalizeReadingOrder(layout);
    const std::wstring text = ocrsel::AllText(layout);
    CHECK_STR(text, L"merhaba dunya");
}