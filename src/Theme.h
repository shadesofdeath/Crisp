// Theme.h — Açık/koyu tema.
//
// İKİ AYRI İŞ YAPAR ve ayrımı bilmek önemlidir:
//
//   1. SİSTEM ÇİZİMLERİ (bağlam menüleri, iletişim kutuları, başlık çubukları).
//      Bunları biz çizmiyoruz; Windows çiziyor. Koyu görünmeleri için uxtheme'in
//      BELGELENMEMİŞ ordinal'leri çağrılır. Owner-draw menü yazmak alternatifti;
//      o yol kısayol sütunu hizasını, onay işaretlerini, klavye gezinmesini ve
//      yüksek kontrast desteğini elle yeniden yazmak demekti.
//
//   2. KENDİ ÇİZİMLERİMİZ (kaplama panelleri, iğne kenarlığı). Bunlar için
//      Palette'ten renk alınır.
//
// Ordinal'ler ÇALIŞMA ZAMANINDA GetProcAddress ile çözülür ve çözülemezse
// sessizce hiçbir şey yapılmaz. Statik olarak bağlansaydı, ordinal'in
// kaldırıldığı bir Windows sürümünde uygulama hiç başlamazdı.
#pragma once

#include <windows.h>

namespace crisp {

enum class ThemeMode {
    System,   // Windows'un ayarını takip et
    Light,
    Dark,
};

// Kendi çizdiğimiz yüzeylerin renkleri.
struct Palette {
    COLORREF surface;      // panel zemini
    COLORREF surfaceAlt;   // ikincil zemin (üstbilgi, seçili satır)
    COLORREF border;
    COLORREF text;
    COLORREF textDim;
    COLORREF accent;
};

namespace theme {

// Uygulama başlarken bir kez. Ordinal'leri çözer ve kipi uygular.
void Initialize(ThemeMode mode);

// Ayar değiştiğinde. Sistem çizimleri anında yenilenir.
void SetMode(ThemeMode mode);

// WM_SETTINGCHANGE / "ImmersiveColorSet" geldiğinde çağrılır.
// Kip System değilse hiçbir şey yapmaz.
// Dönüş: etkin tema DEĞİŞTİ mi — çağıran yalnızca değiştiyse yeniden çizsin.
[[nodiscard]] bool RefreshFromSystem();

[[nodiscard]] bool IsDark() noexcept;
[[nodiscard]] const Palette& Colors() noexcept;

// Pencerenin başlık çubuğunu ve alt denetimlerini temaya uydurur.
// Başlıksız pencerelerde de zararsızdır.
void ApplyToWindow(HWND window);

// Ayar dizesi ⇄ kip. Bilinmeyen dize System'e düşer.
[[nodiscard]] ThemeMode ModeFromString(const wchar_t* value) noexcept;
[[nodiscard]] const wchar_t* ModeToString(ThemeMode mode) noexcept;

}  // namespace theme
}  // namespace crisp
