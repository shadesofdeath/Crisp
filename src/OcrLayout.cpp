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

    // Satırları temsil eden anahtar: üst kenar, eşitlikte sol kenar. Motorun
    // verdiği satır kimliği korunur ki aynı satırın kelimeleri birlikte kalsın.
    struct LineKey {
        int originalLine;
        LONG top;
        LONG left;
    };

    std::vector<LineKey> keys;
    for (const OcrWord& word : layout.words) {
        auto it = keys.begin();
        for (; it != keys.end(); ++it) {
            if (it->originalLine == word.line) {
                break;
            }
        }
        if (it == keys.end()) {
            keys.push_back(LineKey{word.line, word.bounds.top, word.bounds.left});
        } else {
            it->top = word.bounds.top < it->top ? word.bounds.top : it->top;
            it->left = word.bounds.left < it->left ? word.bounds.left : it->left;
        }
    }

    std::sort(keys.begin(), keys.end(), [](const LineKey& a, const LineKey& b) {
        if (a.top != b.top) {
            return a.top < b.top;
        }
        return a.left < b.left;
    });

    // Eski satır kimliği → yeni sıra numarası.
    std::vector<OcrWord> ordered;
    ordered.reserve(layout.words.size());
    for (size_t newLine = 0; newLine < keys.size(); ++newLine) {
        std::vector<OcrWord> lineWords;
        for (const OcrWord& word : layout.words) {
            if (word.line == keys[newLine].originalLine) {
                lineWords.push_back(word);
            }
        }
        std::sort(lineWords.begin(), lineWords.end(),
                  [](const OcrWord& a, const OcrWord& b) {
                      return a.bounds.left < b.bounds.left;
                  });
        for (OcrWord& word : lineWords) {
            word.line = static_cast<int>(newLine);
            ordered.push_back(std::move(word));
        }
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
