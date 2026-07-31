// AboutWindow.h — Uygulamaya özgü "Hakkında" penceresi.
//
// MessageBox DEĞİL. Bir ileti kutusu sistemin kutusudur: simgesi yanlış,
// düzeni sabit, teması bizim seçimimize değil kabuğa bağlı ve içine bir logo
// koymanın yolu yok. Bu pencere kendi zeminini, kendi tipografisini ve kendi
// temasını çizer.
#pragma once

#include <windows.h>

namespace crisp {

// Pencereyi açar ve KAPANANA KADAR DÖNMEZ; kendi mesaj döngüsünü işletir.
// Zaten açıksa mevcut pencere öne getirilir ve hemen döner.
void ShowAboutWindow(HINSTANCE instance);

}  // namespace crisp
