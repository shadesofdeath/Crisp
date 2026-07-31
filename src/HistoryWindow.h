// HistoryWindow.h — Son yakalamaların küçük resimlerini gösteren pencere.
#pragma once

#include "Capture.h"
#include "History.h"

#include <string>

#include <windows.h>

namespace crisp {

enum class HistoryChoice {
    None,   // kullanıcı pencereyi kapattı
    Edit,   // seçili yakalamayı düzenleyicide aç
};

struct HistoryResult {
    HistoryChoice choice = HistoryChoice::None;
    Image image;          // Edit ise dolu
    std::wstring path;
};

// Pencereyi açar ve kapanana kadar döner.
//
// KOPYALAMA, SİLME VE KLASÖRDE GÖSTERME PENCERENİN KENDİ İŞİDİR: bunlar
// pencereyi kapatmayı gerektirmez ve çağırana taşınsalardı, her biri için
// pencerenin yeniden açılıp kapanması gerekirdi. Yalnızca DÜZENLEME dışarı
// taşınır, çünkü düzenleyici bu pencerenin bilmediği bir ayar kümesiyle
// (kayıt biçimi, yakalama sonrası eylemler) çalışır.
[[nodiscard]] HistoryResult ShowHistoryWindow(HINSTANCE instance,
                                              HistoryStore& store);

}  // namespace crisp
