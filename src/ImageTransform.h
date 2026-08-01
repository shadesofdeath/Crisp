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

// Yeni boyuta ölçekler.
//
// SÜZGEÇ EKSEN BAŞINA SEÇİLİR: küçülen eksende alan ortalaması, büyüyen
// eksende Catmull-Rom kübik. Her yerde bilinear kullanmak küçültmede kaynak
// piksellerin çoğunu hiç okumamak demektir — %25'e inen bir ekran görüntüsünde
// ince metin bundan dolayı dağılır.
[[nodiscard]] bool ScaleImage(const Image& source, int width, int height,
                              Image& out);

// Yüzdeye göre ölçekler; sonuç en az 1x1 olur.
[[nodiscard]] bool ScaleImageByPercent(const Image& source, int percent,
                                       Image& out);

enum class FlipAxis {
    Horizontal,   // sol-sağ aynala
    Vertical,     // üst-alt aynala
};

// Aynalama. Döndürme gibi KAYIPSIZDIR: pikseller yalnızca yer değiştirir.
[[nodiscard]] bool FlipImage(const Image& source, FlipAxis axis, Image& out);

// TUVALİ büyütür ya da küçültür; görüntü `offset` konumuna yerleştirilir ve
// boşta kalan yerler `fill` ile boyanır.
//
// CropImage BUNU YAPAMAZ ve bilinçli olarak yapamaz: taşan bir bölgeyi
// reddeder, çünkü yakalama boru hattında taşan bir istek her zaman bir
// hatadır. Tuval büyütmek ise KASITLI bir istektir — kenar boşluğu, gölge ve
// "güzelleştirme" bunun üstüne kurulur.
[[nodiscard]] bool ResizeCanvas(const Image& source, int width, int height,
                                int offsetX, int offsetY, uint32_t fill,
                                Image& out);

// Her kenarda aynı renkte olan kuşakları kırpar. Referans renk KÖŞELERDEN
// alınır; dördü birden aynı değilse sol-üst köşe kazanır.
//
// `tolerance` kanal başına izin verilen sapmadır: JPEG'den geçmiş bir
// görüntüde "beyaz" kenar tam olarak 255 değildir ve sıfır tolerans hiçbir şey
// kırpmazdı.
//
// TAMAMI TEK RENKSE hiçbir şey kırpılmaz: sonucun 0×0 olması ya da tek piksele
// inmesi, kullanıcının görüntüsünü yok etmek olurdu.
[[nodiscard]] bool AutoCropImage(const Image& source, int tolerance, Image& out);

}  // namespace crisp
