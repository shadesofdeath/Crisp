// OcrInternal.h — OCR motorunun WinRT tarafıyla konuşan iç köprü.
//
// AYRI BAŞLIK: metin çıkarma (Ocr.cpp) ve kelime yerleşimi çıkarma
// (OcrScan.cpp) ayrı dosyalarda — birlikte Ocr.cpp ev kuralının 400 satır
// sınırını aşıyordu (docs §9) — ama ikisi de aynı tanımayı çalıştırır.
//
// BU BAŞLIK WinRT ABI'sini TAŞIR ve bu yüzden yalnızca bu iki dosyadan
// dahil edilir. Ocr.h temiz kalır: OCR'ı kullanan kodun WinRT görmesi
// gerekmez.
#pragma once

#include "Capture.h"
#include "Util.h"


#include "Ocr.h"

#include "ImageCodec.h"
#include "Util.h"

#include <roapi.h>
#include <winstring.h>

#include <windows.foundation.h>
#include <windows.globalization.h>
#include <windows.graphics.imaging.h>
#include <windows.media.ocr.h>
#include <windows.storage.streams.h>

#include <vector>

namespace crisp {

// WinRT'nin OcrWord'ü İÇERİ ALINMAZ: bu ad alanında projenin kendi
// crisp::OcrWord yapısı var ve ikisi birden görünür olsaydı her kullanım
// belirsiz kalırdı. WinRT tarafı takma adla anılır.
namespace WinOcr = ABI::Windows::Media::Ocr;

using ABI::Windows::Foundation::Collections::IVectorView;
using ABI::Windows::Foundation::Rect;
using ABI::Windows::Media::Ocr::IOcrLine;
using ABI::Windows::Media::Ocr::IOcrResult;
using ABI::Windows::Media::Ocr::IOcrWord;
using ABI::Windows::Media::Ocr::OcrLine;

// Sahiplenilen HSTRING (çıkış parametreleri için).
class HStringOwner {
public:
    HStringOwner() noexcept = default;
    HStringOwner(const HStringOwner&) = delete;
    HStringOwner& operator=(const HStringOwner&) = delete;
    ~HStringOwner() {
        if (m_value != nullptr) {
            ::WindowsDeleteString(m_value);
        }
    }

    [[nodiscard]] HSTRING* put() noexcept { return &m_value; }

    [[nodiscard]] std::wstring str() const {
        if (m_value == nullptr) {
            return std::wstring();
        }
        UINT32 length = 0;
        const wchar_t* raw = ::WindowsGetStringRawBuffer(m_value, &length);
        return raw == nullptr ? std::wstring() : std::wstring(raw, length);
    }

private:
    HSTRING m_value = nullptr;
};

// Görüntüyü tanır. Motor yoksa ya da dil paketi kurulu değilse false.
[[nodiscard]] bool RunRecognition(
    const Image& image,
    ComPtr<ABI::Windows::Media::Ocr::IOcrResult>& out);

}  // namespace crisp
