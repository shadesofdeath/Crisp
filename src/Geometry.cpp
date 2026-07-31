// Geometry.cpp — bkz. Geometry.h; hiçbir Win32 çağrısı yoktur.
#include "Geometry.h"

namespace crisp {
namespace geom {
namespace {

[[nodiscard]] constexpr LONG Min(LONG a, LONG b) noexcept { return a < b ? a : b; }
[[nodiscard]] constexpr LONG Max(LONG a, LONG b) noexcept { return a > b ? a : b; }

// Değeri [lo, hi] aralığına çeker. hi < lo ise lo döner — çağıranların hepsi
// bounds'un geçerli olduğunu varsayar, ama bozuk bir bounds'ta bile tanımsız
// davranış üretmemeli.
[[nodiscard]] constexpr LONG Clamp(LONG value, LONG lo, LONG hi) noexcept {
    if (hi < lo) {
        return lo;
    }
    return value < lo ? lo : (value > hi ? hi : value);
}

}  // namespace

RECT FromCorners(POINT a, POINT b) noexcept {
    RECT r{};
    r.left   = Min(a.x, b.x);
    r.top    = Min(a.y, b.y);
    r.right  = Max(a.x, b.x);
    r.bottom = Max(a.y, b.y);
    return r;
}

RECT ClampTo(const RECT& r, const RECT& bounds) noexcept {
    RECT out{};
    out.left   = Clamp(r.left,   bounds.left, bounds.right);
    out.top    = Clamp(r.top,    bounds.top,  bounds.bottom);
    out.right  = Clamp(r.right,  bounds.left, bounds.right);
    out.bottom = Clamp(r.bottom, bounds.top,  bounds.bottom);
    return out;
}

RECT InflateClamped(const RECT& r, LONG d, const RECT& bounds) noexcept {
    RECT out = r;
    out.left   -= d;
    out.top    -= d;
    out.right  += d;
    out.bottom += d;

    // Küçültme dikdörtgeni ters çevirebilir; önce çöküşü düzelt, sonra hapset.
    if (out.right < out.left) {
        out.right = out.left;
    }
    if (out.bottom < out.top) {
        out.bottom = out.top;
    }
    return ClampTo(out, bounds);
}

bool IsUsableSelection(const RECT& r, LONG minSide) noexcept {
    return Width(r) >= minSide && Height(r) >= minSide;
}

RECT MagnifierSource(POINT cursor, LONG sourceSide, const RECT& bounds) noexcept {
    if (sourceSide <= 0) {
        return RECT{cursor.x, cursor.y, cursor.x, cursor.y};
    }

    // Kaynak ekrandan büyükse kaydırmanın anlamı kalmaz; olanı ver.
    const LONG boundsWidth  = Width(bounds);
    const LONG boundsHeight = Height(bounds);
    if (sourceSide >= boundsWidth || sourceSide >= boundsHeight) {
        return bounds;
    }

    // İmleç ORTADA olacak şekilde başla. Tek sayı kenarlarda sol/üst yarım
    // piksel fazla olur; büyüteç için farkı görünmez, merkez pikselin daima
    // imlecin altındaki piksel olması ise şart.
    const LONG half = sourceSide / 2;
    LONG left = cursor.x - half;
    LONG top  = cursor.y - half;

    left = Clamp(left, bounds.left, bounds.right  - sourceSide);
    top  = Clamp(top,  bounds.top,  bounds.bottom - sourceSide);

    return RECT{left, top, left + sourceSide, top + sourceSide};
}

POINT MagnifierPlacement(POINT cursor, SIZE panelSize, LONG gap,
                         const RECT& bounds) noexcept {
    POINT p{cursor.x + gap, cursor.y + gap};

    // Sağa sığmıyorsa imlecin soluna geç.
    if (p.x + panelSize.cx > bounds.right) {
        p.x = cursor.x - gap - panelSize.cx;
    }
    // Aşağı sığmıyorsa imlecin üstüne geç.
    if (p.y + panelSize.cy > bounds.bottom) {
        p.y = cursor.y - gap - panelSize.cy;
    }

    // Karşı tarafa geçince de taşabilir (küçük ekran, büyük panel): son çare
    // olarak sınırların içine hapset.
    p.x = Clamp(p.x, bounds.left, Max(bounds.left, bounds.right  - panelSize.cx));
    p.y = Clamp(p.y, bounds.top,  Max(bounds.top,  bounds.bottom - panelSize.cy));
    return p;
}

POINT SizeLabelPlacement(const RECT& selection, SIZE labelSize, LONG gap,
                         const RECT& bounds) noexcept {
    POINT p{selection.left, selection.top - gap - labelSize.cy};

    // Üstte yer yoksa seçimin İÇİNE, üst kenara yasla.
    if (p.y < bounds.top) {
        p.y = selection.top + gap;
        // Seçim etiketi barındıramayacak kadar kısaysa altına taşı.
        if (p.y + labelSize.cy > selection.bottom) {
            p.y = selection.bottom + gap;
        }
    }

    p.x = Clamp(p.x, bounds.left, Max(bounds.left, bounds.right - labelSize.cx));
    p.y = Clamp(p.y, bounds.top,  Max(bounds.top,  bounds.bottom - labelSize.cy));
    return p;
}

POINT SnapToSquare(POINT anchor, POINT other) noexcept {
    const LONG dx = other.x - anchor.x;
    const LONG dy = other.y - anchor.y;

    const LONG adx = dx < 0 ? -dx : dx;
    const LONG ady = dy < 0 ? -dy : dy;
    const LONG side = Max(adx, ady);

    // Sürükleme yönü korunur: sola sürüklerken kare de sola büyümeli.
    const LONG signX = dx < 0 ? -1 : 1;
    const LONG signY = dy < 0 ? -1 : 1;

    return POINT{anchor.x + side * signX, anchor.y + side * signY};
}

}  // namespace geom
}  // namespace crisp
