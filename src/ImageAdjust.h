// ImageAdjust.h — Piksel başına renk ayarlamaları ve küçük süzgeçler.
//
// HEPSİ YERİNDE ÇALIŞIR ve boyutu değiştirmez. Ayrı bir çıktı görüntüsü almak,
// çağıranı her adımda bir kopya tutmaya zorlardı; düzenleyicide efektler zaten
// tabandan yeniden oynanıyor.
//
// ImageEffects.h'den FARKI: orası bir BÖLGEYE uygulanan gizleme efektlerini
// (bulanıklık, mozaik) barındırır, burası görüntünün TAMAMINA uygulanan renk
// ayarlarını. İkisi bir dosyada olsaydı "seçili alanı bulanıklaştır" ile
// "resmi gri yap" aynı yerde durur ve hangisinin bölge aldığı karışırdı.
#pragma once

#include "Capture.h"

#include <cstdint>

namespace crisp {

// Parametresiz dönüşümler.
void ApplyGrayscale(Image& image) noexcept;
void ApplyInvert(Image& image) noexcept;
void ApplySepia(Image& image) noexcept;

// -100..100; 0 hiçbir şey yapmaz.
void AdjustBrightness(Image& image, int amount) noexcept;
void AdjustContrast(Image& image, int amount) noexcept;
void AdjustSaturation(Image& image, int amount) noexcept;

// 10..500 yüzde; 100 hiçbir şey yapmaz. 100'ün ALTI görüntüyü açar (koyu
// bölgeler kalkar), üstü koyulaştırır — gama eğrisinin yönü budur ve ters
// çevirmek kullanıcıyı şaşırtırdı.
void AdjustGamma(Image& image, int percent);

// 3×3 konvolüsyon. Çekirdek satır satır verilir; sonuç
// (toplam + bias) / divisor'dır ve 0..255'e kırpılır.
//
// DIŞARI AÇIK: keskinleştirme, kabartma ve kenar bulma aynı fonksiyonun farklı
// çekirdekleridir ve her biri için ayrı bir döngü yazmak aynı sınır
// denetimini üç kez kopyalamak olurdu.
void Convolve3x3(Image& image, const int kernel[9], int divisor, int bias);

// 0..100; 0 hiçbir şey yapmaz.
void ApplySharpen(Image& image, int amount);

}  // namespace crisp
