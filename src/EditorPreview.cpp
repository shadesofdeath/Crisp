// EditorPreview.cpp — Sürüklenmekte olan şeklin canlı önizlemesi.
//
// AYRI DOSYA: EditorRender.cpp 439 satıra çıkmıştı ve ev kuralı 400 (docs §9).
// Ayrım işlevsel de: EditorRender.cpp BİTMİŞ belgeyi piksellere çevirir,
// burası ise henüz bırakılmamış bir şeklin ekranda nasıl göründüğünü.
//
// ÖNİZLEME EKRANA ÇİZER, GÖRÜNTÜYE DEĞİL. Hiçbir şey belgeye işlenmiyor;
// kullanıcı fareyi bıraktığında şekil listeye giriyor ve asıl çizim orada
// yapılıyor. Bu yüzden burada yapılan her şey bir sonraki boyamada silinir.
#include "EditorRender.h"

#include "Geometry.h"
#include "ImageEffects.h"
#include "Util.h"

namespace crisp {
namespace {

// Ekrandaki bir bölgeyi okuyup efekti uygular ve geri yazar.
//
// EKRAN ÖLÇEĞİNDE ÇALIŞIR, GÖRÜNTÜ ÖLÇEĞİNDE DEĞİL: 4K bir yakalamada %25
// yakınlaştırmayla bakan kullanıcı için işlenecek alan on altıda birine iner,
// ve önizlemenin doğru görünmesi için gereken de zaten ekranda görünen ölçek.
void PreviewEffect(HDC dc, const Shape& shape) {
    const RECT bounds = shape.Bounds();
    const int width = static_cast<int>(geom::Width(bounds));
    const int height = static_cast<int>(geom::Height(bounds));
    if (width <= 0 || height <= 0) {
        return;
    }

    Image patch;
    if (!patch.Create(width, height)) {
        return;
    }
    const unique_hdc memory{::CreateCompatibleDC(dc)};
    if (!memory) {
        return;
    }
    const HGDIOBJ old = ::SelectObject(memory.get(), patch.Handle());
    ::BitBlt(memory.get(), 0, 0, width, height, dc, bounds.left, bounds.top,
             SRCCOPY);

    const RECT whole{0, 0, width, height};
    if (shape.kind == ToolKind::Blur) {
        BlurRegion(patch, whole, BlurRadiusFor(shape));
    } else {
        MosaicRegion(patch, whole, MosaicBlockFor(shape));
    }

    ::BitBlt(dc, bounds.left, bounds.top, width, height, memory.get(), 0, 0,
             SRCCOPY);
    ::SelectObject(memory.get(), old);
}

// Kırpma önizlemesi: dikdörtgenin DIŞINDA kalan her yeri karartır.
void DimOutside(HDC dc, const RECT& canvas, const RECT& keep) {
    const RECT clipped = geom::ClampTo(keep, canvas);

    // Dört şerit: üst, alt, sol, sağ. Tek bir bölge çıkarma işlemi yerine dört
    // dikdörtgen, çünkü AlphaBlend bölge değil dikdörtgen alıyor.
    const RECT bands[4] = {
        {canvas.left, canvas.top, canvas.right, clipped.top},
        {canvas.left, clipped.bottom, canvas.right, canvas.bottom},
        {canvas.left, clipped.top, clipped.left, clipped.bottom},
        {clipped.right, clipped.top, canvas.right, clipped.bottom},
    };

    Image black;
    if (!black.Create(1, 1)) {
        return;
    }
    black.SetPixel(0, 0, 0xFF000000u);

    const unique_hdc memory{::CreateCompatibleDC(dc)};
    if (!memory) {
        return;
    }
    const HGDIOBJ old = ::SelectObject(memory.get(), black.Handle());

    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 150;   // görüleni tamamen silmeden bastırır

    for (const RECT& band : bands) {
        const int width = static_cast<int>(geom::Width(band));
        const int height = static_cast<int>(geom::Height(band));
        if (width <= 0 || height <= 0) {
            continue;
        }
        ::AlphaBlend(dc, band.left, band.top, width, height, memory.get(), 0, 0,
                     1, 1, blend);
    }
    ::SelectObject(memory.get(), old);
}

}  // namespace

void RenderPreview(HDC dc, const Shape& shape, unsigned dpi, const RECT& canvas) {
    if (dc == nullptr) {
        return;
    }

    if (ToolIsEffect(shape.kind) || ToolIsImageOp(shape.kind)) {
        const RECT bounds = shape.Bounds();

        // KARARTMA ARACININ ÖNİZLEMESİ KARARTMAYI GÖSTERMELİ.
        //
        // Burada yalnızca kesikli bir çerçeve çiziliyordu ve gerekçesi "gerçek
        // bulanıklığı her fare hareketinde hesaplamak yavaşlar" idi. Gerekçe
        // ölçülmemişti ve yanlış çıktı: bulanıklık iki geçişli kayan pencere,
        // yani alanla doğrusal, ve burada TÜM GÖRÜNTÜ değil yalnızca seçilen
        // bölge işleniyor — üstelik ekran ölçeğinde, görüntü ölçeğinde değil.
        //
        // Bedeli ise gerçekti: bulanıklık bir GİZLEME aracı. Fareyi bırakana
        // kadar ne kadar gizlendiğini göremeyen kullanıcı, "yetmemiş" demek
        // için gizlemeye çalıştığı şeyi bir kez daha görmek zorunda kalıyordu.
        if (shape.kind == ToolKind::Blur || shape.kind == ToolKind::Mosaic) {
            PreviewEffect(dc, shape);
        } else {
            // KIRPMA: dışarıda kalan yer karartılır. Kırpmanın sorusu "ne
            // gidecek", ve kesikli bir çerçeve bunu yalnızca ima ediyordu.
            DimOutside(dc, canvas, bounds);
        }

        const HPEN pen = ::CreatePen(PS_DOT, 1, RGB(255, 255, 255));
        if (pen == nullptr) {
            return;
        }
        const HGDIOBJ oldPen = ::SelectObject(dc, pen);
        const HGDIOBJ oldBrush = ::SelectObject(dc, ::GetStockObject(NULL_BRUSH));
        const int oldRop = ::SetROP2(dc, R2_XORPEN);
        ::Rectangle(dc, bounds.left, bounds.top, bounds.right, bounds.bottom);
        ::SetROP2(dc, oldRop);
        ::SelectObject(dc, oldBrush);
        ::SelectObject(dc, oldPen);
        ::DeleteObject(pen);
        return;
    }
    DrawShape(dc, shape, dpi);
}

}  // namespace crisp
