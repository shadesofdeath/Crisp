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

int ZoomStep(int currentPercent, int wheelDelta) noexcept {
    int value = Clamp(currentPercent, kZoomMin, kZoomMax);
    if (wheelDelta == 0) {
        return value;
    }

    // 120 birim = bir çentik. Yüksek çözünürlüklü tekerlekler daha küçük
    // değerler gönderir; en az bir adım uygulanır ki yavaş kaydırma hiç
    // yakınlaştırmamaya dönüşmesin.
    int notches = wheelDelta / WHEEL_DELTA;
    if (notches == 0) {
        notches = wheelDelta > 0 ? 1 : -1;
    }

    for (int i = 0; i < (notches > 0 ? notches : -notches); ++i) {
        if (notches > 0) {
            const int next = value + (value / 5) + 1;   // ≈ %20 büyüt
            value = next > kZoomMax ? kZoomMax : next;
        } else {
            const int next = value - (value / 6) - 1;   // ≈ %20 küçült
            value = next < kZoomMin ? kZoomMin : next;
        }
    }
    return value;
}

SIZE ScaledSize(SIZE original, int percent) noexcept {
    const int clamped = static_cast<int>(Clamp(percent, kZoomMin, kZoomMax));
    SIZE out{};
    out.cx = ::MulDiv(original.cx, clamped, 100);
    out.cy = ::MulDiv(original.cy, clamped, 100);
    if (out.cx < 1) {
        out.cx = 1;
    }
    if (out.cy < 1) {
        out.cy = 1;
    }
    return out;
}

POINT ZoomAnchoredOrigin(POINT windowTopLeft, POINT anchorScreen, SIZE oldSize,
                         SIZE newSize) noexcept {
    if (oldSize.cx <= 0 || oldSize.cy <= 0) {
        return windowTopLeft;
    }

    // Çapanın pencere içindeki oranı korunur.
    const LONG offsetX = anchorScreen.x - windowTopLeft.x;
    const LONG offsetY = anchorScreen.y - windowTopLeft.y;

    const LONG scaledX = ::MulDiv(offsetX, newSize.cx, oldSize.cx);
    const LONG scaledY = ::MulDiv(offsetY, newSize.cy, oldSize.cy);

    return POINT{anchorScreen.x - scaledX, anchorScreen.y - scaledY};
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


double FitScale(int imageWidth, int imageHeight, int viewWidth,
                int viewHeight) noexcept {
    if (imageWidth <= 0 || imageHeight <= 0 || viewWidth <= 0 ||
        viewHeight <= 0) {
        return 1.0;
    }
    const double byWidth = static_cast<double>(viewWidth) / imageWidth;
    const double byHeight = static_cast<double>(viewHeight) / imageHeight;
    const double fit = byWidth < byHeight ? byWidth : byHeight;
    return fit < 1.0 ? fit : 1.0;
}

double ClampZoom(double zoom, double minimum, double maximum) noexcept {
    if (zoom < minimum) {
        return minimum;
    }
    return zoom > maximum ? maximum : zoom;
}

POINT ClampPan(POINT pan, int scaledWidth, int scaledHeight, int viewWidth,
               int viewHeight) noexcept {
    const int limitX = scaledWidth > viewWidth ? (scaledWidth - viewWidth) / 2 : 0;
    const int limitY =
        scaledHeight > viewHeight ? (scaledHeight - viewHeight) / 2 : 0;

    POINT clamped = pan;
    clamped.x = clamped.x < -limitX ? -limitX
                                    : (clamped.x > limitX ? limitX : clamped.x);
    clamped.y = clamped.y < -limitY ? -limitY
                                    : (clamped.y > limitY ? limitY : clamped.y);
    return clamped;
}

RECT CanvasRect(const RECT& viewport, int imageWidth, int imageHeight,
                double scale, POINT pan) noexcept {
    const int width = static_cast<int>(imageWidth * scale);
    const int height = static_cast<int>(imageHeight * scale);
    const int left = viewport.left +
                     (static_cast<int>(Width(viewport)) - width) / 2 + pan.x;
    const int top = viewport.top +
                    (static_cast<int>(Height(viewport)) - height) / 2 + pan.y;
    return RECT{left, top, left + width, top + height};
}

POINT ViewToImage(POINT client, POINT canvasOrigin, double scale) noexcept {
    if (scale <= 0.0) {
        return POINT{0, 0};
    }
    return POINT{static_cast<LONG>((client.x - canvasOrigin.x) / scale),
                 static_cast<LONG>((client.y - canvasOrigin.y) / scale)};
}

POINT ImageToView(POINT image, POINT canvasOrigin, double scale) noexcept {
    return POINT{canvasOrigin.x + static_cast<LONG>(image.x * scale),
                 canvasOrigin.y + static_cast<LONG>(image.y * scale)};
}

POINT PanForZoomAnchor(POINT anchor, const RECT& viewport, int imageWidth,
                       int imageHeight, double oldScale, POINT oldPan,
                       double newScale) noexcept {
    const RECT before =
        CanvasRect(viewport, imageWidth, imageHeight, oldScale, oldPan);
    const POINT image =
        ViewToImage(anchor, POINT{before.left, before.top}, oldScale);

    const RECT after =
        CanvasRect(viewport, imageWidth, imageHeight, newScale, oldPan);
    const POINT moved = ImageToView(image, POINT{after.left, after.top}, newScale);

    POINT pan = oldPan;
    pan.x += anchor.x - moved.x;
    pan.y += anchor.y - moved.y;

    const int scaledWidth = static_cast<int>(imageWidth * newScale);
    const int scaledHeight = static_cast<int>(imageHeight * newScale);
    return ClampPan(pan, scaledWidth, scaledHeight,
                    static_cast<int>(Width(viewport)),
                    static_cast<int>(Height(viewport)));
}
}  // namespace geom
}  // namespace crisp
