// SettingsInternal.h — Ayarlar penceresinin İÇ paylaşımı.
//
// SettingsWindow.cpp (denetim oluşturma + yerleşim + çizim) ile
// SettingsInput.cpp (mesajlar, okuma/yazma, klasör seçimi) arasında
// paylaşılır; dışarıya açık değildir.
#pragma once

#include "Settings.h"

#include <string>
#include <vector>

#include <windows.h>

namespace crisp {
namespace settings_ui {

// Tasarım ölçüleri (96 DPI mantıksal piksel).
inline constexpr int kWidth = 660;
inline constexpr int kHeight = 604;
inline constexpr int kPad = 22;
inline constexpr int kColumnGap = 26;
inline constexpr int kRow = 30;          // bir denetim satırının yüksekliği
inline constexpr int kGroupGap = 20;     // gruplar arası boşluk
inline constexpr int kLabelWidth = 96;   // sol sütundaki etiketlerin genişliği
inline constexpr int kButtonHeight = 32;

enum ControlId {
    kIdLanguage = 100,
    kIdTheme,
    kIdDelay,
    kIdMagnifier,
    kIdHighlight,
    kIdPrintScreen,
    kIdShutter,
    kIdAfterCopy,
    kIdAfterSave,
    kIdAfterPin,
    kIdAfterEditor,
    kIdNotify,
    kIdFolder,
    kIdBrowse,
    kIdFormat,
    kIdQuality,
    kIdHistoryLimit,
    kIdHistoryClear,
    kIdHotkeyRegion,
    kIdHotkeyWindow,
    kIdHotkeyFullScreen,
    kIdHotkeyDelayed,
    kIdReset,
    kIdOk,
    kIdCancel,
};

// Çizilecek grup başlığı; denetimler Windows'a, başlıklar bize ait.
struct GroupTitle {
    std::wstring text;
    RECT bounds{};
};

struct State {
    HINSTANCE instance = nullptr;
    Settings* target = nullptr;   // çağıranın ayarları; yalnızca onaylanınca yazılır
    Settings working;             // pencere üzerinde düzenlenen kopya

    std::vector<GroupTitle> groups;
    std::vector<std::wstring> languageCodes;   // birleşik kutudaki sıra
    // Biçim listesi ÇALIŞMA ZAMANINDA kurulur (kullanılamayan biçimler
    // atlanır), bu yüzden dizin→kod eşlemesi sabit olamaz.
    std::vector<std::wstring> formatCodes;

    unsigned dpi = 96;
    bool accepted = false;

    // Koyu temada denetim zeminleri WM_CTLCOLOR* ile boyanır; fırça her
    // boyamada yeniden oluşturulmasın diye bir kez üretilir.
    HBRUSH backgroundBrush = nullptr;
    HFONT font = nullptr;
    HFONT groupFont = nullptr;
};

[[nodiscard]] int Scale(int value, unsigned dpi) noexcept;

// Denetimleri oluşturur ve yerleştirir. Bir kez, WM_CREATE'te çağrılır.
void BuildControls(HWND window, State& state);

// Ayarlardaki değerleri denetimlere yazar.
void LoadIntoControls(HWND window, const State& state);

// Denetimlerdeki değerleri state.working'e okur.
void ReadFromControls(HWND window, State& state);

// --- Değer alışverişi (SettingsValues.cpp) ----------------------------------
[[nodiscard]] std::wstring GetText(HWND window, int id);
[[nodiscard]] bool PickFolder(HWND owner, std::wstring& folder);
void ResetToDefaults(HWND window, State& state);
void ClearHistory(HWND window, const State& state);

void Paint(HWND window, State& state);

}  // namespace settings_ui
}  // namespace crisp
