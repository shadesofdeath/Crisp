// ImageTransform.cpp — bkz. ImageTransform.h.
#include "ImageTransform.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

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

// --- Yeniden örnekleme -------------------------------------------------------
//
// TEK BİR SÜZGEÇ YETMEZ. Eski kod her yönde iki doğrusal (bilinear) örnekleme
// yapıyordu ve KÜÇÜLTMEDE bu yanlıştır: %25'e inerken her hedef piksel 4×4 = 16
// kaynak pikselin yerini tutar ama bilinear yalnızca 2×2 okur, kalan 12'si
// çöpe gider. Sonuç, ince metnin ve çizgilerin parça parça yok olması —
// "aşırı kalite düşüyor" denen şey tam olarak budur.
//
// Bunun yerine eksen başına süzgeç seçilir:
//   küçültme → ALAN ORTALAMASI (hedef pikselin kapladığı tüm kaynak aralığı)
//   büyütme  → CATMULL-ROM (kübik; bilinear'ın bulanıklığı yerine keskin kenar)
//
// Ayrık (separable) uygulanır: önce yatay, sonra dikey. 2B çekirdek doğrudan
// uygulansaydı piksel başına taps² çarpma gerekirdi; ayrık hâlde 2·taps.

// Catmull-Rom çekirdeği (a = -0.5). Toplamı analitik olarak 1'dir.
[[nodiscard]] double CubicKernel(double x) noexcept {
    x = x < 0.0 ? -x : x;
    if (x < 1.0) {
        return ((1.5 * x - 2.5) * x) * x + 1.0;
    }
    if (x < 2.0) {
        return (((-0.5 * x) + 2.5) * x - 4.0) * x + 2.0;
    }
    return 0.0;
}

// Bir eksenin katkı tablosu. Her hedef piksel için ilk kaynak indeksi ve
// ardışık ağırlıklar; ağırlıklar 16.16 sabit noktadır ve satır toplamı tam
// 65536'dır — normalleştirme örnekleme sırasında değil BİR KEZ burada yapılır.
struct AxisTaps {
    int count = 0;
    std::vector<int> first;    // destLen; negatif olabilir (kenar taşması)
    std::vector<int> weight;   // destLen * count
};

// Yuvarlama artığını en ağır dokunmaya ekler. Artık dağıtılmazsa düz bir renk
// ölçeklendiğinde 0xFF yerine 0xFE çıkar ve testler bunu görür.
void NormalizeRow(int* weights, int count) noexcept {
    int total = 0;
    int heaviest = 0;
    for (int t = 0; t < count; ++t) {
        total += weights[t];
        if (weights[t] > weights[heaviest]) {
            heaviest = t;
        }
    }
    weights[heaviest] += 65536 - total;
}

[[nodiscard]] AxisTaps BuildTaps(int sourceLen, int destLen) {
    AxisTaps taps;
    const double scale = static_cast<double>(sourceLen) /
                         static_cast<double>(destLen);

    if (destLen < sourceLen) {
        // Alan ortalaması: hedef piksel [i·s, (i+1)·s) aralığını kaplar ve her
        // kaynak piksel bu aralıkla ÖRTÜŞTÜĞÜ kadar katkı verir.
        taps.count = static_cast<int>(std::ceil(scale)) + 1;
        taps.first.resize(static_cast<size_t>(destLen));
        taps.weight.resize(static_cast<size_t>(destLen) *
                           static_cast<size_t>(taps.count));

        for (int i = 0; i < destLen; ++i) {
            const double left = static_cast<double>(i) * scale;
            const double right = left + scale;
            const int firstIndex = static_cast<int>(std::floor(left));
            taps.first[static_cast<size_t>(i)] = firstIndex;

            int* row = &taps.weight[static_cast<size_t>(i) *
                                    static_cast<size_t>(taps.count)];
            for (int t = 0; t < taps.count; ++t) {
                const double lo = (std::max)(left, static_cast<double>(firstIndex + t));
                const double hi = (std::min)(right, static_cast<double>(firstIndex + t + 1));
                const double overlap = hi > lo ? (hi - lo) / scale : 0.0;
                row[t] = static_cast<int>(overlap * 65536.0 + 0.5);
            }
            NormalizeRow(row, taps.count);
        }
        return taps;
    }

    // Büyütme: dört dokunmalı kübik. Çıpa piksel MERKEZİDİR (+0.5/-0.5);
    // köşeden örneklemek görüntüyü yarım piksel kaydırır.
    taps.count = 4;
    taps.first.resize(static_cast<size_t>(destLen));
    taps.weight.resize(static_cast<size_t>(destLen) * 4u);

    for (int i = 0; i < destLen; ++i) {
        const double centre = (static_cast<double>(i) + 0.5) * scale - 0.5;
        const int firstIndex = static_cast<int>(std::floor(centre)) - 1;
        taps.first[static_cast<size_t>(i)] = firstIndex;

        int* row = &taps.weight[static_cast<size_t>(i) * 4u];
        for (int t = 0; t < 4; ++t) {
            const double distance = centre - static_cast<double>(firstIndex + t);
            row[t] = static_cast<int>(std::lround(CubicKernel(distance) * 65536.0));
        }
        NormalizeRow(row, 4);
    }
    return taps;
}

