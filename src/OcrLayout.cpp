// OcrLayout.cpp — bkz. OcrLayout.h.
#include "OcrLayout.h"

#include "Geometry.h"

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

}  // namespace ocrsel
}  // namespace crisp
