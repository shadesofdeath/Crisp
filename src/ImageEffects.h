// ImageEffects.h — Görüntü üzerinde yerinde çalışan piksel efektleri.
//
// PENCEREYE BAĞLI DEĞİL: ikisi de saf piksel işlemi olduğu için çekirdekte
// durur ve doğrudan test edilir. Bir ekran görüntüsünde e-posta adresini
// kapatan şeyin gerçekten okunamaz hâle geldiğini, arayüzü açmadan
// doğrulayabilmek gerekir.
#pragma once

#include "Capture.h"

#include <windows.h>

namespace crisp {

// Dikdörtgen alanı bulanıklaştırır. radius piksel cinsinden yarıçaptır;
// 0 ya da negatif hiçbir şey yapmaz.
//
// KUTU BULANIKLIĞI, GAUSS DEĞİL: iki geçişli kutu bulanıklığı O(n) çalışır ve
// yeterince büyük yarıçapta gözle Gauss'tan ayırt edilemez. Gizlilik için
// gereken şey görsel incelik değil, metnin geri getirilemez olması.
void BlurRegion(Image& image, const RECT& area, int radius);

// Dikdörtgen alanı mozaikler. block piksel cinsinden karo kenarıdır;
// 2'den küçük değerler hiçbir şey yapmaz.
//
// Her karo, KENDİ piksellerinin ortalamasıyla doldurulur. Tek bir pikseli
// örnekleyip yaymak daha ucuz olurdu ama karo rengi karonun içeriğini temsil
// etmez ve sonuç gürültülü görünür.
void MosaicRegion(Image& image, const RECT& area, int block);

}  // namespace crisp