[[nodiscard]] uint8_t ClampByte(int32_t value) noexcept {
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return static_cast<uint8_t>(value);
}

// Tek eksende geçiş. Kaynak satırları `sourceLine` ile gezilir; hedef pikseller
// arasındaki adım `destStep` bayttır. Yatay geçişte adım 4 (yan yana piksel),
// dikey geçişte satır uzunluğudur — böylece iki geçiş tek fonksiyonla anlatılır.
void ResampleAxis(const uint8_t* source, int sourceStep, int sourceLen,
                  uint8_t* dest, int destStep, const AxisTaps& taps,
                  int lineCount, int sourceLineStride, int destLineStride) {
    const int count = taps.count;
    for (int line = 0; line < lineCount; ++line) {
        const uint8_t* sourceLine = source + static_cast<ptrdiff_t>(line) *
                                                 sourceLineStride;
        uint8_t* destLine = dest + static_cast<ptrdiff_t>(line) * destLineStride;

        for (size_t i = 0; i < taps.first.size(); ++i) {
            const int firstIndex = taps.first[i];
            const int* row = &taps.weight[i * static_cast<size_t>(count)];

            int32_t channel[4] = {0, 0, 0, 0};
            for (int t = 0; t < count; ++t) {
                const int weight = row[t];
                if (weight == 0) {
                    continue;
                }
                // KENAR UZATMA: taşan indeks en yakın kaynak pikseline
                // kırpılır. Sıfır saymak, kenarlarda koyu bir çerçeve bırakırdı.
                int index = firstIndex + t;
                index = index < 0 ? 0 : (index >= sourceLen ? sourceLen - 1 : index);
                const uint8_t* pixel = sourceLine +
                                       static_cast<ptrdiff_t>(index) * sourceStep;
                channel[0] += weight * pixel[0];
                channel[1] += weight * pixel[1];
                channel[2] += weight * pixel[2];
                channel[3] += weight * pixel[3];
            }

            uint8_t* out = destLine + static_cast<ptrdiff_t>(i) * destStep;
            out[0] = ClampByte((channel[0] + 32768) >> 16);
            out[1] = ClampByte((channel[1] + 32768) >> 16);
            out[2] = ClampByte((channel[2] + 32768) >> 16);
            out[3] = ClampByte((channel[3] + 32768) >> 16);
        }
    }
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

    const int sourceWidth = source.Width();
    const int sourceHeight = source.Height();
    const auto* sourceBits = static_cast<const uint8_t*>(source.Bits());
    if (sourceBits == nullptr) {
        return false;
    }

    // GDI bit eşlemleri 32767 kenarı aşamaz; daha büyüğünü istemek ara tamponu
    // gigabaytlara çıkarır ve Image::Create zaten reddeder.
    constexpr int kMaxSide = 32767;
    if (width > kMaxSide || height > kMaxSide) {
        return false;
    }

    // ARA TAMPON yatayda ölçeklenmiş, dikeyde ham hâli tutar. İki geçişi tek
    // geçişte birleştirmek piksel başına taps² çarpma demek olurdu.
    const size_t middleBytes = static_cast<size_t>(width) *
                               static_cast<size_t>(sourceHeight) * 4u;
    std::vector<uint8_t> middle;
    middle.resize(middleBytes);

    const AxisTaps horizontal = BuildTaps(sourceWidth, width);
    ResampleAxis(sourceBits, 4, sourceWidth, middle.data(), 4, horizontal,
                 sourceHeight, source.Stride(), width * 4);

    if (!out.Create(width, height)) {
        return false;
    }
    auto* outBits = static_cast<uint8_t*>(out.Bits());
    if (outBits == nullptr) {
        return false;
    }

    // Dikey geçiş: "satır" artık bir SÜTUNDUR. Adım ile satır uzunluğunun
    // rollerini değiştirmek, aynı çekirdeği ikinci kez yazmaktan iyidir.
    const AxisTaps vertical = BuildTaps(sourceHeight, height);
    ResampleAxis(middle.data(), width * 4, sourceHeight, outBits,
                 out.Stride(), vertical, width, 4, 4);
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
