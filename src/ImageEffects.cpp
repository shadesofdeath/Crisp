// ImageEffects.cpp — bkz. ImageEffects.h.
#include "ImageEffects.h"

#include "Geometry.h"

#include <vector>

namespace crisp {
namespace {

// Alanı görüntünün içine hapseder ve boşsa false döner.
[[nodiscard]] bool ClipArea(const Image& image, const RECT& area, RECT& out) noexcept {
    if (!image.Valid()) {
        return false;
    }
    out = geom::ClampTo(area, RECT{0, 0, image.Width(), image.Height()});
    return !geom::IsEmpty(out);
}

// Tek eksende kutu bulanıklığı. Yatay ve dikey geçiş aynı koddan geçer;
// `horizontal` hangi eksende kayılacağını söyler.
//
// KAYAN TOPLAM: her piksel için pencereyi baştan toplamak O(n·r) olurdu ve
// 40 piksellik bir yarıçapta gözle görülür şekilde yavaşlar. Pencereden çıkanı
// çıkarıp gireni eklemek onu O(n) yapar.
void BlurAxis(const Image& source, Image& target, const RECT& area, int radius,
              bool horizontal) {
    const int width = geom::Width(area);
    const int height = geom::Height(area);
    if (width <= 0 || height <= 0) {
        return;
    }

    const int outerCount = horizontal ? height : width;
    const int innerCount = horizontal ? width : height;

    for (int outer = 0; outer < outerCount; ++outer) {
        int sumB = 0;
        int sumG = 0;
        int sumR = 0;
        int count = 0;

        auto pixelAt = [&](int inner) -> uint32_t {
            const int x = horizontal ? area.left + inner : area.left + outer;
            const int y = horizontal ? area.top + outer : area.top + inner;
            return source.Pixel(x, y);
        };

        // Başlangıç penceresi: [0, radius]
        for (int i = 0; i <= radius && i < innerCount; ++i) {
            const uint32_t p = pixelAt(i);
            sumR += static_cast<int>((p >> 16) & 0xFFu);
            sumG += static_cast<int>((p >> 8) & 0xFFu);
            sumB += static_cast<int>(p & 0xFFu);
            ++count;
        }

        for (int inner = 0; inner < innerCount; ++inner) {
            const int x = horizontal ? area.left + inner : area.left + outer;
            const int y = horizontal ? area.top + outer : area.top + inner;

            // Alfa KORUNUR: yakalamalar opaktır ama kullanıcı saydam bir
            // görüntüyü de düzenleyebilir ve bulanıklık şeffaflığı yemez.
            const uint32_t alpha = source.Pixel(x, y) & 0xFF000000u;
            target.SetPixel(x, y,
                            alpha |
                                (static_cast<uint32_t>(sumR / count) << 16) |
                                (static_cast<uint32_t>(sumG / count) << 8) |
                                static_cast<uint32_t>(sumB / count));

            // Pencereyi bir ileri kaydır.
            const int leaving = inner - radius;
            if (leaving >= 0) {
                const uint32_t p = pixelAt(leaving);
                sumR -= static_cast<int>((p >> 16) & 0xFFu);
                sumG -= static_cast<int>((p >> 8) & 0xFFu);
                sumB -= static_cast<int>(p & 0xFFu);
                --count;
            }
            const int entering = inner + radius + 1;
            if (entering < innerCount) {
                const uint32_t p = pixelAt(entering);
                sumR += static_cast<int>((p >> 16) & 0xFFu);
                sumG += static_cast<int>((p >> 8) & 0xFFu);
                sumB += static_cast<int>(p & 0xFFu);
                ++count;
            }
        }
    }
}

}  // namespace

void BlurRegion(Image& image, const RECT& area, int radius) {
    RECT clipped{};
    if (radius <= 0 || !ClipArea(image, area, clipped)) {
        return;
    }

    // İKİ GEÇİŞ AYRI TAMPON İSTER: yatay geçişin çıktısını kaynağın üstüne
    // yazarsak dikey geçiş kısmen bulanıklaştırılmış pikselleri okur ve sonuç
    // köşegen bir bulaşmaya döner.
    Image scratch;
    if (!CropImage(image, 0, 0, image.Width(), image.Height(), scratch)) {
        return;
    }

    BlurAxis(image, scratch, clipped, radius, true);
    BlurAxis(scratch, image, clipped, radius, false);
}

void MosaicRegion(Image& image, const RECT& area, int block) {
    RECT clipped{};
    if (block < 2 || !ClipArea(image, area, clipped)) {
        return;
    }

    for (int y = clipped.top; y < clipped.bottom; y += block) {
        for (int x = clipped.left; x < clipped.right; x += block) {
            const int right = (x + block < clipped.right) ? x + block : clipped.right;
            const int bottom =
                (y + block < clipped.bottom) ? y + block : clipped.bottom;

            unsigned sumR = 0;
            unsigned sumG = 0;
            unsigned sumB = 0;
            unsigned count = 0;
            for (int py = y; py < bottom; ++py) {
                for (int px = x; px < right; ++px) {
                    const uint32_t p = image.Pixel(px, py);
                    sumR += (p >> 16) & 0xFFu;
                    sumG += (p >> 8) & 0xFFu;
                    sumB += p & 0xFFu;
                    ++count;
                }
            }
            if (count == 0) {
                continue;
            }

            const uint32_t r = sumR / count;
            const uint32_t g = sumG / count;
            const uint32_t b = sumB / count;

            for (int py = y; py < bottom; ++py) {
                for (int px = x; px < right; ++px) {
                    const uint32_t alpha = image.Pixel(px, py) & 0xFF000000u;
                    image.SetPixel(px, py, alpha | (r << 16) | (g << 8) | b);
                }
            }
        }
    }
}

}  // namespace crisp
