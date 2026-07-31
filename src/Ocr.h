// Ocr.h — Görüntüdeki metni çıkarma.
//
// Windows'un KENDİ OCR motoru kullanılır (Windows.Media.Ocr). Tesseract gibi
// bir kütüphane eklemek tek dosya .exe hedefini bozar ve onlarca megabayt dil
// verisi gerektirirdi; buradaki motor işletim sisteminin parçasıdır ve
// kullanıcının kurulu dil paketleriyle çalışır.
//
// C++/WinRT KULLANILMAZ: projede harici bağımlılık yok. Üretilmiş ABI
// başlıkları ve RoGetActivationFactory doğrudan çağrılır.
#pragma once

#include "Capture.h"
#include "OcrLayout.h"

#include <string>

namespace crisp {

// OCR bu sistemde kullanılabilir mi? Motor, kullanıcının dil profilinde OCR
// destekli bir dil yoksa oluşturulamaz — bu bir hata değil, yapılandırma
// durumudur ve kullanıcıya öyle söylenmelidir.
[[nodiscard]] bool IsOcrAvailable();

// Görüntüdeki metni çıkarır. Metin bulunamazsa true döner ve `text` boş kalır;
// yalnızca motor çalıştırılamadığında false döner.
[[nodiscard]] bool RecognizeText(const Image& image, std::wstring& text);

// Kelimeleri KUTULARIYLA birlikte çıkarır — etkileşimli metin seçimi için.
// Koordinatlar `image`'ın piksellerine göredir; çağıran ekran koordinatına
// çevirmek isterse yakalamanın kökenini ekler.
[[nodiscard]] bool RecognizeLayout(const Image& image, OcrLayout& layout);

}  // namespace crisp
