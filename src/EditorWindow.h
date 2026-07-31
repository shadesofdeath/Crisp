// EditorWindow.h — İşaretleme penceresi.
//
// Yakalamayı alır, kullanıcı üzerine çizer, sonucu panoya/dosyaya verir.
// Pencere KENDİ MESAJ DÖNGÜSÜNÜ işletir ve kapanana kadar dönmez; çağıran
// sonucu doğrudan alır.
#pragma once

#include "Capture.h"
#include "Settings.h"

#include <windows.h>

namespace crisp {

struct EditorResult {
    bool accepted = false;   // kullanıcı kopyala/kaydet dedi mi
    bool copyToClipboard = false;
    bool saveToFile = false;
};

// Düzenleyiciyi açar. `image` YERİNDE değiştirilir: dönüşte üzerine çizilmiş
// hâli taşır, böylece çağıran sonucu kaydedebilir.
[[nodiscard]] EditorResult RunEditor(HINSTANCE instance, const Settings& settings,
                                     Image& image);

}  // namespace crisp
