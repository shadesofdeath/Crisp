// ImageCanvas.cpp — Aynalama, tuval boyutlandırma ve otomatik kırpma.
//
// AYRI DOSYA: ölçekleme ve döndürmeyle birlikte ImageTransform.cpp ev
// kuralının 400 satır sınırını aşıyordu (docs §9). Ayrım işlevsel:
// ImageTransform.cpp pikselleri YENİDEN ÖRNEKLER, burası onları YERİNDEN
// OYNATIR — biri süzgeç matematiği, diğeri kopyalama.
#include "ImageTransform.h"

#include <cstring>

namespace crisp {
namespace {

[[nodiscard]] int ChannelDistance(uint32_t a, uint32_t b) noexcept {
    int worst = 0;
    for (int shift = 0; shift <= 24; shift += 8) {
        const int lhs = static_cast<int>((a >> shift) & 0xFFu);
        const int rhs = static_cast<int>((b >> shift) & 0xFFu);
        const int difference = lhs > rhs ? lhs - rhs : rhs - lhs;
        if (difference > worst) {
            worst = difference;
        }
    }
    return worst;
}

[[nodiscard]] bool RowMatches(const Image& image, int y, uint32_t reference,
                              int tolerance) noexcept {
    for (int x = 0; x < image.Width(); ++x) {
        if (ChannelDistance(image.Pixel(x, y), reference) > tolerance) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool ColumnMatches(const Image& image, int x, uint32_t reference,
                                 int tolerance, int top, int bottom) noexcept {
    for (int y = top; y < bottom; ++y) {
        if (ChannelDistance(image.Pixel(x, y), reference) > tolerance) {
            return false;
        }
    }
    return true;
}

}  // namespace

bool FlipImage(const Image& source, FlipAxis axis, Image& out) {
    if (!source.Valid() || &source == &out) {
        return false;
    }
    const int width = source.Width();
    const int height = source.Height();
    if (!out.Create(width, height)) {
        return false;
    }

    const auto* from = static_cast<const uint8_t*>(source.Bits());
    auto* to = static_cast<uint8_t*>(out.Bits());
    if (from == nullptr || to == nullptr) {
        return false;
    }

    if (axis == FlipAxis::Vertical) {
        // SATIRLAR TERS SIRAYLA, içerikleri olduğu gibi: satır başına tek bir
        // memcpy, piksel piksel kopyalamaktan belirgin şekilde hızlı.
        const size_t rowBytes = static_cast<size_t>(width) * 4u;
        for (int y = 0; y < height; ++y) {
            ::memcpy(to + static_cast<size_t>(y) * out.Stride(),
                     from + static_cast<size_t>(height - 1 - y) * source.Stride(),
                     rowBytes);
        }
        return true;
    }

    for (int y = 0; y < height; ++y) {
        const auto* sourceRow = reinterpret_cast<const uint32_t*>(
            from + static_cast<size_t>(y) * source.Stride());
        auto* destinationRow = reinterpret_cast<uint32_t*>(
            to + static_cast<size_t>(y) * out.Stride());
        for (int x = 0; x < width; ++x) {
            destinationRow[width - 1 - x] = sourceRow[x];
        }
    }
    return true;
}

bool ResizeCanvas(const Image& source, int width, int height, int offsetX,
                  int offsetY, uint32_t fill, Image& out) {
    if (!source.Valid() || width <= 0 || height <= 0 || &source == &out) {
        return false;
    }
    if (!out.Create(width, height)) {
        return false;
    }
    out.Fill(fill);

    // ÖRTÜŞEN DİKDÖRTGEN HESAPLANIR, kopyalama sırasında sınır denetimi
    // yapılmaz: negatif bir offset (görüntüyü kırpan tuval) ile piksel başına
    // dört karşılaştırma, büyük bir görüntüde ölçülebilir maliyet.
    const int left = offsetX < 0 ? -offsetX : 0;
    const int top = offsetY < 0 ? -offsetY : 0;
    const int right = (offsetX + source.Width() > width)
                          ? width - offsetX
                          : source.Width();
    const int bottom = (offsetY + source.Height() > height)
                           ? height - offsetY
                           : source.Height();
    if (right <= left || bottom <= top) {
        return true;   // görüntü tuvalin tamamen dışında; boş tuval geçerli
    }

    const auto* from = static_cast<const uint8_t*>(source.Bits());
    auto* to = static_cast<uint8_t*>(out.Bits());
    if (from == nullptr || to == nullptr) {
        return false;
    }
    const size_t rowBytes = static_cast<size_t>(right - left) * 4u;

    for (int y = top; y < bottom; ++y) {
        ::memcpy(to + static_cast<size_t>(offsetY + y) * out.Stride() +
                     static_cast<size_t>(offsetX + left) * 4u,
                 from + static_cast<size_t>(y) * source.Stride() +
                     static_cast<size_t>(left) * 4u,
                 rowBytes);
    }
    return true;
}

bool AutoCropImage(const Image& source, int tolerance, Image& out) {
    if (!source.Valid() || &source == &out) {
        return false;
    }
    if (tolerance < 0) {
        tolerance = 0;
    }

    const int width = source.Width();
    const int height = source.Height();

    // Referans DÖRT KÖŞENİN ÇOĞUNLUĞU. Tek bir köşeye bakmak, o köşede bir
    // simge duran bir ekran görüntüsünde hiçbir şey kırpmamak demekti.
    const uint32_t corners[4] = {
        source.Pixel(0, 0), source.Pixel(width - 1, 0),
        source.Pixel(0, height - 1), source.Pixel(width - 1, height - 1)};
    uint32_t reference = corners[0];
    int best = 0;
    for (int i = 0; i < 4; ++i) {
        int votes = 0;
        for (int j = 0; j < 4; ++j) {
            if (ChannelDistance(corners[i], corners[j]) <= tolerance) {
                ++votes;
            }
        }
        if (votes > best) {
            best = votes;
            reference = corners[i];
        }
    }

    int top = 0;
    while (top < height && RowMatches(source, top, reference, tolerance)) {
        ++top;
    }
    if (top == height) {
        // Görüntünün tamamı tek renk: kırpacak bir şey yok ve 0×0 üretmek
        // kullanıcının görüntüsünü silmek olurdu.
        return CropImage(source, 0, 0, width, height, out);
    }

    int bottom = height;
    while (bottom > top && RowMatches(source, bottom - 1, reference, tolerance)) {
        --bottom;
    }

    int left = 0;
    while (left < width &&
           ColumnMatches(source, left, reference, tolerance, top, bottom)) {
        ++left;
    }
    int right = width;
    while (right > left &&
           ColumnMatches(source, right - 1, reference, tolerance, top, bottom)) {
        --right;
    }

    return CropImage(source, left, top, right - left, bottom - top, out);
}

}  // namespace crisp
