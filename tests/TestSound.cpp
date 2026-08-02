// TestSound.cpp — Deklanşör sesinin bayt düzeni.
#include "TestFramework.h"

#include "Sound.h"

#include <mmsystem.h>

#include <cstring>

using namespace crisp;

namespace {

[[nodiscard]] uint32_t ReadU32(const std::vector<uint8_t>& data, size_t at) {
    return static_cast<uint32_t>(data[at]) |
           (static_cast<uint32_t>(data[at + 1]) << 8) |
           (static_cast<uint32_t>(data[at + 2]) << 16) |
           (static_cast<uint32_t>(data[at + 3]) << 24);
}

[[nodiscard]] uint16_t ReadU16(const std::vector<uint8_t>& data, size_t at) {
    return static_cast<uint16_t>(static_cast<uint16_t>(data[at]) |
                                 (static_cast<uint16_t>(data[at + 1]) << 8));
}

[[nodiscard]] int16_t Sample(const std::vector<uint8_t>& data, size_t index) {
    return static_cast<int16_t>(ReadU16(data, kWavHeaderSize + index * 2));
}

[[nodiscard]] bool TagAt(const std::vector<uint8_t>& data, size_t at,
                         const char* tag) {
    return ::memcmp(data.data() + at, tag, 4) == 0;
}

}  // namespace

CRISP_TEST(Sound, Riff_basligi_dogru) {
    const std::vector<uint8_t> wav = BuildShutterWav();
    CHECK(wav.size() > kWavHeaderSize);
    CHECK(TagAt(wav, 0, "RIFF"));
    CHECK(TagAt(wav, 8, "WAVE"));
    CHECK(TagAt(wav, 12, "fmt "));
    CHECK(TagAt(wav, 36, "data"));
}

CRISP_TEST(Sound, Boyut_alanlari_gercek_uzunlukla_ortusur) {
    const std::vector<uint8_t> wav = BuildShutterWav();

    // RIFF boyutu, dosyanın ilk sekiz baytı hariç uzunluğudur. Yanlış
    // yazılırsa PlaySound sesi ya kısa keser ya hiç çalmaz — ve bu, çalışırken
    // sessizlik olarak görünür, hata olarak değil.
    CHECK_EQ(ReadU32(wav, 4), static_cast<uint32_t>(wav.size()) - 8u);
    CHECK_EQ(ReadU32(wav, 40),
             static_cast<uint32_t>(wav.size()) - kWavHeaderSize);
}

CRISP_TEST(Sound, Bicim_blogu_pcm_ve_beklenen_degerler) {
    const std::vector<uint8_t> wav = BuildShutterWav();
    CHECK_EQ(ReadU32(wav, 16), 16);           // fmt bloğu uzunluğu
    CHECK_EQ(ReadU16(wav, 20), 1);            // PCM
    CHECK_EQ(ReadU16(wav, 22), kShutterChannels);
    CHECK_EQ(ReadU32(wav, 24), kShutterSampleRate);
    CHECK_EQ(ReadU16(wav, 34), kShutterBitsPerSample);

    // Bayt/saniye ve blok hizası TÜRETİLMİŞ alanlardır; elle yazıldıkları için
    // diğerleriyle tutarsız kalmaları en olası hata.
    CHECK_EQ(ReadU32(wav, 28), kShutterSampleRate * kShutterChannels *
                                   (kShutterBitsPerSample / 8u));
    CHECK_EQ(ReadU16(wav, 32), kShutterChannels * (kShutterBitsPerSample / 8u));
}

CRISP_TEST(Sound, Iki_vurus_var_ve_arasi_sessiz) {
    const std::vector<uint8_t> wav = BuildShutterWav();
    const size_t samples = (wav.size() - kWavHeaderSize) / 2;
    CHECK(samples > 2000);

    auto peak = [&](size_t from, size_t to) {
        int maximum = 0;
        for (size_t i = from; i < to && i < samples; ++i) {
            const int value = Sample(wav, i) < 0 ? -Sample(wav, i) : Sample(wav, i);
            maximum = value > maximum ? value : maximum;
        }
        return maximum;
    };

    // İlk vuruş 0. örnekte, ikincisi 0,055 s = 1212. örnek civarında.
    const int first = peak(0, 60);
    const int gap = peak(900, 1150);
    const int second = peak(1212, 1272);

    CHECK(first > 3000);
    CHECK(second > 3000);
    // Aradaki sönüm gerçekten sessizliğe inmeli; inmezse iki vuruş tek bir
    // uzun gürültüye dönüşür ve deklanşör gibi duyulmaz.
    CHECK(gap < first / 10);
}

CRISP_TEST(Sound, Ses_sonunda_soner) {
    const std::vector<uint8_t> wav = BuildShutterWav();
    const size_t samples = (wav.size() - kWavHeaderSize) / 2;
    // Son yüz örnek sıfıra yakın olmalı; olmazsa hoparlörde "çıt" diye bir
    // kesilme duyulur.
    for (size_t i = samples - 100; i < samples; ++i) {
        const int value = Sample(wav, i) < 0 ? -Sample(wav, i) : Sample(wav, i);
        CHECK(value < 200);
    }
}

CRISP_TEST(Sound, Ayni_baytlari_uretir) {
    // SABİT TOHUM: ses her çağrıda aynı olmalı, yoksa bu dosyadaki diğer
    // testler rastgele geçip rastgele kalırdı.
    const std::vector<uint8_t> first = BuildShutterWav();
    const std::vector<uint8_t> second = BuildShutterWav();
    CHECK_EQ(first.size(), second.size());
    CHECK(first == second);
}

CRISP_TEST(Sound, Windows_tamponu_kabul_ediyor) {
    // BAYT DÜZENİ SINAMASI YETMEZ: başlık alanları tutarlı görünüp yine de
    // waveform aygıtının reddettiği bir tampon üretilebilir. Burada ses
    // GERÇEKTEN çalınır (SND_SYNC ile, ~110 ms) ve PlaySound'un kendisi
    // biçimi doğrular.
    //
    // AYGIT YOKSA SINAMA DA YOK. `PlaySound` çıkış aygıtı bulunmayan bir
    // makinede FALSE döner ve bu, tamponla ilgili hiçbir şey söylemez —
    // makinenin donanımıyla ilgili bir şey söyler. Sürüm sunucusunda ses kartı
    // yok ve bu sınama orada, kodda hiçbir şey değişmeden, kırmızı yanıyordu.
    // Ses kartı olan bir makinede sınama aynen çalışmaya devam ediyor.
    if (::waveOutGetNumDevs() == 0) {
        return;
    }

    const std::vector<uint8_t> wav = BuildShutterWav();
    const BOOL played = ::PlaySoundW(reinterpret_cast<LPCWSTR>(wav.data()),
                                     nullptr,
                                     SND_MEMORY | SND_SYNC | SND_NODEFAULT);
    CHECK(played != FALSE);
}