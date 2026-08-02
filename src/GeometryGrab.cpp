// GeometryGrab.cpp — Seçim tutamaklarının aritmetiği.
//
// AYRI DOSYA: Geometry.cpp 293 satır ve bu dosyanın içeriği onu ev kuralının
// 400 satır sınırına dayardı (docs §9). Ayrım ayrıca işlevsel — burada tek bir
// konu var, seçimi bırakıldıktan sonra ayarlamak.
//
// HİÇBİR ŞEY BURADA WIN32 ÇAĞIRMAZ. Tutamak boyutu DPI ölçeklenmiş olarak
// dışarıdan geliyor, koordinatlar çağıranın uzayında; böylece kaplama penceresi
// olmadan, konsol test koşucusundan sınanabiliyor.
#include "Geometry.h"

namespace crisp {
namespace geom {
namespace {

// Tutamakların birbirini yemeye başladığı sınırlar, `handleSize` cinsinden.
//
// SAYILAR GEOMETRİDEN GELİYOR, ZEVKTEN DEĞİL. Bir tutamak kutusu merkezinin iki
// yanına `handleSize/2` uzanır. Karşılıklı iki köşe kutusunun ayrık kalması
// için kenar en az `2 * handleSize` olmalı; araya bir tutamak genişliğinde
// "taşı" bandı da sığsın istiyorsak `3 * handleSize`. Kenar ortası tutamağının
// köşelere değmemesi ise `5 * handleSize` istiyor: köşe kutuları uçlarda birer
// `handleSize` yer kaplıyor, ortadaki kutuya da kendi genişliği artı iki yanına
// birer boşluk gerekiyor.
constexpr LONG kCornersFrom = 3;
constexpr LONG kEdgesFrom = 5;

struct HandlePoint {
    POINT center;
    Grab grab;
};

}  // namespace

int HandleRects(const RECT& selection, LONG handleSize, RECT outRects[8],
                Grab outGrabs[8]) noexcept {
    if (handleSize <= 0 || outRects == nullptr || outGrabs == nullptr) {
        return 0;
    }

    const LONG width = Width(selection);
    const LONG height = Height(selection);
    if (width < kCornersFrom * handleSize || height < kCornersFrom * handleSize) {
        return 0;
    }

    const LONG midX = selection.left + width / 2;
    const LONG midY = selection.top + height / 2;

    // İSABET SIRASI: köşeler önce. Kenar ortası kutusu bir köşe kutusuyla
    // kesiştiğinde kullanıcının kastettiği köşedir; yakınlaştırılmış bir
    // seçimin köşesini tutmak, kenarını tutmaktan daha sık istenir.
    const HandlePoint points[8] = {
        {{selection.left, selection.top}, Grab::NW},
        {{selection.right, selection.top}, Grab::NE},
        {{selection.left, selection.bottom}, Grab::SW},
        {{selection.right, selection.bottom}, Grab::SE},
        {{midX, selection.top}, Grab::N},
        {{midX, selection.bottom}, Grab::S},
        {{selection.left, midY}, Grab::W},
        {{selection.right, midY}, Grab::E},
    };

    const bool withEdges =
        width >= kEdgesFrom * handleSize && height >= kEdgesFrom * handleSize;
    const int count = withEdges ? 8 : 4;

    const LONG half = handleSize / 2;
    for (int i = 0; i < count; ++i) {
        outRects[i] = RECT{points[i].center.x - half, points[i].center.y - half,
                           points[i].center.x - half + handleSize,
                           points[i].center.y - half + handleSize};
        outGrabs[i] = points[i].grab;
    }
    return count;
}

Grab HitTestSelection(const RECT& selection, POINT p, LONG grabSize) noexcept {
    RECT boxes[8]{};
    Grab grabs[8]{};
    const int count = HandleRects(selection, grabSize, boxes, grabs);

    for (int i = 0; i < count; ++i) {
        if (p.x >= boxes[i].left && p.x < boxes[i].right && p.y >= boxes[i].top &&
            p.y < boxes[i].bottom) {
            return grabs[i];
        }
    }

    if (p.x >= selection.left && p.x < selection.right && p.y >= selection.top &&
        p.y < selection.bottom) {
        return Grab::Move;
    }
    return Grab::None;
}

RECT OffsetClamped(const RECT& r, LONG dx, LONG dy, const RECT& bounds) noexcept {
    const LONG width = Width(r);
    const LONG height = Height(r);

    // Sığmıyorsa taşımanın anlamı yok; kırpmak elde kalan tek davranış.
    if (width > Width(bounds) || height > Height(bounds)) {
        return ClampTo(r, bounds);
    }

    RECT out{r.left + dx, r.top + dy, r.right + dx, r.bottom + dy};

    // KAYDIR, DARALTMA. Kenara dayanınca dikdörtgen durur ve boyutunu korur.
    if (out.left < bounds.left) {
        out.left = bounds.left;
        out.right = bounds.left + width;
    } else if (out.right > bounds.right) {
        out.right = bounds.right;
        out.left = bounds.right - width;
    }
    if (out.top < bounds.top) {
        out.top = bounds.top;
        out.bottom = bounds.top + height;
    } else if (out.bottom > bounds.bottom) {
        out.bottom = bounds.bottom;
        out.top = bounds.bottom - height;
    }
    return out;
}

RECT ResizeByGrab(const RECT& origin, Grab grab, POINT cursor, LONG minSide,
                  const RECT& bounds) noexcept {
    RECT r = origin;

    // YALNIZ TUTULAN KENAR YAZILIR. Karşı kenara dokunulmadığı için "sabit
    // nokta" diye ayrıca hesaplanacak bir şey yok, ve min/max karışmadığı için
    // kapsayıcı/dışlayıcı kenar karışıklığı da yapısal olarak imkânsız.
    switch (grab) {
        case Grab::W:
        case Grab::NW:
        case Grab::SW:
            r.left = cursor.x;
            break;
        case Grab::E:
        case Grab::NE:
        case Grab::SE:
            r.right = cursor.x;
            break;
        default:
            break;
    }
    switch (grab) {
        case Grab::N:
        case Grab::NW:
        case Grab::NE:
            r.top = cursor.y;
            break;
        case Grab::S:
        case Grab::SW:
        case Grab::SE:
            r.bottom = cursor.y;
            break;
        default:
            break;
    }

    // Ekranın dışına çıkmasın — diğer bütün yazma yerlerinin kullandığı clamp.
    r = ClampTo(r, bounds);

    // Sonra en küçük kenar zorlanır ve HAREKET EDEN kenar geri itilir; sabit
    // kenarı oynatmak, kullanıcının tutmadığı tarafı kaydırmak olurdu.
    const bool movesLeft = grab == Grab::W || grab == Grab::NW || grab == Grab::SW;
    const bool movesRight = grab == Grab::E || grab == Grab::NE || grab == Grab::SE;
    const bool movesTop = grab == Grab::N || grab == Grab::NW || grab == Grab::NE;
    const bool movesBottom = grab == Grab::S || grab == Grab::SW || grab == Grab::SE;

    if (movesLeft && r.right - r.left < minSide) {
        r.left = r.right - minSide;
    } else if (movesRight && r.right - r.left < minSide) {
        r.right = r.left + minSide;
    }
    if (movesTop && r.bottom - r.top < minSide) {
        r.top = r.bottom - minSide;
    } else if (movesBottom && r.bottom - r.top < minSide) {
        r.bottom = r.top + minSide;
    }

    // En küçük kenarı zorlamak dikdörtgeni yeniden dışarı taşımış olabilir:
    // sınırın dibindeki bir kenarı `minSide` kadar geri itmek onu sınırın
    // ötesine çıkarır. Burada ClampTo doğru olan — daraltmak, dışarıda kalmaya
    // yeğdir ve `bounds` her zaman `minSide`dan büyüktür.
    return ClampTo(r, bounds);
}

}  // namespace geom
}  // namespace crisp
