// EditorSelect.cpp — Eklenmiş bir şekli seçme, taşıma, silme ve yeniden
// biçimlendirme.
//
// NEDEN VAR: bir ok yanlış yere çizildiğinde tek çare geri almaktı — ve geri
// alma ondan sonra çizilen her şeyi de götürüyordu. On şekil koyup dokuzuncuyu
// beş piksel kaydırmak isteyen kullanıcının önünde hiçbir yol yoktu.
//
// NEDEN TAŞIMA VAR AMA YENİDEN BOYUTLANDIRMA YOK: her şekil tipinin kendi
// tutamak mantığı var (serbest çizimin köşesi ne demek?) ve taşıma tek başına
// şikâyetin tamamını karşılıyor. Boyutlandırma, gerçekten istendiğinde
// eklenir; şimdi eklemek, kullanılmayan sekiz dal demek.
#include "EditorInternal.h"

#include "Geometry.h"

#include <cmath>

namespace crisp {
namespace editor {
namespace {

// Bir noktanın şekle isabet edip etmediği.
//
// SINIRLAYICI DİKDÖRTGEN YETMEZ: ince bir okun sınırı ekranın yarısı kadar
// olabilir ve o dikdörtgenin ortasına tıklamak oku seçmemeli. Çizgi benzeri
// şekillerde noktanın ÇİZGİYE uzaklığına bakılır.
[[nodiscard]] double DistanceToSegment(POINT p, POINT a, POINT b) noexcept {
    const double dx = static_cast<double>(b.x - a.x);
    const double dy = static_cast<double>(b.y - a.y);
    const double lengthSquared = dx * dx + dy * dy;
    if (lengthSquared < 1.0) {
        const double ax = static_cast<double>(p.x - a.x);
        const double ay = static_cast<double>(p.y - a.y);
        return std::sqrt(ax * ax + ay * ay);
    }
    double t = (static_cast<double>(p.x - a.x) * dx +
                static_cast<double>(p.y - a.y) * dy) / lengthSquared;
    t = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
    const double cx = static_cast<double>(a.x) + t * dx;
    const double cy = static_cast<double>(a.y) + t * dy;
    const double ex = static_cast<double>(p.x) - cx;
    const double ey = static_cast<double>(p.y) - cy;
    return std::sqrt(ex * ex + ey * ey);
}

[[nodiscard]] bool HitsShape(const Shape& shape, POINT point) noexcept {
    // Tolerans kalınlıkla büyür: 12 piksellik bir fırça darbesinin tam
    // ortasına tıklamak zorunda kalmak, aracı kullanılamaz yapardı.
    const double tolerance = 4.0 + static_cast<double>(shape.thickness);

    if (!shape.points.empty()) {
        for (size_t i = 1; i < shape.points.size(); ++i) {
            if (DistanceToSegment(point, shape.points[i - 1], shape.points[i]) <=
                tolerance * (shape.kind == ToolKind::Highlighter ? 4.0 : 1.0)) {
                return true;
            }
        }
        return false;
    }

    if (shape.kind == ToolKind::Arrow || shape.kind == ToolKind::Line) {
        return DistanceToSegment(point, shape.start, shape.end) <= tolerance;
    }

    RECT bounds = shape.Bounds();
    if (shape.kind == ToolKind::Text || shape.kind == ToolKind::StepNumber) {
        // Metin ve rozet TEK NOKTADIR: sınırları sıfır genişlikte olabilir ve
        // çevresine bir kavrama alanı verilmezse hiç seçilemezler.
        ::InflateRect(&bounds, 40, 24);
        return ::PtInRect(&bounds, point) != FALSE;
    }

    if (shape.filled || ToolIsEffect(shape.kind)) {
        return ::PtInRect(&bounds, point) != FALSE;
    }

    // İçi boş dikdörtgen ve elips: yalnızca KENARI tıklanabilir. İçini de
    // saymak, altındaki şekilleri seçilemez hâle getirirdi.
    RECT inner = bounds;
    ::InflateRect(&inner, -static_cast<int>(tolerance),
                  -static_cast<int>(tolerance));
    RECT outer = bounds;
    ::InflateRect(&outer, static_cast<int>(tolerance),
                  static_cast<int>(tolerance));
    return ::PtInRect(&outer, point) && !::PtInRect(&inner, point);
}

void Refresh(HWND window, State& state) {
    RECT client{};
    ::GetClientRect(window, &client);
    LayoutButtons(state, client);
    ::InvalidateRect(window, nullptr, FALSE);
}

}  // namespace

int ShapeAtPoint(const State& state, POINT image) noexcept {
    const std::vector<Shape>& shapes = state.document.Shapes();
    // SONDAN BAŞA: liste çizim sırasıdır ve en son çizilen en üsttedir.
    // Baştan aramak, altta kalan bir şekli seçtirirdi.
    for (size_t i = shapes.size(); i > 0; --i) {
        if (HitsShape(shapes[i - 1], image)) {
            return static_cast<int>(i - 1);
        }
    }
    return -1;
}

bool SelectMouseDown(HWND window, State& state, POINT client) {
    if (!ToolIsSelect(state.tool)) {
        return false;
    }
    if (!::PtInRect(&state.canvas, client)) {
        return true;   // araç çubuğu dışı boş alan; seçim korunur
    }

    const POINT image = ToImage(state, client);
    state.selected = ShapeAtPoint(state, image);
    if (state.selected >= 0) {
        // GEÇMİŞE ADIM ŞİMDİ EKLENİR, her fare hareketinde değil: taşımanın
        // altmış karesi geçmişe girseydi tek bir hareket geçmişin tamamını
        // doldururdu.
        state.document.BeginEdit();
        state.movingShape = true;
        state.moveGrab = image;
        ::SetCapture(window);
    }
    Refresh(window, state);
    return true;
}

bool SelectMouseMove(HWND window, State& state, POINT client) {
    if (!state.movingShape || state.selected < 0) {
        return false;
    }
    const POINT image = ToImage(state, client);
    Shape* shape = state.document.ShapeAt(static_cast<size_t>(state.selected));
    if (shape == nullptr) {
        state.movingShape = false;
        return false;
    }
    shape->Offset(static_cast<int>(image.x - state.moveGrab.x),
                  static_cast<int>(image.y - state.moveGrab.y));
    state.moveGrab = image;
    Rebuild(state);
    ::InvalidateRect(window, nullptr, FALSE);
    return true;
}

bool SelectMouseUp(HWND window, State& state) {
    if (!state.movingShape) {
        return false;
    }
    state.movingShape = false;
    if (::GetCapture() == window) {
        ::ReleaseCapture();
    }
    Refresh(window, state);
    return true;
}

bool DeleteSelectedShape(HWND window, State& state) {
    if (state.selected < 0) {
        return false;
    }
    if (!state.document.RemoveShape(static_cast<size_t>(state.selected))) {
        return false;
    }
    state.selected = -1;
    DropScaleSource(state);
    Rebuild(state);
    Refresh(window, state);
    return true;
}

void RestyleSelectedShape(State& state) {
    if (state.selected < 0) {
        return;
    }
    Shape* shape = state.document.ShapeAt(static_cast<size_t>(state.selected));
    if (shape == nullptr) {
        return;
    }
    state.document.BeginEdit();
    // Yeniden okunan işaretçi ZORUNLU: BeginEdit listenin bir kopyasını
    // geçmişe iter ama vektörün kendisi yeniden tahsis edilebilir.
    shape = state.document.ShapeAt(static_cast<size_t>(state.selected));
    if (shape == nullptr) {
        return;
    }
    shape->color = state.color;
    shape->thickness = state.thickness;
    shape->filled = state.fillShapes;
    Rebuild(state);
}

void DrawSelectionFrame(HDC dc, const State& state) {
    if (state.selected < 0 || !ToolIsSelect(state.tool)) {
        return;
    }
    const std::vector<Shape>& shapes = state.document.Shapes();
    if (static_cast<size_t>(state.selected) >= shapes.size()) {
        return;
    }

    RECT bounds = ToClientRect(state, shapes[static_cast<size_t>(state.selected)].Bounds());
    ::InflateRect(&bounds, Scale(4, state.dpi), Scale(4, state.dpi));

    const HPEN pen = ::CreatePen(PS_DOT, 1, RGB(255, 255, 255));
    if (pen != nullptr) {
        const HGDIOBJ oldPen = ::SelectObject(dc, pen);
        const HGDIOBJ oldBrush = ::SelectObject(dc, ::GetStockObject(NULL_BRUSH));
        // XOR: seçim çerçevesi hem koyu hem açık görüntünün üstünde görünür
        // olmalı ve sabit bir renk ikisinden birinde kaybolurdu.
        const int oldRop = ::SetROP2(dc, R2_XORPEN);
        ::Rectangle(dc, bounds.left, bounds.top, bounds.right, bounds.bottom);
        ::SetROP2(dc, oldRop);
        ::SelectObject(dc, oldBrush);
        ::SelectObject(dc, oldPen);
        ::DeleteObject(pen);
    }

    // Köşe tutamakları: çerçeve tek başına "bu seçili" demiyor, "burada bir
    // şey var" diyor. Dolu kareler seçimi işaretlemenin bilinen yolu.
    const int handle = Scale(3, state.dpi);
    const POINT corners[4] = {{bounds.left, bounds.top},
                              {bounds.right, bounds.top},
                              {bounds.left, bounds.bottom},
                              {bounds.right, bounds.bottom}};
    for (const POINT& corner : corners) {
        FillRectColor(dc,
                      RECT{corner.x - handle, corner.y - handle,
                           corner.x + handle, corner.y + handle},
                      RGB(255, 255, 255));
        FrameRectColor(dc,
                       RECT{corner.x - handle, corner.y - handle,
                            corner.x + handle, corner.y + handle},
                       1, RGB(24, 24, 27));
    }
}

}  // namespace editor
}  // namespace crisp
