// Toast.h — Yakalama sonrası kısa bildirim.
//
// NEDEN KENDİ PENCEREMİZ: Windows'un kendi bildirimleri (WinRT toast) paket
// kimliği ister; paketsiz çalışan bir sürümde hiç görünmezler. Tepsi balonu
// (NIIF_INFO) ise "Odak yardımı" açıkken sessizce yutulur ve kullanıcı
// yakalamanın alındığını hiç öğrenemez. Küçük bir katmanlı pencere her iki
// durumda da görünür ve küçük resmi de gösterebilir.
//
// PENCEREYİ BEKLEMEZ: çağrı hemen döner, bildirim kendi zamanlayıcısıyla
// sönüp yok olur. Aksi hâlde her yakalamadan sonra uygulama üç saniye
// donardı.
#pragma once

#include "Capture.h"

#include <string>

#include <windows.h>

namespace crisp {

// Yeni bir bildirim açar; ekranda bir tane varsa onun yerini alır.
// `openPath` boş değilse bildirime tıklamak dosyayı açar.
void ShowCaptureToast(HINSTANCE instance, const Image& capture,
                      const std::wstring& title, const std::wstring& detail,
                      const std::wstring& openPath);

// Uygulama kapanırken; açık bildirim varsa kapatır.
void CloseCaptureToast() noexcept;

}  // namespace crisp
