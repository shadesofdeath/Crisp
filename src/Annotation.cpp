// Annotation.cpp — bkz. Annotation.h.
#include "Annotation.h"

#include "Geometry.h"

#include <utility>

namespace crisp {

bool ToolUsesDrag(ToolKind kind) noexcept {
    switch (kind) {
        case ToolKind::Text:
        case ToolKind::StepNumber:
        case ToolKind::Select:
            return false;
        default:
            return true;
    }
}

bool ToolIsFreehand(ToolKind kind) noexcept {
    return kind == ToolKind::Pen || kind == ToolKind::Highlighter;
}

bool ToolIsEffect(ToolKind kind) noexcept {
    return kind == ToolKind::Blur || kind == ToolKind::Mosaic;
}

bool ToolIsImageOp(ToolKind kind) noexcept { return kind == ToolKind::Crop; }

bool ToolIsSelect(ToolKind kind) noexcept { return kind == ToolKind::Select; }

void Shape::Offset(int dx, int dy) noexcept {
    start.x += dx;
    start.y += dy;
    end.x += dx;
    end.y += dy;
    for (POINT& p : points) {
        p.x += dx;
        p.y += dy;
    }
}

namespace {

// Tek bir koordinatı bir aralıktan diğerine eşler.
//
// TAM SAYI ARİTMETİĞİ, KAYAR NOKTA DEĞİL: koordinatlar piksel ve sonuç da
// piksel olacak. Kayar noktaya geçip geri dönmek, aynı şekli aynı yere iki kez
// boyutlandırmanın bir piksel oynamasına yol açardı.
[[nodiscard]] LONG MapSpan(LONG value, LONG fromStart, LONG fromLength,
                           LONG toStart, LONG toLength) noexcept {
    if (fromLength == 0) {
        // Sıfır genişlikli kaynak: oran tanımsız. Şeklin o eksendeki ölçüsü
        // korunur, yalnızca ötelenir.
        return value - fromStart + toStart;
    }
    return toStart + ::MulDiv(static_cast<int>(value - fromStart),
                              static_cast<int>(toLength),
                              static_cast<int>(fromLength));
}

}  // namespace

void Shape::ScaleTo(const RECT& from, const RECT& to) noexcept {
    const LONG fromWidth = from.right - from.left;
    const LONG fromHeight = from.bottom - from.top;
    const LONG toWidth = to.right - to.left;
    const LONG toHeight = to.bottom - to.top;

    const auto map = [&](POINT& p) {
        p.x = MapSpan(p.x, from.left, fromWidth, to.left, toWidth);
        p.y = MapSpan(p.y, from.top, fromHeight, to.top, toHeight);
    };

    map(start);
    map(end);
    for (POINT& p : points) {
        map(p);
    }
}

bool Shape::Resizable() const noexcept {
    return kind != ToolKind::Text && kind != ToolKind::StepNumber;
}

RECT Shape::Bounds() const noexcept {
    if (!points.empty()) {
        // Serbest çizimde sınır noktaların tamamını kapsar; start/end yalnızca
        // ilk ve son noktadır ve arada dışarı taşan bir kavis olabilir.
        RECT bounds{points.front().x, points.front().y, points.front().x,
                    points.front().y};
        for (const POINT& p : points) {
            bounds.left = p.x < bounds.left ? p.x : bounds.left;
            bounds.top = p.y < bounds.top ? p.y : bounds.top;
            bounds.right = p.x > bounds.right ? p.x : bounds.right;
            bounds.bottom = p.y > bounds.bottom ? p.y : bounds.bottom;
        }
        return bounds;
    }
    return geom::FromCorners(start, end);
}

void Document::PushUndo() {
    m_undo.push_back(m_state);
    if (m_undo.size() > kMaxHistory) {
        // En eskisi düşer. erase(begin()) kopyalama yapar ama geçmiş sığ ve
        // bu yol adımda bir kez çalışır.
        m_undo.erase(m_undo.begin());
    }
    // YENİ BİR DÜZENLEME YİNELEME ZİNCİRİNİ KESER: kullanıcı geri alıp sonra
    // başka bir şey çizdiyse, eski ileri yol artık ulaşılamaz bir daldır.
    m_redo.clear();
}

void Document::AddShape(Shape shape) {
    PushUndo();

    if (shape.kind == ToolKind::StepNumber) {
        shape.stepNumber = m_state.nextStepNumber;
        ++m_state.nextStepNumber;
    }
    m_state.shapes.push_back(std::move(shape));
}

void Document::SetBase(std::shared_ptr<const Image> base) {
    m_state.base = std::move(base);
}

void Document::BeginEdit() { PushUndo(); }

Shape* Document::ShapeAt(size_t index) noexcept {
    if (index >= m_state.shapes.size()) {
        return nullptr;
    }
    return &m_state.shapes[index];
}

bool Document::RemoveShape(size_t index) {
    if (index >= m_state.shapes.size()) {
        return false;
    }
    PushUndo();
    m_state.shapes.erase(m_state.shapes.begin() +
                         static_cast<std::ptrdiff_t>(index));
    return true;
}

void Document::ApplyImageOp(std::shared_ptr<const Image> newBase) {
    if (!newBase) {
        return;
    }
    PushUndo();
    m_state.base = std::move(newBase);
    m_state.shapes.clear();
    m_state.nextStepNumber = 1;
}

void Document::Clear() {
    if (m_state.shapes.empty()) {
        return;   // boş belgeyi temizlemek geçmişe boş bir adım eklemesin
    }
    PushUndo();
    m_state.shapes.clear();
    m_state.nextStepNumber = 1;
}

bool Document::Undo() {
    if (m_undo.empty()) {
        return false;
    }
    m_redo.push_back(m_state);
    m_state = m_undo.back();
    m_undo.pop_back();
    return true;
}

bool Document::Redo() {
    if (m_redo.empty()) {
        return false;
    }
    m_undo.push_back(m_state);
    m_state = m_redo.back();
    m_redo.pop_back();
    return true;
}

}  // namespace crisp
