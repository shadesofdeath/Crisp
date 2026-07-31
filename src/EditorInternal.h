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
#include "Settings.h"

#include <vector>

#include <windows.h>

namespace crisp {
namespace editor {

// Tasarım ölçüleri (96 DPI mantıksal piksel).
inline constexpr int kToolbarHeight = 52;
inline constexpr int kButtonSide = 38;
inline constexpr int kButtonGap = 4;
inline constexpr int kGroupGap = 14;
inline constexpr int kSwatchSide = 22;

enum class ButtonKind { Tool, Color, Thickness, Action, Separator };

enum ActionId {
    kActionRotateLeft = 1,
    kActionRotateRight,
    kActionScale,
    kActionUndo,
    kActionRedo,
    kActionClear,
    kActionCopy,
    kActionSave,
    kActionClose,
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

    RECT canvas{};
    double scale = 1.0;

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

// Şekilleri tabana uygulayıp state.image'i tazeler. Çizim her seferinde
// ORİJİNALDEN başlar: üst üste boyamak, geri alınan bir şeklin izini bırakırdı.
void Rebuild(State& state);

[[nodiscard]] POINT ToImage(const State& state, POINT client) noexcept;
[[nodiscard]] int ButtonAt(const State& state, POINT client) noexcept;

}  // namespace editor
}  // namespace crisp
