// ClipboardImage.h — Görüntüyü panoya koyma ve geri okuma.
//
// İKİ BİÇİM BİRDEN yazılır ve bu bilinçlidir:
//   CF_DIB   Word, Paint, eski Win32 uygulamaları bunu bekler.
//   "PNG"    Chrome, Discord, Slack, Figma ve modern web uygulamaları bunu
//            tercih eder; CF_DIB'i alanların bir kısmı alfayı yanlış yorumlayıp
//            görüntüyü siyah gösterir.
// Tek biçim yazmak, hangisini seçersek seçelim kullanıcıların yarısında
// bozuk yapıştırma demektir.
#pragma once

#include "Capture.h"

#include <windows.h>

namespace crisp {

// Panoya yazar. owner, pano sahibi olacak pencere; nullptr geçerlidir.
[[nodiscard]] bool CopyImageToClipboard(const Image& image, HWND owner);

// Panodaki görüntüyü okur. Önce PNG, yoksa CF_DIB denenir.
// Testler ve "panodan al" akışı için.
[[nodiscard]] bool ReadImageFromClipboard(Image& out, HWND owner);

// Panoda görüntü var mı?
[[nodiscard]] bool ClipboardHasImage() noexcept;

// Metni panoya yazar (OCR sonucu için).
[[nodiscard]] bool CopyTextToClipboard(const wchar_t* text, HWND owner);

// DOSYANIN KENDİSİNİ panoya koyar (CF_HDROP): Explorer'da ya da bir sohbet
// penceresinde yapıştırıldığında dosya kopyalanır, görüntü değil.
//
// GÖRÜNTÜDEN FARKLI BİR İŞ: bir e-postaya eklenti olarak sürüklemek isteyen
// kullanıcı CF_DIB'i değil dosyayı ister ve ikisi aynı anda panoda
// tutulamayacağı için bu ayrı bir yakalama sonrası görevdir.
[[nodiscard]] bool CopyFileToClipboard(const std::wstring& path, HWND owner);

}  // namespace crisp
