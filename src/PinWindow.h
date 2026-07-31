// PinWindow.h — Yakalamayı ekranda üstte yüzen bir pencerede sabitler.
//
// Bu, aracın diğerlerinden ayrıldığı yer: iki şeyi yan yana karşılaştırmak için
// ikinci bir monitöre ya da bir düzenleyiciye gerek kalmıyor.
//
// Pencere KENDİ ÖMRÜNÜ YÖNETİR. Çağıran bir tutamaç almaz ve serbest bırakma
// sorumluluğu taşımaz; kullanıcı kapattığında pencere kendini yok eder. Aksi
// hâlde App, kaç iğne açıldığını ve hangisinin kapandığını izlemek zorunda
// kalırdı — iğnelerin App ile hiçbir alışverişi yok.
#pragma once

#include "Capture.h"

#include <windows.h>

namespace crisp {

// Görüntüyü iğneler. topLeft ekran koordinatıdır; pencere oraya, ekran
// sınırlarının içinde kalacak biçimde yerleştirilir.
// Görüntü KOPYALANIR: çağıranın Image'ı hemen yok edilebilir.
[[nodiscard]] bool PinImageToScreen(HINSTANCE instance, const Image& image,
                                    POINT topLeft);

// Açık tüm iğneleri kapatır (uygulama çıkışında).
void CloseAllPins() noexcept;

}  // namespace crisp
