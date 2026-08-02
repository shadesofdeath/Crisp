// Stitch.cpp — bkz. Stitch.h.
#include "Stitch.h"

#include <algorithm>
#include <cstring>

namespace crisp {
namespace {

// SATIR 0 EN ÜST SATIRDIR, çevirme yok.
//
// DIB'lerin çoğu alttan üste saklanır ve bu dosya bir süre öyle varsaydı;
// sınamalar bilinen kaydırmayı bulamayınca ortaya çıktı. `Image::Create`
// biHeight'ı NEGATİF veriyor — yani top-down bir DIB — ve `Image::Pixel` de
// tamponu doğrudan `y` ile indeksliyor. Buradaki çevirme, zaten doğru duran
// bir tamponu ters çeviriyordu.
[[nodiscard]] const uint32_t* RowFromTop(const Image& image, int row) noexcept {
    const auto* base = static_cast<const uint32_t*>(image.Bits());
    return base + static_cast<size_t>(row) * static_cast<size_t>(image.Width());
}

[[nodiscard]] uint32_t* MutableRowFromTop(const Image& image, int row) noexcept {
    auto* base = static_cast<uint32_t*>(image.Bits());
    return base + static_cast<size_t>(row) * static_cast<size_t>(image.Width());
}

// Bir satır grubunun ne kadar benzediği, erken çıkışlı.
//
// ERKEN ÇIKIŞ ÖNEMLİ: kaydırma miktarını ararken yüzlerce aday deneniyor ve
// adayların neredeyse tamamı ilk birkaç satırda eleniyor. Sınırı aşan bir
// adayı sonuna kadar hesaplamak, aramayı on kat yavaşlatırdı.
[[nodiscard]] uint64_t BandDifference(const Image& a, int startA, const Image& b,
                                      int startB, int rows,
                                      uint64_t giveUpAt) noexcept {
    uint64_t total = 0;
    for (int i = 0; i < rows; ++i) {
        total += RowDifference(a, startA + i, b, startB + i);
        if (total >= giveUpAt) {
            return total;
        }
    }
    return total;
}

}  // namespace

uint64_t RowDifference(const Image& a, int rowA, const Image& b,
                       int rowB) noexcept {
    if (!a.Valid() || !b.Valid() || a.Width() != b.Width()) {
        return UINT64_MAX;
    }
    if (rowA < 0 || rowB < 0 || rowA >= a.Height() || rowB >= b.Height()) {
        return UINT64_MAX;
    }

    const uint32_t* left = RowFromTop(a, rowA);
    const uint32_t* right = RowFromTop(b, rowB);
    const int width = a.Width();

    uint64_t total = 0;
    for (int x = 0; x < width; ++x) {
        const uint32_t p = left[x];
        const uint32_t q = right[x];
        // Kanal başına mutlak fark. Alfa yok sayılıyor: yakalanan görüntülerde
        // her zaman 0xFF ve karşılaştırmaya bir şey katmıyor.
        const int dr = static_cast<int>((p >> 16) & 0xFFu) -
                       static_cast<int>((q >> 16) & 0xFFu);
        const int dg = static_cast<int>((p >> 8) & 0xFFu) -
                       static_cast<int>((q >> 8) & 0xFFu);
        const int db = static_cast<int>(p & 0xFFu) - static_cast<int>(q & 0xFFu);
        total += static_cast<uint64_t>(dr < 0 ? -dr : dr) +
                 static_cast<uint64_t>(dg < 0 ? -dg : dg) +
                 static_cast<uint64_t>(db < 0 ? -db : db);
    }
    return total;
}

int FindVerticalShift(const Image& previous, const Image& next,
                      int minOverlap) noexcept {
    if (!previous.Valid() || !next.Valid()) {
        return 0;
    }
    if (previous.Width() != next.Width() ||
        previous.Height() != next.Height()) {
        return 0;
    }

    const int height = previous.Height();

    // ŞERİT ÜSTTEN DEĞİL, ÜÇTE BİRDEN ALINIR.
    //
    // Kullanıcının seçtiği alan yalnızca kayan içerik olmayabilir: bir pencere
    // başlığı, bir araç çubuğu ya da sayfaya yapışık bir başlık üstte durur ve
    // kaydırıldıkça KIPIRDAMAZ. Şerit en üstten alındığında karşılaştırılan
    // şey o sabit bölge oluyordu; sabit bölge yalnızca kendisiyle, yani shift=0
    // ile eşleşir — ve shift=0 dışlandığı için hiçbir aday eşiği geçemiyordu.
    // Sonuç, kaydırmalı yakalamanın "hiçbir şey yakalanamadı" demesiydi, oysa
    // pencere gayet güzel kayıyordu.
    //
    // Üçte birden başlayan bir şerit, makul her başlığın altında kalıyor ve
    // yalnızca gerçekten kayan içeriği okuyor.
    const int bandTop = height / 3;
    const int overlap = (std::max)(1, (std::min)(minOverlap, height - bandTop));

    // shift = 0 anlamsız: hiç kaydırmamış bir pencere yeni bir şey göstermiyor
    // ve birleştirmenin durması gerekiyor. En büyük shift, şeridi `previous`ın
    // dışına taşırmayan değer.
    const int maxShift = height - bandTop - overlap;
    if (maxShift < 1) {
        return 0;
    }

    // ÖRTÜŞEN BÖLGENİN TAMAMI DEĞİL, BİR ŞERİDİ karşılaştırılıyor: `overlap`
    // satır, ayırt etmeye fazlasıyla yetiyor ve arama maliyetini kaydırma
    // miktarından bağımsız kılıyor.
    int best = 0;
    uint64_t bestScore = UINT64_MAX;
    for (int shift = 1; shift <= maxShift; ++shift) {
        // `next`in `shift` kadar kaydığı varsayımı: `next`in şeridi,
        // `previous`ın `shift` satır aşağısındaki aynı şeritle eşleşmeli.
        const uint64_t score = BandDifference(previous, bandTop + shift, next,
                                              bandTop, overlap, bestScore);
        if (score < bestScore) {
            bestScore = score;
            best = shift;
        }
    }

    // EŞİK: en iyi aday yeterince iyi değilse HİÇBİR şey döndürülmez.
    //
    // Her aramanın bir "en iyisi" vardır ve tamamen ilgisiz iki kare için de
    // bir sayı çıkar. O sayıyı kaydırma miktarı sanmak, alakasız iki şeridi
    // birbirine yapıştırılmış bir görüntü üretirdi — ve bu sessizce olurdu.
    // Piksel başına ortalama 12 birimlik (üç kanal toplamı) bir fark, JPEG
    // benzeri gürültüye ve alt piksel yazı yumuşatmasına yer bırakıyor ama
    // gerçekten farklı bir içeriği geçirmiyor.
    const uint64_t budget = static_cast<uint64_t>(previous.Width()) *
                            static_cast<uint64_t>(overlap) * 12u;
    if (bestScore > budget) {
        return 0;
    }
    return best;
}

bool StitchVertical(const std::vector<Image>& frames, int minOverlap, Image& out,
                    size_t* stopped) {
    if (stopped != nullptr) {
        *stopped = frames.size();
    }
    if (frames.empty() || !frames.front().Valid()) {
        return false;
    }

    const int width = frames.front().Width();
    const int height = frames.front().Height();

    // Kaydırma miktarları ÖNCE hesaplanır: toplam yüksekliği bilmeden hedef
    // görüntü ayrılamaz, ve iki kez geçmek tek geçişte büyüyen bir tampon
    // yönetmekten basit.
    std::vector<int> shifts;
    shifts.reserve(frames.size());
    size_t used = 1;
    int total = height;

    for (size_t i = 1; i < frames.size(); ++i) {
        if (!frames[i].Valid() || frames[i].Width() != width ||
            frames[i].Height() != height) {
            break;
        }
        const int shift = FindVerticalShift(frames[i - 1], frames[i], minOverlap);
        if (shift <= 0) {
            break;   // eşleşme yok: burada bitiyoruz
        }
        shifts.push_back(shift);
        total += shift;
        ++used;
    }

    if (stopped != nullptr) {
        *stopped = used;
    }
    if (!out.Create(width, total)) {
        return false;
    }

    // İlk kare bütünüyle.
    for (int y = 0; y < height; ++y) {
        std::memcpy(MutableRowFromTop(out, y), RowFromTop(frames[0], y),
                    static_cast<size_t>(width) * sizeof(uint32_t));
    }

    // Sonrakilerin YALNIZCA yeni kısmı: her karenin son `shift` satırı.
    int writtenTo = height;
    for (size_t i = 0; i < shifts.size(); ++i) {
        const Image& frame = frames[i + 1];
        const int shift = shifts[i];
        for (int row = 0; row < shift; ++row) {
            const int source = height - shift + row;
            std::memcpy(MutableRowFromTop(out, writtenTo + row),
                        RowFromTop(frame, source),
                        static_cast<size_t>(width) * sizeof(uint32_t));
        }
        writtenTo += shift;
    }
    return true;
}

}  // namespace crisp
