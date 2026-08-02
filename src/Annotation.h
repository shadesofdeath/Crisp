// Annotation.h — İşaretleme belgesi: şekiller ve geri al/yinele.
//
// PENCERE YOK, GDI YOK. Belge bir şekil listesi ve bir geçmiş yığınıdır;
// çizim EditorRender'ın, etkileşim EditorWindow'un işi. Bu ayrım olmadan
// "geri al iki adım atlıyor" gibi bir hatayı sınamak, pencereyi açıp fareyle
// on kez tıklamayı gerektirirdi.
#pragma once

#include "Capture.h"

#include <memory>
#include <string>
#include <vector>

#include <windows.h>

namespace crisp {

enum class ToolKind {
    Select,
    Arrow,
    Line,
    Rectangle,
    Ellipse,
    Pen,
    Highlighter,
    Text,
    StepNumber,
    Blur,
    Mosaic,
    Crop,
};

// Araç, eklenmiş şekilleri SEÇİP düzenliyor mu?
[[nodiscard]] bool ToolIsSelect(ToolKind kind) noexcept;

// Araç bir sürükleme mi bekliyor yoksa tek tıkla mı yerleşiyor?
[[nodiscard]] bool ToolUsesDrag(ToolKind kind) noexcept;

// Araç serbest çizim mi (nokta dizisi biriktirir)?
[[nodiscard]] bool ToolIsFreehand(ToolKind kind) noexcept;

// Araç bir bölge efekti mi (altındaki pikselleri değiştirir, üstüne çizmez)?
[[nodiscard]] bool ToolIsEffect(ToolKind kind) noexcept;

// Araç, şekil eklemek yerine GÖRÜNTÜNÜN KENDİSİNİ mi değiştiriyor?
// Bu araçlar belgeye şekil koymaz; sürükleme bittiğinde tabanı yeniler.
[[nodiscard]] bool ToolIsImageOp(ToolKind kind) noexcept;

struct Shape {
    ToolKind kind = ToolKind::Arrow;
    POINT start{};                 // sürükleme başlangıcı
    POINT end{};                   // sürükleme sonu
    std::vector<POINT> points;     // Pen/Highlighter
    std::wstring text;             // Text
    COLORREF color = RGB(255, 59, 48);
    int thickness = 3;
    int stepNumber = 0;            // StepNumber

    // Dikdörtgen ve elips İÇİ DOLU mu? Varsayılan hayır: dolu bir şekil
    // altındaki ekran görüntüsünü gizler ve işaretlemenin amacı göstermek.
    bool filled = false;

    // Bulanıklık ve mozaik şiddeti, yüzde (100 = otomatik).
    //
    // AYARDA DEĞİL ŞEKİLDE DURUYOR: geri alma şekil listesini geri sarar ve
    // şiddet ayardan okunsaydı, kullanıcı ayarı değiştirdikten sonra ESKİ
    // bulanıklıklar da yeni şiddetle yeniden çizilirdi.
    int strength = 100;

    // Şekli verilen kadar öteler (seçim aracıyla taşıma).
    void Offset(int dx, int dy) noexcept;

    // Şekli `from` dikdörtgeninden `to` dikdörtgenine oranlayarak taşır ve
    // ölçekler (seçim aracıyla boyutlandırma).
    //
    // TEK BİR ORANLAMA HER ŞEKLE YETİYOR. Ok ve çizgi iki uçtan, dikdörtgen ve
    // elips köşelerden, serbest çizim ise yüzlerce noktadan oluşuyor; üçüne
    // ayrı boyutlandırma yazmak üç ayrı hata yüzeyi demekti. Her koordinatı
    // aynı oranla eşlemek üçünü birden doğru yapıyor: bir okun ucu da, bir
    // kavisin ortası da olması gereken yerde kalıyor.
    //
    // KALINLIK ÖLÇEKLENMEZ. Kullanıcı kalınlığı ayrıca seçiyor; iki katına
    // büyütülen bir kutunun çizgisinin de kalınlaşması, seçmediği bir şeyin
    // değişmesi olurdu.
    //
    // `from` bir eksende sıfır genişlikteyse o eksende yalnızca ötelenir:
    // sıfıra bölmek yerine şeklin o yöndeki ölçüsü korunur.
    void ScaleTo(const RECT& from, const RECT& to) noexcept;

    // Şeklin boyutlandırılmasının bir anlamı var mı? Metin ve rozet TEK
    // NOKTADIR; sınırları sıfır ölçülüdür ve onları "boyutlandırmak" yalnızca
    // taşımak olurdu — ki zaten taşınabiliyorlar.
    [[nodiscard]] bool Resizable() const noexcept;

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
    // Şekillerin üzerine çizildiği taban görüntü.
    //
    // PAYLAŞILAN İŞARETÇİ: geçmişteki her adım kendi kopyasını tutsaydı, 4K
    // bir yakalamada 64 adım × 14 MB = yarım gigabayttan fazla olurdu. Şekil
    // eklemek tabanı değiştirmez, bu yüzden adımların çoğu aynı görüntüyü
    // paylaşır; yalnızca kırpma/döndürme/ölçekleme yeni bir taban üretir.
    std::shared_ptr<const Image> base;
    std::vector<Shape> shapes;
    int nextStepNumber = 1;
};

class Document {
public:
    Document() = default;

    // --- Düzenleme ----------------------------------------------------------
    void AddShape(Shape shape);
    void Clear();

    // Başlangıç tabanı; geçmişe adım eklemez.
    void SetBase(std::shared_ptr<const Image> base);

    // --- Var olan şekli düzenleme (seçim aracı) -----------------------------
    // Geçmişe BİR adım ekler ve sonrasında ShapeAt ile doğrudan yazılabilir.
    //
    // NEDEN İKİ ADIMLI: sürükleyerek taşımanın her karesi geçmişe girseydi tek
    // bir hareket, altmış dört adımlık geçmişin tamamını doldururdu. Çağıran
    // sürükleme BAŞLARKEN bir kez BeginEdit çağırır.
    void BeginEdit();
    [[nodiscard]] Shape* ShapeAt(size_t index) noexcept;
    bool RemoveShape(size_t index);

    // Görüntünün kendisini değiştiren bir işlem uygular (kırpma, döndürme,
    // ölçekleme).
    //
    // ŞEKİLLER TEMİZLENİR ve bu bilinçlidir: şekil koordinatları eski
    // görüntünün uzayındadır. Kırptıktan sonra onları korumak, okların
    // görüntünün dışına kaymış hâlde durması demek olurdu. Çağıran, işlemden
    // ÖNCE şekilleri tabana pişirir; geri alma pişmemiş hâli geri getirir.
    void ApplyImageOp(std::shared_ptr<const Image> newBase);

    [[nodiscard]] const std::shared_ptr<const Image>& Base() const noexcept {
        return m_state.base;
    }

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
