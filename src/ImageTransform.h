// ImageTransform.h — Görüntünün kendisini değiştiren işlemler.
//
// Kırpma Capture.h'de (yakalama boru hattının parçası); döndürme ve
// ölçekleme burada. Üçü de saf piksel işlemi olduğu için çekirdekte durur ve
// doğrudan test edilir.
#pragma once

#include "Capture.h"

#include <windows.h>

namespace crisp {

// Saat yönünde çeyrek tur sayısı. Negatif değerler saat yönünün tersidir;
// 4'ün katları kimlik dönüşümüdür.
//
// AÇI DEĞİL ÇEYREK TUR: rastgele açıda döndürme yeniden örnekleme ister ve
// ekran görüntüsündeki metni bulanıklaştırır. Bir ekran alıntısı aracında
// gereken tek şey 90 derecelik adımlar ve onlar KAYIPSIZDIR — pikseller
// yalnızca yer değiştirir.
[[nodiscard]] bool RotateImage(const Image& source, int quarterTurns, Image& out);

// Yeni boyuta ölçekler. İki doğrusal (bilinear) örnekleme kullanılır.
[[nodiscard]] bool ScaleImage(const Image& source, int width, int height,
                              Image& out);

// Yüzdeye göre ölçekler; sonuç en az 1x1 olur.
[[nodiscard]] bool ScaleImageByPercent(const Image& source, int percent,
                                       Image& out);

}  // namespace crisp
