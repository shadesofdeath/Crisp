// EditorWindow.h — İşaretleme penceresi.
//
// Yakalamayı alır, kullanıcı üzerine çizer, sonucu panoya/dosyaya verir.
// Pencere KENDİ MESAJ DÖNGÜSÜNÜ işletir ve kapanana kadar dönmez; çağıran
// sonucu doğrudan alır.
#pragma once

#include "Capture.h"
#include "Settings.h"

#include <string>

#include <windows.h>

namespace crisp {

// DÜZENLEYİCİ İŞİ KENDİSİ YAPAR. Eskiden "kopyala" yalnızca bir bayrak
// döndürüyor, pencere anında kapanıyor ve gerçek kopyalama çağıranda
// oluyordu — kullanıcının gördüğü tek şey pencerenin kaybolmasıydı ve
// kopyalanıp kopyalanmadığını anlamanın yolu yoktu. Artık pano ve dosya
// işlemleri pencere AÇIKKEN yapılır, onay durum çubuğunda görünür ve burada
// dönen şey NE OLDUĞUNUN kaydıdır.
struct EditorResult {
    bool accepted = false;    // bir çıktı üretildi mi (geçmişe yazılsın mı)
    bool copied = false;      // panoya kopyalandı
    std::wstring savedPath;   // kaydedildiyse dosya yolu; yoksa boş
};

// Düzenleyiciyi açar. `image` YERİNDE değiştirilir: dönüşte üzerine çizilmiş
// hâli taşır, böylece çağıran sonucu kaydedebilir.
[[nodiscard]] EditorResult RunEditor(HINSTANCE instance, const Settings& settings,
                                     Image& image);

}  // namespace crisp
