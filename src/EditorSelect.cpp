// EditorSelect.cpp — Eklenmiş bir şekli seçme, taşıma, silme ve yeniden
// biçimlendirme.
//
// NEDEN VAR: bir ok yanlış yere çizildiğinde tek çare geri almaktı — ve geri
// alma ondan sonra çizilen her şeyi de götürüyordu. On şekil koyup dokuzuncuyu
// beş piksel kaydırmak isteyen kullanıcının önünde hiçbir yol yoktu.
//
// BOYUTLANDIRMA DA VAR ARTIK. Bir süre yoktu ve buradaki gerekçe "her şekil
// tipinin kendi tutamak mantığı var, taşıma tek başına yetiyor" diyordu — ama
// seçim çerçevesinin dört köşesine o sırada da tutamak ÇİZİLİYORDU. Arayüz
// tutmadığı bir söz veriyordu, tıpkı kaplamadaki seçimin sekiz tutamağı gibi.
//
// Gerekçe teknik olarak da yanlış çıktı: tek bir oranlama (`Shape::ScaleTo`)
// oku, dikdörtgeni ve serbest çizimi birden doğru boyutlandırıyor, çünkü üçü de
// sonuçta bir koordinat listesi. "Kullanılmayan sekiz dal" hiç gerekmedi.
//
// Tutamakların yeri ve isabet testi `geom::HandleRects` ile
// `geom::HitTestSelection`ten geliyor — kaplamadaki seçimle AYNI fonksiyonlar.
// İkinci bir kopya, iki yerde ayrı ayrı bozulabilen iki tutamak mantığı demekti.
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

// Tutamağın İSTEMCİ koordinatındaki kavrama ölçüsü.
//
// İSABET TESTİ İSTEMCİDE YAPILIR, GÖRÜNTÜDE DEĞİL. Tutamak ekranda sabit
// büyüklükte çizilir; %25'e küçültülmüş bir görüntüde görüntü koordinatına
// çevrilmiş bir kavrama alanı dört katına çıkar ve şeklin yanındaki boşluğu da
// yutardı.
[[nodiscard]] LONG HandleGrabSize(const State& state) noexcept {
    return Scale(13, state.dpi);
}

// Seçili şeklin çerçevesi, istemci koordinatında. Çizimle AYNI hesap:
// `DrawSelectionFrame` de bu payı veriyor.
[[nodiscard]] RECT SelectionFrame(const State& state, const Shape& shape) {
    RECT bounds = ToClientRect(state, shape.Bounds());
    ::InflateRect(&bounds, Scale(4, state.dpi), Scale(4, state.dpi));
    return bounds;
}

}  // namespace

