// Annotation.h — İşaretleme belgesi: şekiller ve geri al/yinele.
//
// PENCERE YOK, GDI YOK. Belge bir şekil listesi ve bir geçmiş yığınıdır;
// çizim EditorRender'ın, etkileşim EditorWindow'un işi. Bu ayrım olmadan
// "geri al iki adım atlıyor" gibi bir hatayı sınamak, pencereyi açıp fareyle
// on kez tıklamayı gerektirirdi.
#pragma once

#include "Capture.h"

#include <string>
#include <vector>

#include <windows.h>

namespace crisp {

enum class ToolKind {
    Select,
    Arrow,
    Rectangle,
    Ellipse,
    Pen,
    Highlighter,
    Text,
    StepNumber,
    Blur,
    Mosaic,
};

// Araç bir sürükleme mi bekliyor yoksa tek tıkla mı yerleşiyor?
[[nodiscard]] bool ToolUsesDrag(ToolKind kind) noexcept;

// Araç serbest çizim mi (nokta dizisi biriktirir)?
[[nodiscard]] bool ToolIsFreehand(ToolKind kind) noexcept;

// Araç bir bölge efekti mi (altındaki pikselleri değiştirir, üstüne çizmez)?
[[nodiscard]] bool ToolIsEffect(ToolKind kind) noexcept;

struct Shape {
    ToolKind kind = ToolKind::Arrow;
    POINT start{};                 // sürükleme başlangıcı
    POINT end{};                   // sürükleme sonu
    std::vector<POINT> points;     // Pen/Highlighter
    std::wstring text;             // Text
    COLORREF color = RGB(255, 59, 48);
    int thickness = 3;
    int stepNumber = 0;            // StepNumber

    // Normalleştirilmiş sınırlayıcı dikdörtgen; sürükleme yönünden bağımsız.
    [[nodiscard]] RECT Bounds() const noexcept;
};

// Geri al/yinele için tutulan tam durum.
//
// ANLIK GÖRÜNTÜ, KOMUT DEĞİL: her şekil birkaç yüz bayt ve bir ekran
// görüntüsünde onlarcası olur. Komut deseni (her işlem için ileri/geri
// çiftleri) burada yalnızca hata yüzeyi eklerdi; listenin kopyası hem basit
// hem yanlış olamaz.
struct DocumentState {
    std::vector<Shape> shapes;
    int nextStepNumber = 1;
};

class Document {
public:
    Document() = default;

    // --- Düzenleme ----------------------------------------------------------
    void AddShape(Shape shape);
    void Clear();

    // --- Geçmiş -------------------------------------------------------------
    [[nodiscard]] bool CanUndo() const noexcept { return !m_undo.empty(); }
    [[nodiscard]] bool CanRedo() const noexcept { return !m_redo.empty(); }
    bool Undo();
    bool Redo();

    [[nodiscard]] const std::vector<Shape>& Shapes() const noexcept {
        return m_state.shapes;
    }
    [[nodiscard]] bool empty() const noexcept { return m_state.shapes.empty(); }
    [[nodiscard]] int NextStepNumber() const noexcept {
        return m_state.nextStepNumber;
    }

    // Geçmişte tutulacak en fazla adım. Sınırsız bir geçmiş, uzun bir oturumda
    // fark edilmeden büyür.
    static constexpr size_t kMaxHistory = 64;

private:
    void PushUndo();

    DocumentState m_state;
    std::vector<DocumentState> m_undo;
    std::vector<DocumentState> m_redo;
};

}  // namespace crisp
