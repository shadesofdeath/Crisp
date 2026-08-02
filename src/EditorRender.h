// EditorRender.h — Şekilleri bir görüntüye çizer.
//
// Çizim, ETKİLEŞİMDEN AYRI: aynı fonksiyon hem ekrandaki önizlemeyi hem de
// kaydedilecek/panoya gidecek son görüntüyü üretir. İkisi ayrı yollardan
// geçseydi kullanıcı gördüğünden farklı bir şey kaydedebilirdi.
#pragma once

#include "Annotation.h"
#include "Capture.h"
#include "Geometry.h"

#include <vector>

#include <windows.h>

namespace crisp {

// Şekilleri görüntünün üstüne çizer; görüntü YERİNDE değişir.
//
// Efekt araçları (bulanıklık, mozaik) altlarındaki pikselleri değiştirdiği
// için ÖNCE uygulanır: sonra uygulansaydı üstlerine çizilmiş bir ok da
// bulanıklaşırdı.
void RenderShapes(Image& image, const std::vector<Shape>& shapes, unsigned dpi);

// Bulanıklık yarıçapı ve mozaik karesi, ŞEKLİN ÖLÇÜSÜNDEN türer.
//
// TEK YERDE, ÇÜNKÜ İKİ YER OKUYOR: son çizim (`RenderShapes`) ve canlı
// önizleme (`RenderPreview`). Formüller iki kopya hâlinde dursaydı, önizlemede
// gördüğü bulanıklıkla bıraktığında oluşan bulanıklığın farklı çıkması an
// meselesiydi — ve karartma aracında bu, gizlendiğini sanılan bir şeyin
// gizlenmemesi demek.
[[nodiscard]] inline LONG ShorterSide(const RECT& bounds) noexcept {
    const LONG width = geom::Width(bounds);
    const LONG height = geom::Height(bounds);
    return width < height ? width : height;
}

[[nodiscard]] inline int BlurRadiusFor(const Shape& shape) noexcept {
    // Yarıçap alanla ölçeklenir: küçük bir seçimde 20 piksellik bulanıklık her
    // şeyi tek renge indirirdi.
    //
    // ŞİDDET ŞEKİLDEN GELİR, ayardan değil: geri alma şekil listesini geri
    // sarar ve ayar sonradan değiştirildiğinde eski bulanıklıklar da yeni
    // şiddetle yeniden çizilirdi.
    const int radius =
        static_cast<int>(ShorterSide(shape.Bounds()) / 12) * shape.strength / 100;
    return radius < 3 ? 3 : radius;
}

[[nodiscard]] inline int MosaicBlockFor(const Shape& shape) noexcept {
    const int block =
        static_cast<int>(ShorterSide(shape.Bounds()) / 10) * shape.strength / 100;
    return block < 4 ? 4 : block;
}

// Sürüklenmekte olan şeklin önizlemesi. `canvas` görüntünün istemci
// koordinatındaki yeri; kırpma önizlemesi dışını karartmak için ona ihtiyaç
// duyuyor.
void RenderPreview(HDC dc, const Shape& shape, unsigned dpi,
                   const RECT& canvas);

// Tek bir şekli DC üzerine çizer.
//
// DIŞARI AÇIK, ÇÜNKÜ ÖNİZLEME DE ÇAĞIRIYOR ve o artık ayrı bir dosyada
// (EditorPreview.cpp). Aynı fonksiyon hem ekrandaki önizlemeyi hem
// kaydedilecek görüntüyü çiziyor; ikisi ayrı yollardan geçseydi kullanıcı
// gördüğünden farklı bir şey kaydedebilirdi.
void DrawShape(HDC dc, const Shape& shape, unsigned dpi);

// Metin ve adım rozetinin yazı tipi.
//
// DIŞARI AÇIK ÇÜNKÜ ÖNİZLEME DE KULLANIR: yazılmakta olan metnin kutusunu ve
// imlecini çizen kod aynı tipi kurmalı, yoksa önizleme sonuçtan farklı boyda
// çıkar. `scale` yakınlaştırma oranıdır — tuval %50'de çizilirken önizleme de
// yarı boyda olmalı.
[[nodiscard]] HFONT CreateTextFont(int thickness, unsigned dpi, double scale,
                                   bool bold);

}  // namespace crisp