geom::Grab ShapeHandleAt(const State& state, POINT client) noexcept {
    if (state.selected < 0 || !ToolIsSelect(state.tool)) {
        return geom::Grab::None;
    }
    const std::vector<Shape>& shapes = state.document.Shapes();
    if (static_cast<size_t>(state.selected) >= shapes.size()) {
        return geom::Grab::None;
    }
    const Shape& shape = shapes[static_cast<size_t>(state.selected)];
    if (!shape.Resizable()) {
        return geom::Grab::None;
    }

    // YALNIZCA TUTAMAKLAR, İÇİ DEĞİL. `HitTestSelection` seçimin içini
    // `Grab::Move` diye bildiriyor; burada taşımayı şeklin kendi isabet testi
    // yapıyor ve çerçevenin içindeki boşluğu taşıma alanı saymak, altındaki
    // şekilleri seçilemez hâle getirirdi.
    const geom::Grab hit = geom::HitTestSelection(SelectionFrame(state, shape),
                                                  client, HandleGrabSize(state));
    return hit == geom::Grab::Move ? geom::Grab::None : hit;
}

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

    // TUTAMAK ÖNCE SORULUR. Tutamaklar çerçevenin DIŞINA taşıyor ve altlarında
    // başka bir şekil olabilir; şekil isabet testi önce gelseydi, bir kutunun
    // köşesini tutmak arkasındaki oku seçerdi.
    if (const geom::Grab handle = ShapeHandleAt(state, client);
        handle != geom::Grab::None) {
        const std::vector<Shape>& shapes = state.document.Shapes();
        state.document.BeginEdit();
        state.shapeGrab = handle;
        state.shapeOrigin = shapes[static_cast<size_t>(state.selected)].Bounds();
        ::SetCapture(window);
        Refresh(window, state);
        return true;
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
    if (state.shapeGrab != geom::Grab::None && state.selected >= 0) {
        Shape* shape = state.document.ShapeAt(static_cast<size_t>(state.selected));
        if (shape == nullptr) {
            state.shapeGrab = geom::Grab::None;
            return false;
        }

        // SINIR GÖRÜNTÜNÜN KENDİSİ. Şekil tuvalin dışına taşırılabilseydi
        // kaydedilen dosyada görünmeyen bir parçası olurdu.
        const std::shared_ptr<const Image>& base = state.document.Base();
        const RECT canvas{0, 0, base ? base->Width() : 0,
                          base ? base->Height() : 0};

        // EN KÜÇÜK KENAR BİR PİKSEL DEĞİL: sıfıra indirilen bir şekil geri
        // büyütülemez, çünkü tutamakları da üst üste biner. Sekiz piksel,
        // kullanıcının yanlışlıkla yok edemeyeceği en küçük ölçü.
        const RECT resized =
            geom::ResizeByGrab(state.shapeOrigin, state.shapeGrab,
                               ToImage(state, client), 8, canvas);

        // ORANLAMA HER SEFERİNDE BAŞLANGIÇ SINIRINDAN YAPILIR. Şeklin o anki
        // sınırından ölçeklemek, yuvarlama hatalarını üst üste bindirir ve
        // sürükleme uzadıkça şekil kayardı.
        shape->ScaleTo(shape->Bounds(), resized);
        Rebuild(state);
        ::InvalidateRect(window, nullptr, FALSE);
        return true;
    }

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
    if (state.shapeGrab != geom::Grab::None) {
        state.shapeGrab = geom::Grab::None;
        if (::GetCapture() == window) {
            ::ReleaseCapture();
        }
        Refresh(window, state);
        return true;
    }
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

HCURSOR SelectCursor(HWND window, const State& state) {
    POINT cursor{};
    if (::GetCursorPos(&cursor) == FALSE ||
        ::ScreenToClient(window, &cursor) == FALSE) {
        return nullptr;
    }

    // Köşegen tutamaklar köşegen ok, kenar tutamakları tek eksenli ok. Windows
    // imleçleri bunlar için hazır geliyor ve kullanıcının başka programlardan
    // bildiği şey de bu.
    switch (ShapeHandleAt(state, cursor)) {
        case geom::Grab::NW:
        case geom::Grab::SE: return ::LoadCursorW(nullptr, IDC_SIZENWSE);
        case geom::Grab::NE:
        case geom::Grab::SW: return ::LoadCursorW(nullptr, IDC_SIZENESW);
        case geom::Grab::N:
        case geom::Grab::S:  return ::LoadCursorW(nullptr, IDC_SIZENS);
        case geom::Grab::E:
        case geom::Grab::W:  return ::LoadCursorW(nullptr, IDC_SIZEWE);
        default:             return nullptr;
    }
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

    // TUTAMAKLAR: ÇİZİLEN KÜME, TUTULAN KÜMEDİR.
    //
    // Burada elle yazılmış dört köşe vardı ve hiçbiri fareye yanıt vermiyordu;
    // yalnızca "bu şekil seçili" demenin süslü bir yoluydular. Şimdi hem yerler
    // hem sayı `geom::HandleRects`ten geliyor — isabet testiyle aynı fonksiyon —
    // ve küçük bir şekilde tutamaklar kendiliğinden dörde iniyor ya da tamamen
    // kalkıyor, üst üste binmesinler diye.
    //
    // Metin ve rozet boyutlandırılamaz; onlarda tutamak çizmek, olmayan bir
    // şeyi vaat etmek olurdu.
    if (!shapes[static_cast<size_t>(state.selected)].Resizable()) {
        return;
    }

    RECT boxes[8]{};
    geom::Grab grabs[8]{};
    const int count =
        geom::HandleRects(bounds, Scale(13, state.dpi), boxes, grabs);

    const int drawSide = Scale(7, state.dpi);
    for (int i = 0; i < count; ++i) {
        const LONG cx = boxes[i].left + geom::Width(boxes[i]) / 2;
        const LONG cy = boxes[i].top + geom::Height(boxes[i]) / 2;
        const RECT handle{cx - drawSide / 2, cy - drawSide / 2,
                          cx + drawSide / 2, cy + drawSide / 2};
        FillRectColor(dc, handle, RGB(255, 255, 255));
        FrameRectColor(dc, handle, 1, RGB(24, 24, 27));
    }
}

}  // namespace editor
}  // namespace crisp
