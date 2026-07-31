// Messages.h — Uygulamaya özel pencere mesajları ve zamanlayıcı kimlikleri.
#pragma once

#include <windows.h>

namespace crisp {

// Tepsi simgesi geri bildirimi. WM_APP tabanlı olmalı: WM_USER aralığı pencere
// SINIFINA aittir ve alt sınıflandırılmış denetimlerle çakışabilir.
inline constexpr UINT WM_CRISP_TRAY = WM_APP + 1;

// Gecikmeli yakalama sayacı.
inline constexpr UINT_PTR TIMER_DELAY = 1;

// Tepsi simgesinin tema (açık/koyu görev çubuğu) yoklaması.
inline constexpr UINT_PTR TIMER_THEME = 2;

// Global kısayol kimlikleri. RegisterHotKey aynı iş parçacığında benzersiz
// olmalarını şart koşar.
enum HotkeyId : int {
    HOTKEY_REGION = 1,
    HOTKEY_FULLSCREEN = 2,
    HOTKEY_WINDOW = 3,
    HOTKEY_DELAYED = 4,
    HOTKEY_PRINTSCREEN = 5,
};

}  // namespace crisp
