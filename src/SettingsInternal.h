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
//
// ÜÇ SÜTUN: ayar sayısı iki sütuna sığmıyordu ve pencereyi uzatmak, alt
// şeridin ekran dışına düşmesi demekti. Üçüncü sütun kısayollara ve yakalama
// sonrası görevlere ayrıldı; ikisi de listeye dönüşen ve büyüyen gruplar.
inline constexpr int kWidth = 980;
inline constexpr int kHeight = 660;
inline constexpr int kPad = 22;
inline constexpr int kColumnGap = 24;
inline constexpr int kRow = 30;          // bir denetim satırının yüksekliği
inline constexpr int kGroupGap = 18;     // gruplar arası boşluk
inline constexpr int kLabelWidth = 140;  // etiket sütununun genişliği
// 140 DENEYEREK BULUNDU: 104'te "Bulanıklık (%)" gibi orta uzunlukta bir
// etiket kutunun altına giriyordu ve kırpılmış metin, ayarın ne olduğunu
// söylemiyordu.
inline constexpr int kButtonHeight = 32;
inline constexpr int kActionWidth = 150; // kısayol eyleminin açılır kutusu

enum ControlId {
    kIdLanguage = 100,
    kIdTheme,
    kIdDelay,
    kIdMagnifier,
    kIdHighlight,
    kIdPrintScreen,
    kIdShutter,
    kIdShellMenu,
    kIdIncludeCursor,
    kIdDimStrength,
    kIdAfterCopy,
    kIdAfterSave,
    kIdAfterPin,
    kIdAfterEditor,
    kIdAfterCopyPath,
    kIdAfterCopyFile,
    kIdAfterReveal,
    kIdAfterOcr,
    kIdNotify,
    kIdFolder,
    kIdBrowse,
    kIdFormat,
    kIdQuality,
    kIdNameFormat,
    kIdSubFolder,
    kIdBlurStrength,
    kIdMosaicStrength,
    kIdHistoryLimit,
    kIdHistoryClear,
    kIdUploadService,
    kIdUploadKey,
    kIdAfterUpload,
    kIdReset,
    kIdOk,
    kIdCancel,

    // Kısayol satırları BLOK HÂLİNDE: yuva sayısı değiştiğinde tek tek kimlik
    // eklemek yerine taban + indeks kullanılır.
    kIdHotkeyActionFirst = 200,
    kIdHotkeyKeyFirst = 220,
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
    // Yükleme servisi birleşik kutusundaki sıra → servis kimliği. Enum sırasına
    // güvenilmez: liste "yok" girdisiyle başlıyor ve ileride bir servis
    // listeden çıkarılabilir.
    std::vector<std::wstring> uploadServiceIds;

    unsigned dpi = 96;
    bool accepted = false;

    // Koyu temada denetim zeminleri WM_CTLCOLOR* ile boyanır; fırça her
    // boyamada yeniden oluşturulmasın diye bir kez üretilir.
    HBRUSH backgroundBrush = nullptr;
    HFONT font = nullptr;
    HFONT groupFont = nullptr;
};

[[nodiscard]] int Scale(int value, unsigned dpi) noexcept;

// Kısayol eyleminin görünen adı. Ayarlar penceresi ve tepsi menüsü aynı
// listeyi kullanır; iki ayrı kopya, birine eylem eklendiğinde diğerinin
// eksik kalması demekti.
[[nodiscard]] UINT HotkeyActionLabel(HotkeyAction action) noexcept;

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

// Anahtar alanını, seçili servis anahtar istiyorsa açar, istemiyorsa kapatır.
//
// KAPALI BİR ALAN, DOLDURULMASI GEREKMEDİĞİNİ SÖYLER. Catbox seçiliyken açık
// duran bir "API anahtarı" kutusu, kullanıcıya bulması gereken bir şey olduğunu
// düşündürür.
void UpdateUploadKeyState(HWND window, const State& state);
void ClearHistory(HWND window, const State& state);

void Paint(HWND window, State& state);

}  // namespace settings_ui
}  // namespace crisp
