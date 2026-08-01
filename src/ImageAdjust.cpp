// ImageAdjust.cpp — bkz. ImageAdjust.h.
#include "ImageAdjust.h"

#include <cmath>
#include <vector>

namespace crisp {
namespace {

[[nodiscard]] uint8_t Clamp255(int value) noexcept {
    return static_cast<uint8_t>(value < 0 ? 0 : (value > 255 ? 255 : value));
}

// ALFA HİÇ DEĞİŞMEZ. Yakalamalar opaktır ama düzenleyiciden gelen bir görüntü
// olmayabilir ve parlaklığı artırırken saydamlığı da artırmak, hiç kimsenin
// istemediği bir yan etki olurdu.
template <typename Fn>
void ForEachPixel(Image& image, Fn&& transform) noexcept {
    if (!image.Valid()) {
        return;
    }
    auto* pixels = static_cast<uint32_t*>(image.Bits());
    if (pixels == nullptr) {
        return;
    }
    const size_t count =
        static_cast<size_t>(image.Width()) * static_cast<size_t>(image.Height());
    for (size_t i = 0; i < count; ++i) {
        const uint32_t pixel = pixels[i];
        int r = static_cast<int>((pixel >> 16) & 0xFFu);
        int g = static_cast<int>((pixel >> 8) & 0xFFu);
        int b = static_cast<int>(pixel & 0xFFu);
        transform(r, g, b);
        pixels[i] = (pixel & 0xFF000000u) |
                    (static_cast<uint32_t>(Clamp255(r)) << 16) |
                    (static_cast<uint32_t>(Clamp255(g)) << 8) |
                    static_cast<uint32_t>(Clamp255(b));
    }
}

[[nodiscard]] int Luma(int r, int g, int b) noexcept {
    return (299 * r + 587 * g + 114 * b) / 1000;
}

[[nodiscard]] int ClampAmount(int amount) noexcept {
    return amount < -100 ? -100 : (amount > 100 ? 100 : amount);
}

}  // namespace

void ApplyGrayscale(Image& image) noexcept {
    ForEachPixel(image, [](int& r, int& g, int& b) {
        const int grey = Luma(r, g, b);
        r = grey;
        g = grey;
        b = grey;
    });
}

void ApplyInvert(Image& image) noexcept {
    ForEachPixel(image, [](int& r, int& g, int& b) {
        r = 255 - r;
        g = 255 - g;
        b = 255 - b;
    });
}

void ApplySepia(Image& image) noexcept {
    // Microsoft'un yıllardır dolaşan sepia matrisi; katsayıların toplamı 1'i
    // aştığı için açık tonlar doyar ve kırpma bunu üstlenir.
    ForEachPixel(image, [](int& r, int& g, int& b) {
        const int sr = (393 * r + 769 * g + 189 * b) / 1000;
        const int sg = (349 * r + 686 * g + 168 * b) / 1000;
        const int sb = (272 * r + 534 * g + 131 * b) / 1000;
        r = sr;
        g = sg;
        b = sb;
    });
}

void AdjustBrightness(Image& image, int amount) noexcept {
    const int delta = ClampAmount(amount) * 255 / 100;
    if (delta == 0) {
        return;
    }
    ForEachPixel(image, [delta](int& r, int& g, int& b) {
        r += delta;
        g += delta;
        b += delta;
    });
}

void AdjustContrast(Image& image, int amount) noexcept {
    const int clamped = ClampAmount(amount);
    if (clamped == 0) {
        return;
    }
    // Standart kontrast eğrisi; ±100'de çarpan 0 ile ~5 arasında kalır.
    const double level = static_cast<double>(clamped) * 2.55;
    const double factor =
        (259.0 * (level + 255.0)) / (255.0 * (259.0 - level));
    ForEachPixel(image, [factor](int& r, int& g, int& b) {
        r = static_cast<int>(factor * (r - 128) + 128.0);
        g = static_cast<int>(factor * (g - 128) + 128.0);
        b = static_cast<int>(factor * (b - 128) + 128.0);
    });
}

void AdjustSaturation(Image& image, int amount) noexcept {
    const int clamped = ClampAmount(amount);
    if (clamped == 0) {
        return;
    }
    // Griye doğru ya da griden uzağa doğrusal karışım: -100 tam gri,
    // +100 doygunluğu iki katına çıkarır.
    const int scale = 100 + clamped;
    ForEachPixel(image, [scale](int& r, int& g, int& b) {
        const int grey = Luma(r, g, b);
        r = grey + (r - grey) * scale / 100;
        g = grey + (g - grey) * scale / 100;
        b = grey + (b - grey) * scale / 100;
    });
}

void AdjustGamma(Image& image, int percent) {
    if (percent < 10) {
        percent = 10;
    }
    if (percent > 500) {
        percent = 500;
    }
    if (percent == 100) {
        return;
    }

    // ARAMA TABLOSU: piksel başına pow() çağırmak 4K bir görüntüde sekiz
    // milyon üstel hesap demek; 256 girdilik tablo aynı sonucu verir.
    const double gamma = static_cast<double>(percent) / 100.0;
    uint8_t table[256];
    for (int i = 0; i < 256; ++i) {
        const double normalized = static_cast<double>(i) / 255.0;
        table[i] = Clamp255(
            static_cast<int>(std::pow(normalized, gamma) * 255.0 + 0.5));
    }

    ForEachPixel(image, [&table](int& r, int& g, int& b) {
        r = table[Clamp255(r)];
        g = table[Clamp255(g)];
        b = table[Clamp255(b)];
    });
}

void Convolve3x3(Image& image, const int kernel[9], int divisor, int bias) {
    if (!image.Valid() || kernel == nullptr || divisor == 0) {
        return;
    }
    const int width = image.Width();
    const int height = image.Height();

    // KAYNAK KOPYASI ZORUNLU: yerinde çalışan bir konvolüsyon, henüz
    // işlenmemiş komşuların yerine zaten işlenmişleri okur ve sonuç sağ-aşağı
    // doğru bulaşır.
    std::vector<uint32_t> source(static_cast<size_t>(width) *
                                 static_cast<size_t>(height));
    const auto* bits = static_cast<const uint8_t*>(image.Bits());
    for (int y = 0; y < height; ++y) {
        const auto* row = reinterpret_cast<const uint32_t*>(
            bits + static_cast<size_t>(y) * image.Stride());
        for (int x = 0; x < width; ++x) {
            source[static_cast<size_t>(y) * width + x] = row[x];
        }
    }

    auto sample = [&](int x, int y) -> uint32_t {
        // KENAR UZATMA: dışarısını sıfır saymak görüntünün çevresine koyu bir
        // çerçeve çizerdi.
        const int cx = x < 0 ? 0 : (x >= width ? width - 1 : x);
        const int cy = y < 0 ? 0 : (y >= height ? height - 1 : y);
        return source[static_cast<size_t>(cy) * width + cx];
    };

    auto* pixels = static_cast<uint8_t*>(image.Bits());
    for (int y = 0; y < height; ++y) {
        auto* row = reinterpret_cast<uint32_t*>(
            pixels + static_cast<size_t>(y) * image.Stride());
        for (int x = 0; x < width; ++x) {
            int accumulator[3] = {0, 0, 0};
            for (int ky = 0; ky < 3; ++ky) {
                for (int kx = 0; kx < 3; ++kx) {
                    const int weight = kernel[ky * 3 + kx];
                    if (weight == 0) {
                        continue;
                    }
                    const uint32_t pixel = sample(x + kx - 1, y + ky - 1);
                    accumulator[0] += weight * static_cast<int>((pixel >> 16) & 0xFFu);
                    accumulator[1] += weight * static_cast<int>((pixel >> 8) & 0xFFu);
                    accumulator[2] += weight * static_cast<int>(pixel & 0xFFu);
                }
            }
            const uint32_t original = source[static_cast<size_t>(y) * width + x];
            row[x] = (original & 0xFF000000u) |
                     (static_cast<uint32_t>(
                          Clamp255(accumulator[0] / divisor + bias)) << 16) |
                     (static_cast<uint32_t>(
                          Clamp255(accumulator[1] / divisor + bias)) << 8) |
                     static_cast<uint32_t>(
                         Clamp255(accumulator[2] / divisor + bias));
        }
    }
}

void ApplySharpen(Image& image, int amount) {
    if (amount <= 0) {
        return;
    }
    if (amount > 100) {
        amount = 100;
    }
    // Merkez ağırlığı şiddetle büyür, çevre sabit kalır ve bölen toplamı
    // korur; böylece şiddet 0'a yaklaşırken sonuç kimliğe yaklaşır.
    const int centre = 8 + amount / 4;
    const int kernel[9] = {0, -1, 0, -1, centre, -1, 0, -1, 0};
    Convolve3x3(image, kernel, centre - 4, 0);
}

}  // namespace crisp
