// Sound.h — Deklanşör sesinin üretimi.
//
// NEDEN ÜRETİLİYOR, DOSYADAN OKUNMUYOR: Windows'un Media klasöründe deklanşör
// ya da kamera sesi YOKTUR (yetmiş wav'ın hiçbiri değil). Depoya bir .wav
// koymak "tek dosya exe" hedefini bozmadan mümkün olsa da, sesi kaynak olarak
// gömmek ikiliyi büyütür ve elle üretilmiş bir varlığı daha bakıma bırakırdı.
// Yüz milisaniyelik bir tıkırtıyı hesaplamak birkaç satır ve hiçbir varlık
// gerektirmiyor.
//
// PENCERE YOK: burası yalnızca bayt üretir, çalmaz. Böylece başlığın doğruluğu
// ve örnek sayısı konsol koşucusundan sınanabilir; ses çalmak uygulama
// katmanının işi.
#pragma once

#include <cstdint>
#include <vector>

namespace crisp {

// Çalmaya hazır bir RIFF/WAVE tamponu döndürür: 22050 Hz, 16 bit, tek kanal.
//
// SES İKİ VURUŞTUR — perdenin açılışı ve kapanışı. Tek vuruş "tık" değil
// "çat" gibi duyuluyor; deklanşörü tanınır kılan şey iki vuruş arasındaki
// kısa boşluk.
[[nodiscard]] std::vector<uint8_t> BuildShutterWav();

// Üretilen sesin çözümlenebilmesi için gereken sabitler; testler de kullanır.
inline constexpr uint32_t kShutterSampleRate = 22050;
inline constexpr uint16_t kShutterBitsPerSample = 16;
inline constexpr uint16_t kShutterChannels = 1;

// RIFF başlığının uzunluğu: "RIFF" + boyut + "WAVE" + "fmt " bloğu + "data"
// başlığı.
inline constexpr size_t kWavHeaderSize = 44;

}  // namespace crisp
