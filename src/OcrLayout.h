// OcrLayout.h — Tanınan kelimelerin yerleşimi ve metin seçme mantığı.
//
// BU BAŞLIKTA WinRT YOKTUR. OCR motoruyla konuşan kod Ocr.cpp'de; burada
// yalnızca "kelimeler ve kutuları" var. Ayrım bilinçli: isabet testi, aralık
// normalleştirme ve seçili metnin birleştirilmesi, bir OCR motoru
// çalıştırmadan sınanabilmeli.
#pragma once

#include <string>
#include <vector>

#include <windows.h>

namespace crisp {

struct OcrWord {
    std::wstring text;
    RECT bounds{};   // görüntü koordinatı (yakalanan görüntünün pikselleri)
    int line = 0;    // ait olduğu satırın indeksi
};

struct OcrLayout {
    // OKUMA SIRASINDA: satır satır, her satırda soldan sağa. Aralık seçimi
    // bu sıraya dayanır, bu yüzden motorun verdiği sıra korunur.
    std::vector<OcrWord> words;

    [[nodiscard]] bool empty() const noexcept { return words.empty(); }
    [[nodiscard]] int count() const noexcept {
        return static_cast<int>(words.size());
    }
};

namespace ocrsel {

// Kelimeleri GÖRSEL okuma sırasına sokar: satırlar üstten alta, satır içindeki
// kelimeler soldan sağa. Satır indeksleri 0'dan başlayarak yeniden atanır.
//
// NEDEN GEREKLİ: aralık seçimi kelime sırasına dayanır. Motor tüm ekranı
// tararken satırları ekrandaki konumlarına göre değil kendi bölge analizine
// göre sıralar; birden çok pencere açıkken sıra ekranda zıplar. O hâlde
// kullanıcı iki nokta arasını seçtiğinde vurgulanan kelimeler ekranın
// dört bir yanına dağılır ve seçim anlamsız görünür.
void NormalizeReadingOrder(OcrLayout& layout);

// Noktanın ÜZERİNDE bulunduğu kelime; yoksa -1.
[[nodiscard]] int WordAt(const OcrLayout& layout, POINT point) noexcept;

// Noktaya en yakın kelime; yerleşim boşsa -1.
//
// ÖNCE AYNI SATIR: kullanıcı bir satırın sağ boşluğuna sürüklediğinde o
// satırın son kelimesini beklerse, düz Öklit uzaklığı bir alt satırın başındaki
// kelimeyi seçebilir ve seçim aşağı atlar. Dikey uzaklık bu yüzden ağırlıklıdır.
[[nodiscard]] int NearestWord(const OcrLayout& layout, POINT point) noexcept;

// Sürükleme yönü ne olursa olsun [lo, hi] artan sırada döner.
void NormalizeRange(int a, int b, int& lo, int& hi) noexcept;

// Aralıktaki kelimeleri metne çevirir. Aynı satırdakiler boşlukla, satır
// değişimi CRLF ile birleşir — pano metni Windows uygulamalarına gideceği için
// düz LF tek satır sayılırdı.
[[nodiscard]] std::wstring TextForRange(const OcrLayout& layout, int lo,
                                        int hi);

// Tüm yerleşimin metni.
[[nodiscard]] std::wstring AllText(const OcrLayout& layout);

// --- Satır işlemleri ---------------------------------------------------------

// Yerleşimdeki satır sayısı (son kelimenin satır indeksi + 1).
[[nodiscard]] int LineCount(const OcrLayout& layout) noexcept;

// Verilen kelimenin ait olduğu satırın ilk ve son kelime indeksi.
// wordIndex geçersizse first/last -1 kalır.
void LineRange(const OcrLayout& layout, int wordIndex, int& first,
               int& last) noexcept;

// Bir satırın tüm kelimelerini kapsayan dikdörtgen. Satır boşsa boş dikdörtgen.
//
// SATIR KUTUSU KELİMELERİN BİRLEŞİMİDİR, kelime kutularının ayrı ayrı
// çizilmesi değil: kullanıcı bir metin satırını tek bir blok olarak görür,
// kelime kelime kutulanmış bir satır tel kafese benzer.
[[nodiscard]] RECT LineBounds(const OcrLayout& layout, int line) noexcept;

// Noktanın üzerinde bulunduğu satır; yoksa -1. Kelimeler arası boşluklar da
// satıra dahildir — kullanıcı iki kelime arasına tıkladığında satırı seçebilmeli.
[[nodiscard]] int LineAt(const OcrLayout& layout, POINT point) noexcept;

}  // namespace ocrsel
}  // namespace crisp
