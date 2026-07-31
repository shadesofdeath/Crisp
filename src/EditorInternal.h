// EditorInternal.h — Düzenleyicinin İÇ paylaşımı. Dışarıya açık değildir;
// yalnızca EditorWindow.cpp (yerleşim ve çizim) ile EditorInput.cpp (mesajlar)
// arasında paylaşılır.
//
// AYRI DOSYA OLMASININ SEBEBİ boyut: yerleşim, çizim, girdi ve pencere ömrü
// tek bir .cpp'de 400 satırı fazlasıyla aşardı (ev kuralı §9). Ayrım işlevsel:
// bir dosya NE göründüğünü, diğeri NE OLDUĞUNU anlatır.
#pragma once

#include "AlphaLayer.h"
#include "Annotation.h"
#include "Capture.h"
#include "EditorWindow.h"
#include "OcrLayout.h"
#include "Settings.h"

#include <vector>

#include <windows.h>

namespace crisp {
namespace editor {

// Tasarım ölçüleri (96 DPI mantıksal piksel).
inline constexpr int kToolbarHeight = 52;
inline constexpr int kStatusHeight = 26;
inline constexpr int kButtonSide = 38;
inline constexpr int kButtonGap = 4;
inline constexpr int kGroupGap = 14;
inline constexpr int kSwatchSide = 22;

// Yakınlaştırma sınırları. Alt sınır büyük bir yakalamanın tümünü görmeye,
// üst sınır tek pikseli ayırt etmeye yeter; ötesi kullanışlı değil, yalnızca
// kaybolmayı kolaylaştırır.
inline constexpr double kMinZoom = 0.1;
inline constexpr double kMaxZoom = 8.0;

enum class ButtonKind { Tool, Color, Thickness, Action, Separator };

enum ActionId {
    kActionZoomOut = 1,
    kActionZoomIn,
    kActionZoomFit,
    kActionOcr,
    kActionRotateLeft,
    kActionRotateRight,
    kActionScale,
    kActionUndo,
    kActionRedo,
    kActionClear,
    kActionCopy,
    kActionSave,
    kActionClose,
};

// Metin tanıma kipinin durumu.
//
// AYRI BİR KİP, AYRI BİR ARAÇ DEĞİL: metin seçmek çizim yapmaya benzemez —
// sürükleme şekil üretmez, imleç I işaretine döner, sağ tık menüsü farklıdır.
// Araç listesine sıkıştırmak, her çizim yolunun içine "ama OCR açıksa" diye
// bir dal eklemek olurdu.
struct OcrMode {
    bool active = false;
    bool attempted = false;   // tanıma çalıştırıldı mı (boş sonuç da sayılır)
    OcrLayout layout;
    int anchor = -1;          // sürüklemenin başladığı kelime
    int cursor = -1;          // sürüklemenin bittiği kelime
    bool selecting = false;

    [[nodiscard]] bool HasSelection() const noexcept {
        return anchor >= 0 && cursor >= 0;
    }
    void Clear() noexcept {
        anchor = -1;
        cursor = -1;
        selecting = false;
    }
};

struct Button {
    ButtonKind kind = ButtonKind::Tool;
    ToolKind tool = ToolKind::Arrow;
    COLORREF color = 0;
    int thickness = 0;
    int action = 0;
    RECT bounds{};
    bool enabled = true;
};

inline constexpr COLORREF kPalette[] = {
    RGB(255, 59, 48), RGB(255, 149, 0),  RGB(255, 214, 10), RGB(52, 199, 89),
    RGB(10, 132, 255), RGB(24, 24, 27), RGB(255, 255, 255),
};

struct State {
    // İleti kutusu ve alt pencereler için gerekli; pencere sınıfını kaydeden
    // örnek tutamacı.
    HINSTANCE instance = nullptr;
    Image* image = nullptr;   // çağıranın görüntüsü; dönüşte sonuç burada
    Image original;           // hiç dokunulmamış taban; her boyamada kopyalanır
    Document document;
    Settings settings;

    ToolKind tool = ToolKind::Arrow;
    COLORREF color = kPalette[0];
    int thickness = 3;

    std::vector<Button> buttons;
    unsigned dpi = 96;

    bool dragging = false;
    Shape draft;

    bool typing = false;
    Shape textDraft;

    int hoverButton = -1;
    EditorResult result{};

    RECT canvas{};     // görüntünün istemci koordinatındaki yeri
    RECT viewport{};   // tuvalin görünebileceği alan (araç çubuğu ile durum
                       // çubuğu arasında kalan bölge)
    double scale = 1.0;

