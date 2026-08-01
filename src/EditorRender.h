// EditorRender.h — Şekilleri bir görüntüye çizer.
//
// Çizim, ETKİLEŞİMDEN AYRI: aynı fonksiyon hem ekrandaki önizlemeyi hem de
// kaydedilecek/panoya gidecek son görüntüyü üretir. İkisi ayrı yollardan
// geçseydi kullanıcı gördüğünden farklı bir şey kaydedebilirdi.
#pragma once

#include "Annotation.h"
#include "Capture.h"

#include <vector>

#include <windows.h>

namespace crisp {

// Şekilleri görüntünün üstüne çizer; görüntü YERİNDE değişir.
//
// Efekt araçları (bulanıklık, mozaik) altlarındaki pikselleri değiştirdiği
// için ÖNCE uygulanır: sonra uygulansaydı üstlerine çizilmiş bir ok da
// bulanıklaşırdı.
void RenderShapes(Image& image, const std::vector<Shape>& shapes, unsigned dpi);

// Sürüklenmekte olan şekli DC'ye çizer (henüz belgeye eklenmemiş önizleme).
void RenderPreview(HDC dc, const Shape& shape, unsigned dpi);

// Metin ve adım rozetinin yazı tipi.
//
// DIŞARI AÇIK ÇÜNKÜ ÖNİZLEME DE KULLANIR: yazılmakta olan metnin kutusunu ve
// imlecini çizen kod aynı tipi kurmalı, yoksa önizleme sonuçtan farklı boyda
// çıkar. `scale` yakınlaştırma oranıdır — tuval %50'de çizilirken önizleme de
// yarı boyda olmalı.
[[nodiscard]] HFONT CreateTextFont(int thickness, unsigned dpi, double scale,
                                   bool bold);

}  // namespace crisp
