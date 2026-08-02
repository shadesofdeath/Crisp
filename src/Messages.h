// Messages.h — Uygulamaya özel pencere mesajları ve zamanlayıcı kimlikleri.
#pragma once

#include <windows.h>

namespace crisp {

// Tepsi simgesi geri bildirimi. WM_APP tabanlı olmalı: WM_USER aralığı pencere
// SINIFINA aittir ve alt sınıflandırılmış denetimlerle çakışabilir.
inline constexpr UINT WM_CRISP_TRAY = WM_APP + 1;

// Yükleme bitti; lParam sonucu taşıyan bir yükün adresi ve alan taraf onu siler.
//
// MESAJ, ÇÜNKÜ AĞ ARKA PLANDA. Sonucu doğrudan pencereye yazmak, arayüz
// nesnelerine başka bir iş parçacığından dokunmak olurdu.
inline constexpr UINT WM_CRISP_UPLOAD_DONE = WM_APP + 2;

// Gecikmeli yakalama sayacı.
inline constexpr UINT_PTR TIMER_DELAY = 1;

// Tepsi simgesinin tema (açık/koyu görev çubuğu) yoklaması.
inline constexpr UINT_PTR TIMER_THEME = 2;

// Geri sayım göstergesinin saniye vuruşu.
inline constexpr UINT_PTR TIMER_COUNTDOWN = 3;

// Global kısayol kimlikleri. RegisterHotKey aynı iş parçacığında benzersiz
// olmalarını şart koşar.
//
// YUVA NUMARASI KİMLİKTİR: kısayolun NE YAPTIĞI artık ayarlarda duruyor ve
// kimliğe gömülü değil. Eskiden HOTKEY_REGION hem "birinci kısayol" hem "bölge
// seç" demekti; eylemi değiştirmek imkânsızdı.
enum HotkeyId : int {
    HOTKEY_SLOT_FIRST = 1,
    HOTKEY_SLOT_LAST = 6,
    HOTKEY_PRINTSCREEN = 7,
};

}  // namespace crisp