    // YAKINLAŞTIRMA. fitToWindow açıkken ölçek pencereye göre hesaplanır ve
    // pencere boyutlandıkça izler; kullanıcı bir kez yakınlaştırdığında kapanır,
    // çünkü o andan sonra ölçeği kullanıcı seçmiştir.
    bool fitToWindow = true;
    double zoom = 1.0;
    POINT pan{};       // ortalanmış konuma eklenen kaydırma (istemci pikseli)
    bool panning = false;
    POINT panGrab{};   // kaydırmanın başladığı fare noktası
    POINT panStart{};  // kaydırmanın başındaki pan değeri

    // İmlecin görüntü koordinatı; durum çubuğunda gösterilir.
    POINT hoverImage{-1, -1};

    OcrMode ocr;

    // Araç çubuğunun yuvarlatılmış zeminleri tek bir alfa katmanına çizilip
    // bir kerede karıştırılır; şekil başına AlphaBlend on beş blit demek olurdu.
    AlphaLayer chrome;

    // İpucu balonu: imleç bir düğmede beklediğinde açılır.
    int tooltipButton = -1;
    bool tooltipVisible = false;
};

[[nodiscard]] int Scale(int value, unsigned dpi) noexcept;

// Araç çubuğunun tüm düğmeleri için gereken en küçük genişlik.
// SABİT BİR SAYI DEĞİL: araç ya da renk eklendiğinde elle güncellenmesi
// gereken bir tahmin, bir sonraki eklemede unutulur ve düğmeler üst üste
// biner — nitekim ilk denemede tam olarak bu oldu.
[[nodiscard]] int RequiredToolbarWidth(unsigned dpi) noexcept;

void LayoutButtons(State& state, const RECT& client);
void LayoutCanvas(State& state, const RECT& client);
void Paint(HWND window, State& state);

// --- Araç çubuğu çizimi (EditorChrome.cpp) ----------------------------------
void FillRectColor(HDC dc, const RECT& r, COLORREF color);
void FrameRectColor(HDC dc, const RECT& r, int thickness, COLORREF color);
[[nodiscard]] bool IsSelected(const State& state, const Button& button) noexcept;
void DrawButtonBackground(AlphaLayer& layer, const State& state,
                          const Button& button, bool selected, bool hovered);
void DrawButtonGlyph(HDC dc, const State& state, const Button& button,
                     bool selected);

// --- Eylemler ve çizim adımları (EditorActions.cpp) -------------------------
void CommitTextDraft(State& state);
void ApplyAction(HWND window, State& state, int action);
void BeginDraw(HWND window, State& state, POINT client);
void UpdateDraw(HWND window, State& state, POINT client);
void EndDraw(HWND window, State& state);

// Şekilleri tabana uygulayıp state.image'i tazeler. Çizim her seferinde
// ORİJİNALDEN başlar: üst üste boyamak, geri alınan bir şeklin izini bırakırdı.
void Rebuild(State& state);

[[nodiscard]] POINT ToImage(const State& state, POINT client) noexcept;
[[nodiscard]] POINT ToClient(const State& state, POINT image) noexcept;
[[nodiscard]] RECT ToClientRect(const State& state, const RECT& image) noexcept;
[[nodiscard]] int ButtonAt(const State& state, POINT client) noexcept;

// --- Yakınlaştırma (EditorLayout.cpp) ---------------------------------------
// İmlecin ALTINDAKİ PİKSELİ SABİT TUTAR: ekranın ortasına yakınlaştırmak,
// kullanıcının baktığı yeri her adımda kaybetmesi demek olurdu.
void ZoomAt(HWND window, State& state, double factor, POINT anchor);
void ZoomToFit(HWND window, State& state);
void ZoomToActual(HWND window, State& state);

// Görüntüyü görünür alandan tamamen çıkaracak kaydırmaları engeller.
void ClampPan(State& state);

// --- Metin tanıma (EditorOcr.cpp) -------------------------------------------
// Kipi açar; ilk açılışta tanımayı çalıştırır. Tanıma yoksa uyarır ve kip
// açılmaz.
void ToggleOcrMode(HWND window, State& state);

// OCR kipindeki fare ve klavye. İşlendiyse true döner ve çağıran normal
// çizim yolunu ATLAR.
[[nodiscard]] bool OcrMouseDown(HWND window, State& state, POINT client);
[[nodiscard]] bool OcrMouseMove(HWND window, State& state, POINT client);
[[nodiscard]] bool OcrMouseUp(HWND window, State& state);
[[nodiscard]] bool OcrKeyDown(HWND window, State& state, unsigned key, bool control);
void OcrShowMenu(HWND window, State& state, POINT screen);
void OcrCopySelection(HWND window, const State& state);

// Tanınan kelimeleri ve seçimi tuvalin üstüne çizer.
void DrawOcrOverlay(HDC dc, const State& state);

}  // namespace editor
}  // namespace crisp
