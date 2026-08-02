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

// Süren bir iş için bildirim: SÖNMEZ, yerini bir sonraki bildirim alana kadar
// durur, ve ayrıntı satırının sonunda geçen saniyeyi sayar.
//
// SÖNMEMESİ ASIL ÖZELLİĞİ. Yükleme servise göre yirmi saniye sürebiliyor ve
// üç saniyede sönen bir "yükleniyor" bildirimi, kalan on yedi saniye boyunca
// kullanıcıya hiçbir şey söylemiyor: işin sürdüğü ile unutulduğu aynı görünür.
//
// Yine de sonsuz değil: çağıran taraf sonucu bildirmeden ölürse bildirim
// `kStuckMs` sonunda kendiliğinden kapanır. Ekranda sonsuza dek duran bir
// kutu, hiç göstermemekten kötüdür.
void ShowProgressToast(HINSTANCE instance, const Image& capture,
                       const std::wstring& title, const std::wstring& detail);

// Uygulama kapanırken; açık bildirim varsa kapatır.
void CloseCaptureToast() noexcept;

}  // namespace crisp
