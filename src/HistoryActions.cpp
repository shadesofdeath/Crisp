// HistoryActions.cpp — Geçmiş penceresinin eylemleri.
//
// AYRI DOSYA: mesaj yordamıyla aynı dosyada HistoryInput.cpp 400 satırı
// aşıyordu (docs §9). Burası NE yapıldığını, HistoryInput.cpp hangi mesajla
// tetiklendiğini anlatır.
#include "HistoryInternal.h"

#include "ClipboardImage.h"
#include "Geometry.h"
#include "ImageCodec.h"
#include "Localization.h"
#include "MessageWindow.h"
#include "Util.h"
#include "resource.h"

#include <shellapi.h>

#include <string>

namespace crisp {
namespace history {
namespace {

// Bağlam menüsü komutları; menü TPM_RETURNCMD ile okunduğu için dışarı sızmaz.
enum MenuId { kMenuEdit = 1, kMenuCopy, kMenuReveal, kMenuDelete, kMenuClear };

}  // namespace


[[nodiscard]] const Tile* Selected(const State& state) noexcept {
    if (state.selected < 0 ||
        state.selected >= static_cast<int>(state.tiles.size())) {
        return nullptr;
    }
    return &state.tiles[static_cast<size_t>(state.selected)];
}

// Seçili kutucuğu görünür alana getirir. Klavyeyle gezinirken seçim ekranın
// dışına kayarsa, kullanıcı neyin seçili olduğunu göremez.
void EnsureVisible(HWND window, State& state) {
    const Tile* tile = Selected(state);
    if (tile == nullptr) {
        return;
    }

    RECT client{};
    ::GetClientRect(window, &client);
    const int header = Scale(kHeaderHeight, state.dpi);
    const int margin = Scale(kGap, state.dpi);

    if (tile->bounds.top < header + margin) {
        state.scroll -= (header + margin) - tile->bounds.top;
    } else if (tile->bounds.bottom > client.bottom - margin) {
        state.scroll += tile->bounds.bottom - (client.bottom - margin);
    } else {
        return;
    }

    Layout(window, state);
    UpdateScrollBar(window, state);
    Layout(window, state);   // kaydırma sınırlandıysa konumlar tazelensin
    ::InvalidateRect(window, nullptr, FALSE);
}

void MoveSelection(HWND window, State& state, int delta) {
    if (state.tiles.empty()) {
        return;
    }
    int next = state.selected + delta;
    if (next < 0) {
        next = 0;
    }
    if (next >= static_cast<int>(state.tiles.size())) {
        next = static_cast<int>(state.tiles.size()) - 1;
    }
    if (next == state.selected) {
        return;
    }
    state.selected = next;
    EnsureVisible(window, state);
    ::InvalidateRect(window, nullptr, FALSE);
}

void EditSelected(HWND window, State& state) {
    const Tile* tile = Selected(state);
    if (tile == nullptr) {
        return;
    }
    // TAM GÖRÜNTÜ DİSKTEN OKUNUR: bellekte tutulan yalnızca küçük resimdir ve
    // onu düzenleyiciye vermek, kullanıcının yakalamasını 176 piksele
    // düşürmek olurdu.
    Image full;
    if (!LoadPng(tile->entry.path, full)) {
        return;
    }
    state.result.choice = HistoryChoice::Edit;
    state.result.image = std::move(full);
    state.result.path = tile->entry.path;
    ::DestroyWindow(window);
}

void CopySelected(HWND window, const State& state) {
    const Tile* tile = Selected(state);
    if (tile == nullptr) {
        return;
    }
    Image full;
    if (!LoadPng(tile->entry.path, full)) {
        return;
    }
    if (!CopyImageToClipboard(full, window)) {
        LogV(L"Geçmişten panoya kopyalanamadı");
    }
}

void RevealSelected(const State& state) {
    const Tile* tile = Selected(state);
    if (tile == nullptr) {
        return;
    }
    std::wstring arguments = L"/select,\"";
    arguments += tile->entry.path;
    arguments += L'"';
    ::ShellExecuteW(nullptr, L"open", L"explorer.exe", arguments.c_str(), nullptr,
                    SW_SHOWNORMAL);
}

void DeleteSelected(HWND window, State& state) {
    const Tile* tile = Selected(state);
    if (tile == nullptr || state.store == nullptr) {
        return;
    }
    // Silinen kaydın komşusu seçili kalsın: her silmeden sonra listenin başına
    // dönmek, arka arkaya temizlik yapmayı zorlaştırırdı.
    const int fallback = state.selected;
    if (!state.store->Remove(tile->entry.path)) {
        return;
    }
    state.selected = -1;
    ReloadTiles(window, state);
    if (!state.tiles.empty()) {
        state.selected = fallback < static_cast<int>(state.tiles.size())
                             ? fallback
                             : static_cast<int>(state.tiles.size()) - 1;
    }
    ::InvalidateRect(window, nullptr, FALSE);
}

void ClearAll(HWND window, State& state) {
    if (state.store == nullptr || state.tiles.empty()) {
        return;
    }
    // GERİ ALINAMAZ bir işlem için onay: tek tıkla yirmi dört yakalamayı
    // silmek, kullanıcının kaybettiğini ancak sonradan fark etmesi olurdu.
    const MessageResult answer =
        ShowMessage(state.instance, window, Loc::Str(IDS_HISTORY_CLEAR_ASK),
                    MessageIcon::Question, MessageButtons::YesNo);
    if (answer != MessageResult::Yes) {
        return;
    }
    (void)state.store->Clear();
    state.selected = -1;
    ReloadTiles(window, state);
    ::InvalidateRect(window, nullptr, FALSE);
}

void ShowContextMenu(HWND window, State& state, POINT screen) {
    const HMENU menu = ::CreatePopupMenu();
    if (menu == nullptr) {
        return;
    }

    const bool hasSelection = Selected(state) != nullptr;
    const UINT itemFlags = hasSelection ? MF_STRING : (MF_STRING | MF_GRAYED);

    ::AppendMenuW(menu, itemFlags, kMenuEdit, Loc::Str(IDS_HISTORY_EDIT).c_str());
    ::AppendMenuW(menu, itemFlags, kMenuCopy, Loc::Str(IDS_HISTORY_COPY).c_str());
    ::AppendMenuW(menu, itemFlags, kMenuReveal,
                  Loc::Str(IDS_HISTORY_REVEAL).c_str());
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu, itemFlags, kMenuDelete,
                  Loc::Str(IDS_HISTORY_DELETE).c_str());
    ::AppendMenuW(menu, state.tiles.empty() ? (MF_STRING | MF_GRAYED) : MF_STRING,
                  kMenuClear, Loc::Str(IDS_HISTORY_CLEAR).c_str());

    ::SetForegroundWindow(window);
    const int chosen = ::TrackPopupMenuEx(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD |
                                                    TPM_NONOTIFY,
                                          screen.x, screen.y, window, nullptr);
    ::DestroyMenu(menu);

    switch (chosen) {
        case kMenuEdit:   EditSelected(window, state); break;
        case kMenuCopy:   CopySelected(window, state); break;
        case kMenuReveal: RevealSelected(state); break;
        case kMenuDelete: DeleteSelected(window, state); break;
        case kMenuClear:  ClearAll(window, state); break;
        default: break;
    }
}

}  // namespace history
}  // namespace crisp
