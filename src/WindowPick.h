// WindowPick.h — İmlecin altındaki pencereyi bulma.
//
// Kaplama tüm ekranı kapladığı için WindowFromPoint DAİMA kaplamanın kendisini
// döndürür; bu yüzden üst düzey pencereler Z sırasına göre elle taranır ve
// yok sayılacak pencere (kaplama) dışarıda bırakılır.
#pragma once

#include <string>

#include <windows.h>

namespace crisp {

// Ekran koordinatındaki noktanın altındaki en üstteki uygun pencere.
// ignore, taramadan çıkarılır (kaplamanın kendisi). Bulunamazsa nullptr.
[[nodiscard]] HWND WindowUnderPoint(POINT screenPoint, HWND ignore) noexcept;

// Pencerenin GÖRÜNEN çerçeve sınırı. GetWindowRect'ten farkı, Windows 10/11'in
// görünmez yeniden boyutlandırma kenarlığını içermemesidir; o kenarlık dahil
// edilirse vurgulama dikdörtgeni pencerenin birkaç piksel dışına taşar ve
// kullanıcı yanlış bir alanı seçtiğini sanır.
[[nodiscard]] bool WindowFrameBounds(HWND window, RECT& out) noexcept;

// Yakalanabilir mi: görünür, simge durumunda değil, DWM tarafından gizlenmemiş
// (cloaked) ve sıfır boyutlu değil.
//
// CLOAKED KONTROLÜ ŞART: mağaza uygulamaları kapatıldığında pencereleri yok
// edilmez, yalnızca "cloaked" işaretlenir. IsWindowVisible bunlara HÂLÂ true
// döner, dolayısıyla bu kontrol olmadan ekranda görünmeyen hayalet pencereler
// imlecin altında seçilebilir hâle gelir.
[[nodiscard]] bool IsCapturableWindow(HWND window) noexcept;

// "Aktif pencere" — kullanıcının Alt+Tab'la geçeceği pencere.
//
// GetForegroundWindow TEK BAŞINA YETMEZ ve bu bir hata olarak öğrenildi:
// tepsi menüsünün açılabilmesi için TrackPopupMenuEx'ten önce
// SetForegroundWindow(kendi penceremiz) çağrılmak ZORUNDADIR (belgelenmiş bir
// kabuk gerekliliği). Menü kapandığında ön plan hâlâ Crisp'in görünmez mesaj
// penceresidir, dolayısıyla tepsiden çağrılan "aktif pencere" her seferinde
// kendimizi bulup vazgeçiyordu.
//
// Bu yüzden ön plan uygun değilse Z SIRASI taranır: kullanıcının penceresi
// hâlâ tepede durur, yalnızca odakta değildir.
[[nodiscard]] HWND ForegroundCapturableWindow() noexcept;

// Pencerenin başlığı; dosya adı şablonundaki %pn için.
[[nodiscard]] std::wstring WindowTitleText(HWND window);

}  // namespace crisp
