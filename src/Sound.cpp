// Sound.cpp — bkz. Sound.h.
#include "Sound.h"

#include <cmath>

namespace crisp {
namespace {

// Toplam süre. Daha uzunu deklanşör değil "tık tık" gibi duyuluyor; daha
// kısası hoparlörün tepki süresine sıkışıp duyulmuyor.
constexpr double kDurationSeconds = 0.11;

// İki vuruşun başlangıç anı ve sönüm hızı.
constexpr double kFirstClickAt = 0.000;
constexpr double kSecondClickAt = 0.055;
constexpr double kDecayPerSecond = 90.0;

constexpr double kAmplitude = 0.42;   // 1.0 kırpar; kulakta sert duyulur

// SABİT TOHUMLU üreteç. Deklanşör sesi geniş bantlı bir gürültü patlamasıdır;
// std::rand kullanmak, sesi sürecin rastgele durumuna bağlar ve testte
// örnekleri karşılaştırmayı imkânsız kılardı.
class Noise {
public:
    [[nodiscard]] double Next() noexcept {
        // Numerical Recipes'in doğrusal eşleniği; taşma bilinçli ve tanımlı.
        m_state = m_state * 1664525u + 1013904223u;
        const double unit = static_cast<double>(m_state >> 8) / 16777216.0;
        return unit * 2.0 - 1.0;   // [-1, 1]
    }

private:
    uint32_t m_state = 0x5EED1234u;
};

void PushLittleEndian32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xFFu));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFFu));
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFFu));
}

void PushLittleEndian16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xFFu));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
}

void PushTag(std::vector<uint8_t>& out, const char* tag) {
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<uint8_t>(tag[i]));
    }
}

// Tek bir vuruşun t anındaki genliği: ani başlayıp üstel sönen gürültü.
[[nodiscard]] double ClickEnvelope(double t, double start) noexcept {
    if (t < start) {
        return 0.0;
    }
    return ::exp(-(t - start) * kDecayPerSecond);
}

}  // namespace

std::vector<uint8_t> BuildShutterWav() {
    const uint32_t sampleCount =
        static_cast<uint32_t>(kShutterSampleRate * kDurationSeconds);
    const uint32_t dataBytes = sampleCount * (kShutterBitsPerSample / 8);

    std::vector<uint8_t> wav;
    wav.reserve(kWavHeaderSize + dataBytes);

    PushTag(wav, "RIFF");
    PushLittleEndian32(wav, 36u + dataBytes);   // dosya boyutu - 8
    PushTag(wav, "WAVE");

    PushTag(wav, "fmt ");
    PushLittleEndian32(wav, 16u);   // PCM için fmt bloğu 16 bayttır
    PushLittleEndian16(wav, 1u);    // PCM
    PushLittleEndian16(wav, kShutterChannels);
    PushLittleEndian32(wav, kShutterSampleRate);
    PushLittleEndian32(wav, kShutterSampleRate * kShutterChannels *
                                (kShutterBitsPerSample / 8u));   // bayt/saniye
    PushLittleEndian16(wav,
                       static_cast<uint16_t>(kShutterChannels *
                                             (kShutterBitsPerSample / 8u)));
    PushLittleEndian16(wav, kShutterBitsPerSample);

    PushTag(wav, "data");
    PushLittleEndian32(wav, dataBytes);

    Noise noise;
    for (uint32_t i = 0; i < sampleCount; ++i) {
        const double t = static_cast<double>(i) / kShutterSampleRate;
        const double envelope =
            ClickEnvelope(t, kFirstClickAt) + ClickEnvelope(t, kSecondClickAt);

        // Gürültüye alçak frekanslı bir vuruş eklenir: saf gürültü "şş" gibi
        // duyuluyor, mekanik tık için gövde gerekiyor.
        const double body = ::sin(t * 2.0 * 3.14159265358979 * 180.0) * 0.35;
        double value = (noise.Next() + body) * envelope * kAmplitude;

        value = value > 1.0 ? 1.0 : (value < -1.0 ? -1.0 : value);
        const int16_t sample = static_cast<int16_t>(value * 32767.0);
        PushLittleEndian16(wav, static_cast<uint16_t>(sample));
    }

    return wav;
}

}  // namespace crisp
