// HistoryInternal.h — Geçmiş penceresinin İÇ paylaşımı.
//
// HistoryWindow.cpp (yerleşim + çizim) ile HistoryInput.cpp (mesajlar, menü,
// pencere ömrü) arasında paylaşılır; dışarıya açık değildir. Ayrım, ev
// kuralının 400 satır sınırından doğdu ve işlevsel: bir dosya NE göründüğünü,
// diğeri NE OLDUĞUNU anlatır.
#pragma once

#include "Capture.h"
#include "History.h"
#include "HistoryWindow.h"

#include <vector>

#include <windows.h>

namespace crisp {
namespace history {

// Tasarım ölçüleri (96 DPI mantıksal piksel).
inline constexpr int kPad = 20;
inline constexpr int kGap = 14;
inline constexpr int kTileWidth = 184;

// Kutucuk bir KARTTIR: üstte küçük resim alanı, altında iki satır etiket.
// kInset kartın iç kenar boşluğu; küçük resim bu kadar içeriden başlar, bu
// yüzden hiçbir zaman kartın kenarına dayanmaz.
inline constexpr int kInset = 10;
inline constexpr int kThumbHeight = 120;
inline constexpr int kLabelHeight = 48;
inline constexpr int kTileHeight = kThumbHeight + kLabelHeight;
inline constexpr int kHeaderHeight = 46;

struct Tile {
    HistoryEntry entry;
    Image thumbnail;   // ölçeklenmiş; tam görüntü diskte kalır
    int fullWidth = 0;
    int fullHeight = 0;
    RECT bounds{};     // istemci koordinatı, kaydırma UYGULANMIŞ
};

struct State {
    HINSTANCE instance = nullptr;
    HistoryStore* store = nullptr;
    std::vector<Tile> tiles;

    int selected = -1;
    int hovered = -1;
    int scroll = 0;      // piksel cinsinden dikey kaydırma
    int contentHeight = 0;
    int columns = 1;

    unsigned dpi = 96;
    HistoryResult result;
};

[[nodiscard]] int Scale(int value, unsigned dpi) noexcept;

// Diskteki kayıtları okuyup küçük resimleri üretir. Seçili öğe korunmaya
// çalışılır; silinmişse en yakın komşuya kayar.
void ReloadTiles(HWND window, State& state);

void Layout(HWND window, State& state);
void Paint(HWND window, State& state);

// Kaydırma çubuğunu içerik yüksekliğine göre günceller ve kaydırmayı sınırlar.
void UpdateScrollBar(HWND window, State& state);

// --- Eylemler (HistoryActions.cpp) ------------------------------------------
[[nodiscard]] const Tile* Selected(const State& state) noexcept;
void EnsureVisible(HWND window, State& state);
void MoveSelection(HWND window, State& state, int delta);
void EditSelected(HWND window, State& state);
void CopySelected(HWND window, const State& state);
void RevealSelected(const State& state);
void DeleteSelected(HWND window, State& state);
void ClearAll(HWND window, State& state);
void ShowContextMenu(HWND window, State& state, POINT screen);

[[nodiscard]] int TileAt(const State& state, POINT client) noexcept;

}  // namespace history
}  // namespace crisp
