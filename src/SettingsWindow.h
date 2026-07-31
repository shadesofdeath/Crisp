// SettingsWindow.h — Tüm ayarların düzenlendiği pencere.
#pragma once

#include "Settings.h"

#include <windows.h>

namespace crisp {

// Pencereyi açar ve kapanana kadar döner.
//
// AYARLARI YERİNDE DEĞİŞTİRİR ve yalnızca kullanıcı onayladıysa: dönüş true
// ise `settings` yeni değerleri taşır ve çağıran bunları diske yazıp yeniden
// uygulamakla yükümlüdür. Pencerenin kendisi kaydetmez, çünkü kısayolların
// yeniden kaydedilmesi ve dilin yeniden yüklenmesi uygulama katmanının işidir.
[[nodiscard]] bool ShowSettingsWindow(HINSTANCE instance, Settings& settings);

}  // namespace crisp
