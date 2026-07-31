// OcrLayout.cpp — bkz. OcrLayout.h.
#include "OcrLayout.h"

#include "Geometry.h"

#include <algorithm>
#include <utility>

namespace crisp {
namespace ocrsel {
namespace {

// Dikey uzaklık bu kadar ağırlıklandırılır. Metin satırları yataya göre çok
// daha yakın aralıklı olduğundan, ağırlıksız uzaklık satırın sağ boşluğundaki
// bir imleci alt satırın ilk kelimesine bağlar ve seçim aşağı atlar.
constexpr LONG kVerticalWeight = 4;

[[nodiscard]] LONG DistanceToRect(const RECT& r, POINT p) noexcept {
    // Dikdörtgene en kısa uzaklık; içerideyse sıfır.
    const LONG dx = p.x < r.left ? r.left - p.x : (p.x > r.right ? p.x - r.right : 0);
    const LONG dy =
        p.y < r.top ? r.top - p.y : (p.y > r.bottom ? p.y - r.bottom : 0);
    return dx + dy * kVerticalWeight;
}

}  // namespace

void NormalizeReadingOrder(OcrLayout& layout) {
    if (layout.empty()) {
        return;
    }

    // MOTORUN SATIR KİMLİĞİ ATILIR ve satırlar DİKEY ÖRTÜŞMEDEN yeniden
    // kurulur.
    //
    // Sebebi ölçüldü: motor tek bir kod satırını "if", "(control &&
    // (wParam", "|| wParam" gibi parçalara bölüp her birine ayrı satır
    // kimliği veriyor. Kimlikleri koruyup yalnızca üst kenara göre sıralamak,
    // birkaç pikselle ayrılan bu parçaları rastgele bir sıraya sokuyordu ve
    // kopyalanan metin "II wParam / (controL && (wParam / if" diye çıkıyordu.
    // Ekranda aynı hizada duran her şey aynı satırdır; ölçüt bu olmalı.
    std::vector<OcrWord> words = std::move(layout.words);
    std::sort(words.begin(), words.end(),
              [](const OcrWord& a, const OcrWord& b) {
                  if (a.bounds.top != b.bounds.top) {
                      return a.bounds.top < b.bounds.top;
                  }
                  return a.bounds.left < b.bounds.left;
              });

    std::vector<OcrWord> ordered;
    ordered.reserve(words.size());

    size_t index = 0;
    int line = 0;
    while (index < words.size()) {
        std::vector<OcrWord> row;
        row.push_back(words[index]);

        // Satırın dikey ORTA noktası ve yüksekliği, satıra katılan her
        // kelimeyle güncellenir; yoksa eğik yazılmış ya da farklı punto bir
        // kelime satırı kopartırdı.
        LONG rowTop = words[index].bounds.top;
        LONG rowBottom = words[index].bounds.bottom;
        ++index;

        while (index < words.size()) {
            const RECT& next = words[index].bounds;
            const LONG centre = (next.top + next.bottom) / 2;
            // Kelimenin ortası satırın dikey aralığına düşüyorsa aynı satır.
            // Üst üste binmeyen ama bir pikselle değen kutuları birleştirmemek
            // için ölçüt "orta nokta içeride", "kenarlar değiyor" değil.
            if (centre < rowTop || centre > rowBottom) {
                break;
            }
            rowTop = next.top < rowTop ? next.top : rowTop;
            rowBottom = next.bottom > rowBottom ? next.bottom : rowBottom;
            row.push_back(words[index]);
            ++index;
        }

        std::sort(row.begin(), row.end(),
                  [](const OcrWord& a, const OcrWord& b) {
                      return a.bounds.left < b.bounds.left;
                  });
        for (OcrWord& word : row) {
            word.line = line;
            ordered.push_back(std::move(word));
        }
        ++line;
    }

    layout.words = std::move(ordered);
}

int WordAt(const OcrLayout& layout, POINT point) noexcept {
    for (int i = 0; i < layout.count(); ++i) {
        if (::PtInRect(&layout.words[static_cast<size_t>(i)].bounds, point)) {
            return i;
        }
    }
    return -1;
}

int NearestWord(const OcrLayout& layout, POINT point) noexcept {
    const int exact = WordAt(layout, point);
    if (exact >= 0) {
        return exact;
    }
    if (layout.empty()) {
        return -1;
    }

    int best = 0;
    LONG bestDistance = DistanceToRect(layout.words[0].bounds, point);
    for (int i = 1; i < layout.count(); ++i) {
        const LONG distance =
            DistanceToRect(layout.words[static_cast<size_t>(i)].bounds, point);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = i;
        }
    }
    return best;
}

void NormalizeRange(int a, int b, int& lo, int& hi) noexcept {
    if (a <= b) {
        lo = a;
        hi = b;
    } else {
        lo = b;
        hi = a;
    }
}

std::wstring TextForRange(const OcrLayout& layout, int lo, int hi) {
    std::wstring text;
    if (layout.empty()) {
        return text;
    }

    // Aralık kırpılır: çağıran -1 ya da taşan bir indeks verdiğinde çökmek
    // yerine elde olanı döndürmek doğru davranış.
    if (lo < 0) {
        lo = 0;
    }
    if (hi >= layout.count()) {
        hi = layout.count() - 1;
    }
    if (lo > hi) {
        return text;
    }

    int currentLine = layout.words[static_cast<size_t>(lo)].line;
    for (int i = lo; i <= hi; ++i) {
        const OcrWord& word = layout.words[static_cast<size_t>(i)];
        if (i > lo) {
            if (word.line != currentLine) {
                text += L"\r\n";
                currentLine = word.line;
            } else {
                text += L' ';
            }
        }
        text += word.text;
    }
    return text;
}

std::wstring AllText(const OcrLayout& layout) {
    return TextForRange(layout, 0, layout.count() - 1);
}

int LineCount(const OcrLayout& layout) noexcept {
    if (layout.empty()) {
        return 0;
    }
    // Kelimeler okuma sırasında ve satır indeksleri artan; son kelimeninki
    // en büyüğüdür.
    return layout.words.back().line + 1;
}

void LineRange(const OcrLayout& layout, int wordIndex, int& first,
               int& last) noexcept {
    first = -1;
    last = -1;
    if (wordIndex < 0 || wordIndex >= layout.count()) {
        return;
    }

    const int line = layout.words[static_cast<size_t>(wordIndex)].line;
    first = wordIndex;
    last = wordIndex;
    while (first > 0 && layout.words[static_cast<size_t>(first - 1)].line == line) {
        --first;
    }
    while (last + 1 < layout.count() &&
           layout.words[static_cast<size_t>(last + 1)].line == line) {
        ++last;
    }
}

RECT LineBounds(const OcrLayout& layout, int line) noexcept {
    RECT bounds{};
    bool found = false;

    for (const OcrWord& word : layout.words) {
        if (word.line != line) {
            continue;
        }
        if (!found) {
            bounds = word.bounds;
            found = true;
            continue;
        }
        bounds.left = word.bounds.left < bounds.left ? word.bounds.left : bounds.left;
        bounds.top = word.bounds.top < bounds.top ? word.bounds.top : bounds.top;
        bounds.right =
            word.bounds.right > bounds.right ? word.bounds.right : bounds.right;
        bounds.bottom =
            word.bounds.bottom > bounds.bottom ? word.bounds.bottom : bounds.bottom;
    }
    return bounds;
}

int LineAt(const OcrLayout& layout, POINT point) noexcept {
    const int lines = LineCount(layout);
    for (int line = 0; line < lines; ++line) {
        const RECT bounds = LineBounds(layout, line);
        if (!geom::IsEmpty(bounds) && ::PtInRect(&bounds, point)) {
            return line;
        }
    }
    return -1;
}

}  // namespace ocrsel
}  // namespace crisp
