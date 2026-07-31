// ImageTransform.cpp — bkz. ImageTransform.h.
#include "ImageTransform.h"

namespace crisp {
namespace {

// Negatif ve büyük değerleri 0..3 aralığına indirir. C++'ta negatif modülo
// negatif kalır, bu yüzden düz % yetmez.
[[nodiscard]] int NormalizeQuarters(int quarterTurns) noexcept {
    int turns = quarterTurns % 4;
    if (turns < 0) {
        turns += 4;
    }
    return turns;
}

}  // namespace

bool RotateImage(const Image& source, int quarterTurns, Image& out) {
    if (!source.Valid() || &source == &out) {
        return false;
    }

    const int turns = NormalizeQuarters(quarterTurns);
    const int width = source.Width();
    const int height = source.Height();

    if (turns == 0) {
        return CropImage(source, 0, 0, width, height, out);
    }

    // 90 ve 270'te kenarlar YER DEĞİŞTİRİR; 180'de aynı kalır.
    const bool swapped = (turns == 1 || turns == 3);
    if (!out.Create(swapped ? height : width, swapped ? width : height)) {
        return false;
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const uint32_t pixel = source.Pixel(x, y);
            switch (turns) {
                case 1:   // saat yönünde 90°
                    out.SetPixel(height - 1 - y, x, pixel);
                    break;
                case 2:   // 180°
                    out.SetPixel(width - 1 - x, height - 1 - y, pixel);
                    break;
                default:  // 270° (saat yönünün tersine 90°)
                    out.SetPixel(y, width - 1 - x, pixel);
                    break;
            }
        }
    }
    return true;
}

bool ScaleImage(const Image& source, int width, int height, Image& out) {
    if (!source.Valid() || width <= 0 || height <= 0 || &source == &out) {
        return false;
    }
    if (width == source.Width() && height == source.Height()) {
        return CropImage(source, 0, 0, width, height, out);
    }
    if (!out.Create(width, height)) {
        return false;
    }

    const int sourceWidth = source.Width();
    const int sourceHeight = source.Height();

    // Sabit noktalı 16.16 adım: kayan noktayla piksel başına iki çarpma yerine
    // tam sayı toplama. Büyük görüntülerde fark ölçülebilir.
    const int64_t stepX =
        (static_cast<int64_t>(sourceWidth) << 16) / width;
    const int64_t stepY =
        (static_cast<int64_t>(sourceHeight) << 16) / height;

    for (int y = 0; y < height; ++y) {
        // Piksel MERKEZİNDEN örneklenir (+0.5). Köşeden örneklemek görüntüyü
        // yarım piksel sola-yukarı kaydırır ve küçültmede kenarları yer.
        const int64_t sy = ((static_cast<int64_t>(y) << 16) + 32768) * stepY / 65536 - 32768;
        const int y0 = static_cast<int>(sy >> 16);
        const int fracY = static_cast<int>(sy & 0xFFFF);
        const int y1 = (y0 + 1 < sourceHeight) ? y0 + 1 : sourceHeight - 1;
        const int yy0 = y0 < 0 ? 0 : y0;

        for (int x = 0; x < width; ++x) {
            const int64_t sx =
                ((static_cast<int64_t>(x) << 16) + 32768) * stepX / 65536 - 32768;
            const int x0 = static_cast<int>(sx >> 16);
            const int fracX = static_cast<int>(sx & 0xFFFF);
            const int x1 = (x0 + 1 < sourceWidth) ? x0 + 1 : sourceWidth - 1;
            const int xx0 = x0 < 0 ? 0 : x0;

            const uint32_t p00 = source.Pixel(xx0, yy0);
            const uint32_t p10 = source.Pixel(x1, yy0);
            const uint32_t p01 = source.Pixel(xx0, y1);
            const uint32_t p11 = source.Pixel(x1, y1);

            uint32_t result = 0;
            for (int shift = 0; shift <= 24; shift += 8) {
                const int c00 = static_cast<int>((p00 >> shift) & 0xFFu);
                const int c10 = static_cast<int>((p10 >> shift) & 0xFFu);
                const int c01 = static_cast<int>((p01 >> shift) & 0xFFu);
                const int c11 = static_cast<int>((p11 >> shift) & 0xFFu);

                const int top = c00 + (((c10 - c00) * fracX) >> 16);
                const int bottom = c01 + (((c11 - c01) * fracX) >> 16);
                const int value = top + (((bottom - top) * fracY) >> 16);

                result |= static_cast<uint32_t>(value & 0xFF) << shift;
            }
            out.SetPixel(x, y, result);
        }
    }
    return true;
}

bool ScaleImageByPercent(const Image& source, int percent, Image& out) {
    if (!source.Valid() || percent <= 0) {
        return false;
    }
    int width = ::MulDiv(source.Width(), percent, 100);
    int height = ::MulDiv(source.Height(), percent, 100);
    if (width < 1) {
        width = 1;
    }
    if (height < 1) {
        height = 1;
    }
    return ScaleImage(source, width, height, out);
}

}  // namespace crisp
